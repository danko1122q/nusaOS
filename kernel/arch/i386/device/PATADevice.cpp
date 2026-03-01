/*
    This file is part of nusaOS.
    
    nusaOS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    nusaOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.
    
    Copyright (c) Byteduck 2016-2020. All rights reserved.
*/

#include "PATADevice.h"
#include "kernel/IO.h"
#include "kernel/kstd/KLog.h"
#include "kernel/filesystem/FileDescriptor.h"

PATADevice* PATADevice::find(PATADevice::Channel channel, PATADevice::DriveType drive, bool use_pio) {
    PCI::Address addr = {0,0,0};
    PCI::enumerate_devices([](PCI::Address addr, PCI::ID id, uint16_t type, void* data) {
        if(type == PCI_TYPE_IDE_CONTROLLER)
            *((PCI::Address*)data) = addr;
    }, &addr);
    if(addr.is_zero())
        return nullptr;
    return new PATADevice(addr, channel, drive, use_pio);
}

PATADevice::PATADevice(PCI::Address addr, PATADevice::Channel channel, PATADevice::DriveType drive, bool use_pio)
    : IRQHandler()
    , DiskDevice(channel == PRIMARY ? 3 : 4, drive == MASTER ? 0 : 1)
    , _pci_addr(addr), _channel(channel), _drive(drive), _use_pio(use_pio)
{
    _io_base      = channel == PRIMARY ? 0x1F0 : 0x170;
    _control_base = channel == PRIMARY ? 0x3F6 : 0x376;
    _bus_master_base = PCI::read_word(addr, PCI_BAR4) & (~1);

    // Detect bus mastering capability
    if(PCI::read_byte(addr, PCI_PROG_IF) & 0x80u) {
        KLog::dbg("PATA", "IDE controller capable of bus mastering");
    } else {
        KLog::dbg("PATA", "IDE controller not capable of bus mastering, using PIO");
        _use_pio = true;
        use_pio  = true;
    }

    // IRQ: channel 0 = IRQ14, channel 1 = IRQ15
    set_irq(channel == PRIMARY ? 14 : 15);

    // Select drive
    IO::outb(_io_base + ATA_DRIVESEL, 0xA0u | (drive == SLAVE ? 0x10u : 0x00u));
    IO::outb(_io_base + ATA_SECCNT0, 0);
    IO::outb(_io_base + ATA_LBA0, 0);
    IO::outb(_io_base + ATA_LBA1, 0);
    IO::outb(_io_base + ATA_LBA2, 0);

    // IDENTIFY
    IO::outb(_io_base + ATA_COMMAND, ATA_IDENTIFY);
    uint8_t status = wait_status();
    if(!status)
        return; // No drive found

    uint16_t identity[256];
    uint16_t identity_reversed[256];
    for(int i = 0; i < 256; i++) {
        uint16_t val = IO::inw(_io_base);
        identity[i]          = val;
        identity_reversed[i] = ((val & 0xFFu) << 8u) | ((val & 0xFF00u) >> 8u);
    }

    // Model number: strip trailing spaces
    auto* identity_model_number = (uint8_t*)&identity_reversed[ATA_IDENTITY_MODEL_NUMBER_START];
    for(int i = ATA_IDENTITY_MODEL_NUMBER_LENGTH - 1; i >= 0; i--) {
        if(identity_model_number[i] != ' ') break;
        identity_model_number[i] = '\0';
    }
    memcpy(_model_number, identity_model_number, ATA_IDENTITY_MODEL_NUMBER_LENGTH);

    auto* identity_block = (Identity*)identity;

    // DMA capability
    if(!identity_block->capabilities.dma_supported) {
        _use_pio = true;
        use_pio  = true;
    }

    // LBA48 support
    // commands_supported is uint16_t[3]; LBA48 bit is word[1] bit 10
    // (ATA spec: combined bit 26 = word index 1, local bit 10)
    _supports_lba48 = (identity_block->commands_supported[1] & (1u << 10)) != 0;

    // Disk size: prefer LBA48 total if available and larger
    _max_addressable_block = identity_block->user_addressable_sectors;
    if(_supports_lba48) {
        uint64_t lba48_sectors =
            (uint64_t)identity[100] |
            ((uint64_t)identity[101] << 16) |
            ((uint64_t)identity[102] << 32) |
            ((uint64_t)identity[103] << 48);
        if(lba48_sectors > _max_addressable_block)
            _max_addressable_block = lba48_sectors;
    }

    PCI::enable_interrupt(addr);
    if(!use_pio) {
        PCI::enable_bus_mastering(addr);
        _prdt_region = MM.alloc_kernel_region(sizeof(PRDT));
        _prdt        = (PRDT*)_prdt_region->start();
        _dma_region  = MM.alloc_dma_region(ATA_MAX_SECTORS_AT_ONCE * 512);

        // Reset bus master status register
        IO::outb(_bus_master_base + ATA_BM_STATUS,
                 IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x4u);
    }

    KLog::info("PATA", "Setup disk {} using {} ({} blocks, LBA{})",
               _model_number,
               _use_pio ? "PIO" : "DMA",
               _max_addressable_block,
               _supports_lba48 ? "48" : "28");
}

