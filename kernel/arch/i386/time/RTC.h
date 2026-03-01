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

#pragma once

#include "kernel/kstd/unix_types.h"
#include "kernel/interrupt/IRQHandler.h"
#include "kernel/kstd/kstddef.h"
#include "kernel/time/TimeKeeper.h"

#define RTC_IRQ 0x8

// ─── Register CMOS ────────────────────────────────────────────────────────────
#define CMOS_SECONDS   0x00
#define CMOS_MINUTES   0x02
#define CMOS_HOURS     0x04
#define CMOS_WEEKDAY   0x06
#define CMOS_DAY       0x07
#define CMOS_MONTH     0x08
#define CMOS_YEAR      0x09
// Register century: 0x32 di ACPI FADT (kebanyakan BIOS modern dan QEMU).
// Jika BIOS tidak mendukung register ini, akan return 0x00 — ditangani
// oleh Y2K heuristic di timestamp() sehingga tahun tetap benar.
#define CMOS_CENTURY   0x32
#define CMOS_STATUS_A  0x0A
#define CMOS_STATUS_B  0x0B
#define CMOS_STATUS_C  0x0C

#define CMOS_STATUS_UPDATE_IN_PROGRESS  0x80
#define CMOS_STATUS_BINARY_MODE         0x04  // bit 2 STATUS_B: 1=binary, 0=BCD
#define CMOS_SQUARE_WAVE_INTERRUPT_FLAG 0x40

#define RTC_FREQUENCY      1024
#define RTC_FREQUENCYVAL_MIN 2
#define RTC_FREQUENCYVAL_MAX 14
#define RTC_FREQUENCY_DIVIDER 32768

// BCD → binary: (high nibble * 10) + low nibble
// Gunakan fungsi inline, bukan macro, agar type-safe dan tidak ada
// double-evaluation jika argumen punya side effects.
static inline int bcd_to_bin(uint8_t val) {
    return ((val >> 4) & 0x0F) * 10 + (val & 0x0F);
}

class RTC: public IRQHandler, public TimeKeeper {
public:
    ///RTC
    RTC(TimeManager* time);
    static time_t timestamp();

    ///IRQHandler
    void handle_irq(IRQRegisters* regs) override;
    bool mark_in_irq() override;

    ///TimeHandler
    int frequency() override;
    void enable() override;
    void disable() override;

private:
    bool set_frequency(int frequency);

    int _timestamp = 0;
    int _frequency = 0;
};