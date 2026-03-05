/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "InodeVMObject.h"

kstd::Arc<InodeVMObject> InodeVMObject::make_for_inode(kstd::string name, kstd::Arc<Inode> inode, InodeVMObject::Type type) {
	kstd::vector<PageIndex> pages;
	pages.resize((inode->metadata().size + PAGE_SIZE - 1) / PAGE_SIZE);
	memset(pages.storage(), 0, pages.size() * sizeof(PageIndex));
	return kstd::Arc<InodeVMObject>(new InodeVMObject(name, pages, kstd::move(inode), type, false));
}

ResultRet<kstd::Arc<VMObject>> InodeVMObject::clone() {
	ASSERT(m_inode);
	LOCK(m_page_lock);
	ASSERT(m_type == Type::Private);
	become_cow_and_ref_pages();
	auto new_object = kstd::Arc(new InodeVMObject(m_name, m_physical_pages, m_inode, m_type, m_type == Type::Private));
	new_object->m_committed_pages = this->m_committed_pages;
	return kstd::static_pointer_cast<VMObject>(new_object);
}

InodeVMObject::InodeVMObject(kstd::string name, kstd::vector<PageIndex> physical_pages, kstd::Arc<Inode> inode, InodeVMObject::Type type, bool cow):
	VMObject(kstd::move(name), kstd::move(physical_pages), cow),
	m_inode(kstd::move(inode)),
	m_type(type)
{}

ResultRet<bool> InodeVMObject::try_fault_in_page(size_t index) {
	// Check under lock, then release before touching the allocator.
	// Holding VMObject::Page during MM.alloc_physical_page() causes a
	// VMObject::Page -> PhysicalRegion lock ordering violation.
	{
		LOCK(m_page_lock);
		if(index >= m_physical_pages.size())
			return Result(ERANGE);
		if(m_physical_pages[index])
			return false;
	}

	// Allocate and read the page WITHOUT holding VMObject::Page.
	auto new_page_res = MM.alloc_physical_page();
	if(new_page_res.is_error())
		return new_page_res.result();
	auto new_page = new_page_res.value();

	ssize_t nread = 0;
	MM.with_quickmapped(new_page, [&](void* buf) {
		nread = m_inode->read(index * PAGE_SIZE, PAGE_SIZE, KernelPointer<uint8_t>((uint8_t*) buf), nullptr);
	});
	if(nread < 0) {
		MM.free_physical_page(new_page);
		return Result(-nread);
	}

	// Re-acquire to commit. Double-check in case another thread raced us.
	{
		LOCK(m_page_lock);
		if(m_physical_pages[index]) {
			// Another thread already faulted this page in.
			MM.free_physical_page(new_page);
			return false;
		}
		m_committed_pages++;
		m_physical_pages[index] = new_page;
	}

	return true;
}