PATADevice::~PATADevice() = default;

uint8_t PATADevice::wait_status(uint8_t flags) {
    uint8_t ret = 0;
    while((ret = IO::inb(_control_base)) & flags);
    return ret;
}

void PATADevice::wait_ready() {
    uint8_t status = IO::inb(_control_base);
    while((status & ATA_STATUS_BSY) || !(status & ATA_STATUS_RDY))
        status = IO::inb(_control_base);
}

Result PATADevice::read_sectors_dma(uint32_t lba, uint8_t num_sectors, uint8_t* buf) {
    ASSERT(num_sectors <= ATA_MAX_SECTORS_AT_ONCE);
    LOCK(_lock);

    if(num_sectors * 512 > _dma_region->size())
        return Result(-EINVAL);
    _prdt->addr = _dma_region->object()->physical_page(0).paddr();
    _prdt->size = num_sectors * 512;
    _prdt->eot  = 0x8000;

    IO::outb(_io_base + ATA_DRIVESEL, 0xA0u | (_drive == SLAVE ? 0x8u : 0x0u));
    IO::wait(10);

    IO::outb(_bus_master_base, 0);
    IO::outl(_bus_master_base + ATA_BM_PRDT, _prdt_region->object()->physical_page(0).paddr());
    IO::outb(_bus_master_base, ATA_BM_READ);
    IO::outb(_bus_master_base + ATA_BM_STATUS,
             IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x6u);

    access_drive(ATA_READ_DMA, lba, num_sectors);

    while(!(IO::inb(_control_base) & ATA_STATUS_DRQ));
    IO::outb(_bus_master_base, 0x9);

    TaskManager::current_thread()->block(_blocker);
    _blocker.set_ready(false);
    uninstall_irq();

    if(_post_irq_status & ATA_STATUS_ERR) {
        KLog::err("PATA", "DMA read fail: status={#x} bm_status={#x}",
                  _post_irq_status, _post_irq_bm_status);
        return Result(-EIO);
    }

    memcpy((void*)buf, (void*)_dma_region->start(), 512 * num_sectors);

    IO::outb(_bus_master_base + ATA_BM_STATUS,
             IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x6u);

    return Result(SUCCESS);
}

Result PATADevice::write_sectors_dma(uint32_t lba, uint8_t num_sectors, const uint8_t* buf) {
    ASSERT(num_sectors <= ATA_MAX_SECTORS_AT_ONCE);
    LOCK(_lock);

    if(num_sectors * 512 > _dma_region->size())
        return Result(-EINVAL);
    _prdt->addr = _dma_region->object()->physical_page(0).paddr();
    _prdt->size = num_sectors * 512;
    _prdt->eot  = 0x8000;

    memcpy((void*)_dma_region->start(), buf, 512 * num_sectors);

    IO::outb(_io_base + ATA_DRIVESEL, 0xA0u | (_drive == SLAVE ? 0x8u : 0x0u));
    IO::wait(10);

    IO::outb(_bus_master_base, 0);
    IO::outl(_bus_master_base + ATA_BM_PRDT, _prdt_region->object()->physical_page(0).paddr());
    IO::outb(_bus_master_base + ATA_BM_STATUS,
             IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x6u);

    access_drive(ATA_WRITE_DMA, lba, num_sectors);

    while(IO::inb(_control_base) & ATA_STATUS_BSY || !(IO::inb(_control_base) & ATA_STATUS_DRQ));
    IO::outb(_bus_master_base, 0x1);

    TaskManager::current_thread()->block(_blocker);
    _blocker.set_ready(false);
    uninstall_irq();

    if(_post_irq_status & ATA_STATUS_ERR) {
        KLog::err("PATA", "DMA write fail: status={#x} bm_status={#x}",
                  _post_irq_status, _post_irq_bm_status);
        return Result(-EIO);
    }

    IO::outb(_bus_master_base + ATA_BM_STATUS,
             IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x6u);

    return Result(SUCCESS);
}

