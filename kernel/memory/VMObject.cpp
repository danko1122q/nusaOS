/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */

#include "VMObject.h"
#include "MemoryManager.h"
#include <kernel/kstd/KLog.h>

VMObject::VMObject(kstd::string name, kstd::vector<PageIndex> physical_pages, bool all_cow):
        m_name(kstd::move(name)),
        m_physical_pages(kstd::move(physical_pages)),
        m_cow_pages(m_physical_pages.size()),
        m_size(m_physical_pages.size() * PAGE_SIZE)
{
        if(all_cow) {
                for(size_t i = 0; i < m_physical_pages.size(); i++) {
                        if(m_physical_pages[i])
                                m_cow_pages.set(i, true);
                }
        }
}

VMObject::~VMObject() {
        for(auto physical_page : m_physical_pages)
                if(physical_page)
                        MemoryManager::inst().get_physical_page(physical_page).unref();
}

PhysicalPage& VMObject::physical_page(size_t index) const {
        return MemoryManager::inst().get_physical_page(m_physical_pages[index]);
}

Result VMObject::try_cow_page(PageIndex page) {
        ASSERT(page < m_physical_pages.size());

        // Check under lock first, but don't hold it during physical page allocation.
        // Holding VMObject::Page while calling MM.alloc_physical_page() causes a
        // lock ordering violation: VMObject::Page -> PhysicalRegion, which can
        // deadlock against code that holds PhysicalRegion then acquires VMObject::Page.
        PageIndex old_page_index;
        {
                LOCK(m_page_lock);
                if(!page_is_cow(page))
                        return Result(EINVAL);
                old_page_index = m_physical_pages[page];
                ASSERT(old_page_index);
        }

        // Allocate and copy the new page WITHOUT holding VMObject::Page.
        auto new_page_res = MM.alloc_physical_page();
        if(new_page_res.is_error())
                return new_page_res.result();
        auto new_page = new_page_res.value();
        MM.copy_page(old_page_index, new_page);

        // Re-acquire to commit the swap.
        {
                LOCK(m_page_lock);
                // Double-check: another thread may have already resolved this CoW fault.
                if(!page_is_cow(page)) {
                        MM.free_physical_page(new_page);
                        return Result(Result::Success);
                }
                auto& slot = m_physical_pages[page];
                MM.get_physical_page(slot).unref();
                slot = new_page;
                m_cow_pages.set(page, false);
        }

        return Result(Result::Success);
}

ResultRet<kstd::Arc<VMObject>> VMObject::clone() {
        // Base VMObject::clone() tidak pernah boleh dipanggil langsung —
        // semua subclass (AnonymousVMObject, InodeVMObject) harus override ini.
        // Dulu ASSERT(false) = BSOD; sekarang kembalikan error supaya caller bisa handle.
        KLog::crit("VMObject", "clone() called on base VMObject '{}' — subclass must override!", m_name.c_str());
        return Result(ENOSYS);
}

void VMObject::become_cow_and_ref_pages() {
        LOCK(m_page_lock);
        for(size_t i = 0; i < m_physical_pages.size(); i++) {
                if(m_physical_pages[i]) {
                        m_cow_pages.set(i, true);
                        physical_page(i).ref();
                }
        }
}