void PATADevice::write_sectors_pio(uint32_t sector, uint8_t sectors, const uint8_t* buffer) {
    LOCK(_lock);

    _blocker.set_ready(false);
    access_drive(ATA_WRITE_PIO, sector, sectors);
    for(int j = 0; j < sectors; j++) {
        TaskManager::current_thread()->block(_blocker);
        _blocker.set_ready(false);
        IO::wait(10);
        while(IO::inb(_io_base + ATA_STATUS) & ATA_STATUS_BSY ||
              !(IO::inb(_io_base + ATA_STATUS) & ATA_STATUS_DRQ));
        for(int i = 0; i < 256; i++)
            IO::outw(_io_base + ATA_DATA, buffer[i*2] | (buffer[i*2+1] << 8u));
        buffer += 512;
    }
    uninstall_irq();
}

void PATADevice::read_sectors_pio(uint32_t sector, uint8_t sectors, uint8_t* buffer) {
    LOCK(_lock);

    _blocker.set_ready(false);
    access_drive(ATA_READ_PIO, sector, sectors);
    for(int j = 0; j < sectors; j++) {
        TaskManager::current_thread()->block(_blocker);
        _blocker.set_ready(false);
        for(int i = 0; i < 256; i++) {
            uint16_t tmp = IO::inw(_io_base + ATA_DATA);
            buffer[i*2]   = (uint8_t)tmp;
            buffer[i*2+1] = (uint8_t)(tmp >> 8u);
        }
        buffer += 512;
    }
    uninstall_irq();
}

void PATADevice::access_drive(uint8_t command, uint32_t lba, uint8_t num_sectors) {
    TaskManager::enter_critical();
    wait_ready();

    if(_supports_lba48) {
        // LBA48: send high bytes first, then low bytes
        IO::outb(_io_base + ATA_DRIVESEL,
                 0x40u | (_drive == SLAVE ? 0x10u : 0x00u)); // LBA bit, no high nibble

        // High bytes
        IO::outb(_io_base + ATA_SECCNT0, 0);                          // sector count high
        IO::outb(_io_base + ATA_LBA0,    (lba >> 24) & 0xFFu);        // LBA 24-31
        IO::outb(_io_base + ATA_LBA1,    0);                           // LBA 32-39 (0 for 32-bit LBA)
        IO::outb(_io_base + ATA_LBA2,    0);                           // LBA 40-47

        // Low bytes
        IO::outb(_io_base + ATA_SECCNT0, num_sectors);
        IO::outb(_io_base + ATA_LBA0,    lba & 0xFFu);
        IO::outb(_io_base + ATA_LBA1,    (lba >> 8)  & 0xFFu);
        IO::outb(_io_base + ATA_LBA2,    (lba >> 16) & 0xFFu);
    } else {
        // LBA28
        IO::outb(_io_base + ATA_DRIVESEL,
                 0xE0u | (_drive == SLAVE ? 0x10u : 0x00u) | ((lba >> 24) & 0x0Fu));
        IO::wait(20);

        IO::outb(_io_base + ATA_SECCNT0, num_sectors);
        IO::outb(_io_base + ATA_LBA0,    lba         & 0xFFu);
        IO::outb(_io_base + ATA_LBA1,    (lba >> 8)  & 0xFFu);
        IO::outb(_io_base + ATA_LBA2,    (lba >> 16) & 0xFFu);
    }

    wait_ready();

    reinstall_irq();
    TaskManager::leave_critical();
    IO::outb(_io_base + ATA_COMMAND, command);
}

Result PATADevice::read_uncached_blocks(uint32_t block, uint32_t count, uint8_t* buffer) {
    if(!_use_pio) {
        size_t num_chunks = (count + ATA_MAX_SECTORS_AT_ONCE - 1) / ATA_MAX_SECTORS_AT_ONCE;
        for(size_t i = 0; i < num_chunks; i++) {
            uint32_t num_sectors = min((size_t)ATA_MAX_SECTORS_AT_ONCE, (size_t)count);
            Result res = read_sectors_dma(block + i * ATA_MAX_SECTORS_AT_ONCE, num_sectors,
                                          buffer + 512 * i * ATA_MAX_SECTORS_AT_ONCE);
            if(res.is_error()) return res;
            count -= num_sectors;
        }
    } else {
        while(count) {
            uint8_t to_read = (uint8_t)min((uint32_t)count, (uint32_t)0xFFu);
            read_sectors_pio(block, to_read, buffer);
            block  += to_read;
            buffer += to_read * 512;
            count  -= to_read;
        }
    }
    return Result(SUCCESS);
}

Result PATADevice::write_uncached_blocks(uint32_t block, uint32_t count, const uint8_t* buffer) {
    if(!_use_pio) {
        size_t num_chunks = (count + ATA_MAX_SECTORS_AT_ONCE - 1) / ATA_MAX_SECTORS_AT_ONCE;
        for(size_t i = 0; i < num_chunks; i++) {
            uint32_t num_sectors = min((size_t)ATA_MAX_SECTORS_AT_ONCE, (size_t)count);
            Result res = write_sectors_dma(block + i * ATA_MAX_SECTORS_AT_ONCE, num_sectors,
                                           buffer + 512 * i * ATA_MAX_SECTORS_AT_ONCE);
            if(res.is_error()) return res;
            count -= num_sectors;
        }
    } else {
        while(count) {
            uint8_t to_write = (uint8_t)min((uint32_t)count, (uint32_t)0xFFu);
            write_sectors_pio(block, to_write, buffer);
            block  += to_write;
            buffer += to_write * 512;
            count  -= to_write;
        }
    }
    return Result(SUCCESS);
}

size_t PATADevice::block_size() {
    return 512;
}

ssize_t PATADevice::read(FileDescriptor& fd, size_t offset, SafePointer<uint8_t> buffer, size_t count) {
    size_t first_block       = offset / block_size();
    size_t first_block_start = offset % block_size();
    size_t bytes_left        = count;
    size_t blk               = first_block;
    ssize_t nread            = 0;

    uint8_t block_buf[block_size()];
    while(bytes_left) {
        if(blk > _max_addressable_block) break;

        Result res = read_block(blk, block_buf);
        if(res.is_error()) return res.code();

        if(blk == first_block) {
            size_t avail   = block_size() - first_block_start;
            size_t to_copy = min(bytes_left, avail);
            buffer.write(block_buf + first_block_start, to_copy);
            nread     += to_copy;
            bytes_left -= to_copy;
        } else {
            size_t to_copy = min(bytes_left, block_size());
            buffer.write(block_buf, count - bytes_left, to_copy);
            nread     += to_copy;
            bytes_left -= to_copy;
        }
        blk++;
    }
    return nread;
}

ssize_t PATADevice::write(FileDescriptor& fd, size_t offset, SafePointer<uint8_t> buffer, size_t count) {
    size_t first_block       = offset / block_size();
    size_t last_block        = (offset + count - 1) / block_size();
    size_t first_block_start = offset % block_size();
    size_t bytes_left        = count;
    size_t blk               = first_block;

    if(last_block > _max_addressable_block)
        return -ENOSPC;

    uint8_t block_buf[block_size()];
    while(bytes_left) {
        Result res = read_block(blk, block_buf);
        if(res.is_error()) return res.code();

        if(blk == first_block) {
            size_t avail   = block_size() - first_block_start;
            size_t to_copy = min(bytes_left, avail);
            buffer.read(block_buf + first_block_start, to_copy);
            bytes_left -= to_copy;
        } else {
            size_t to_copy = min(bytes_left, block_size());
            buffer.read(block_buf, count - bytes_left, to_copy);
            bytes_left -= to_copy;
        }

        res = write_block(blk, block_buf);
        if(res.is_error()) return res.code();
        blk++;
    }
    return count;
}

void PATADevice::handle_irq(IRQRegisters* regs) {
    _post_irq_status    = IO::inb(_io_base + ATA_STATUS);
    _post_irq_bm_status = IO::inb(_bus_master_base + ATA_BM_STATUS);

    // Only handle IRQs meant for this device (DMA active bit)
    if(!(_post_irq_bm_status & 0x4u))
        return;

    // Acknowledge interrupt
    IO::outb(_bus_master_base + ATA_BM_STATUS,
             IO::inb(_bus_master_base + ATA_BM_STATUS) | 0x4u);

    _blocker.set_ready(true);
    TaskManager::yield_if_idle();
}