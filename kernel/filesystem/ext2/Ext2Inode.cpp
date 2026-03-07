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

#include "Ext2Inode.h"
#include <kernel/kstd/cstring.h>
#include "Ext2BlockGroup.h"
#include "Ext2Filesystem.h"
#include <kernel/filesystem/DirectoryEntry.h>
#include <kernel/kstd/KLog.h>

Ext2Inode::Ext2Inode(Ext2Filesystem& filesystem, ino_t id): Inode(filesystem, id) {
	Ext2BlockGroup* bg = ext2fs().get_block_group(block_group());

	uint8_t block_buf[ext2fs().block_size()];
	ext2fs().read_blocks(bg->inode_table_block + block(), 1, block_buf);

	auto* inodeRaw = (Raw*)block_buf;
	inodeRaw += index() % ext2fs().inodes_per_block;
	memcpy(&raw, inodeRaw, sizeof(Ext2Inode::Raw));

	create_metadata();

	if(!_metadata.is_device())
		read_block_pointers();
}

Ext2Inode::Ext2Inode(Ext2Filesystem& filesystem, ino_t i, const Raw& raw, kstd::vector<uint32_t>& block_pointers, ino_t parent)
    : Inode(filesystem, i), block_pointers(block_pointers), raw(raw)
{
	create_metadata();
	if(IS_DIR(raw.mode)) {
		kstd::vector<DirectoryEntry> entries;
		entries.reserve(2);
		entries.push_back(DirectoryEntry(i, TYPE_DIR, "."));
		entries.push_back(DirectoryEntry(parent, TYPE_DIR, ".."));
		Result res = write_directory_entries(entries);
		if(res.is_error())
			KLog::err("ext2", "Error {} writing new ext2 directory inode's entries to disk", res.code());
	}
	write_to_disk();
}

Ext2Inode::~Ext2Inode() {
	if(_dirty && exists())
		write_to_disk();
}

uint32_t Ext2Inode::block_group() {
	return (id - 1) / ext2fs().superblock.inodes_per_group;
}

uint32_t Ext2Inode::index() {
	return (id - 1) % ext2fs().superblock.inodes_per_group;
}

uint32_t Ext2Inode::block() {
	return (index() * ext2fs().superblock.inode_size) / ext2fs().block_size();
}

Ext2Filesystem& Ext2Inode::ext2fs() {
	return (Ext2Filesystem&)(fs);
}

size_t Ext2Inode::num_blocks() {
	if(_metadata.is_symlink() && _metadata.size < 60) return 0;
	return (_metadata.size + ext2fs().block_size() - 1) / ext2fs().block_size();
}

uint32_t Ext2Inode::get_block_pointer(uint32_t block_index) {
	if(block_index >= block_pointers.size()) return 0;
	return block_pointers[block_index];
}

bool Ext2Inode::set_block_pointer(uint32_t block_index, uint32_t block) {
	LOCK(lock);

	if(block_index == 0 && block_pointers.empty()) {
		block_pointers.push_back(block);
		_dirty = true;
		return true;
	} else if(block_index < block_pointers.size()) {
		block_pointers[block_index] = block;
		_dirty = true;
		return true;
	} else if(block_index == block_pointers.size()) {
		block_pointers.push_back(block);
		_dirty = true;
		return true;
	} else return false;
}

kstd::vector<uint32_t>& Ext2Inode::get_block_pointers() {
	return block_pointers;
}

void Ext2Inode::free_all_blocks() {
	ext2fs().free_blocks(block_pointers);
	ext2fs().free_blocks(pointer_blocks);
}

ssize_t Ext2Inode::read(size_t start, size_t length, SafePointer<uint8_t> buffer, FileDescriptor* fd) {
	if(_metadata.is_device()) return 0;
	if(_metadata.size == 0) return 0;
	if(start >= _metadata.size) return 0;
	if(length == 0) return 0;
	if(!exists()) return -ENOENT;

	LOCK(lock);

	// Fast-path: symlinks < 60 bytes stored in block pointer array
	if(_metadata.is_symlink() && _metadata.size < 60) {
		ssize_t actual_length = min(_metadata.size - start, length);
		buffer.write(((uint8_t*)raw.block_pointers) + start, (size_t)actual_length);
		return actual_length;
	}

	if(start + length > _metadata.size) length = _metadata.size - start;

	size_t first_block     = start / ext2fs().block_size();
	size_t first_block_start = start % ext2fs().block_size();
	size_t bytes_left      = length;
	size_t block_index     = first_block;

	uint8_t block_buf[ext2fs().block_size()];
	while(bytes_left) {
		uint32_t blk = get_block_pointer(block_index);
		if(!blk) break; // Sparse / unallocated block — treat as zeros
		ext2fs().read_block(blk, block_buf);

		if(block_index == first_block) {
			size_t avail = ext2fs().block_size() - first_block_start;
			size_t to_copy = min(bytes_left, avail);
			buffer.write(block_buf + first_block_start, to_copy);
			bytes_left -= to_copy;
		} else {
			size_t to_copy = min(bytes_left, (size_t)ext2fs().block_size());
			buffer.write(block_buf, length - bytes_left, to_copy);
			bytes_left -= to_copy;
		}
		block_index++;
	}
	return (ssize_t)(length - bytes_left);
}

ssize_t Ext2Inode::write(size_t start, size_t length, SafePointer<uint8_t> buf, FileDescriptor* fd) {
	if(_metadata.is_device()) return 0;
	if(length == 0) return 0;
	if(!exists()) return -ENOENT;

	LOCK(lock);

	// Fast-path: inline symlink
	if(_metadata.is_symlink() && max(_metadata.size, start + length) < 60) {
		buf.read(((uint8_t*)raw.block_pointers) + start, length);
		if(start + length > _metadata.size) _metadata.size = start + length;
		write_inode_entry();
		return length;
	}

	// Expand the file if needed BEFORE writing
	if(start + length > _metadata.size) {
		auto res = truncate((off_t)start + (off_t)length);
		if(res.is_error()) return res.code();
	}

	size_t first_block      = start / ext2fs().block_size();
	size_t first_block_start = start % ext2fs().block_size();
	size_t bytes_left       = length;
	size_t block_index      = first_block;

	uint8_t block_buf[ext2fs().block_size()];
	while(bytes_left) {
		uint32_t blk = get_block_pointer(block_index);
		if(!blk) return -ENOSPC; // Should not happen after truncate succeeded

		ext2fs().read_block(blk, block_buf);

		if(block_index == first_block) {
			size_t avail   = ext2fs().block_size() - first_block_start;
			size_t to_copy = min(bytes_left, avail);
			buf.read(block_buf + first_block_start, to_copy);
			bytes_left -= to_copy;
		} else {
			size_t to_copy = min(bytes_left, (size_t)ext2fs().block_size());
			buf.read(block_buf, length - bytes_left, to_copy);
			bytes_left -= to_copy;
		}

		ext2fs().write_block(blk, block_buf);
		block_index++;
	}

	// Update modification time and persist inode entry
	// TODO: replace 0 with real timestamp when kernel provides one
	raw.mtime = 0;
	_dirty = true;
	write_inode_entry();

	return length;
}

void Ext2Inode::iterate_entries(kstd::IterationFunc<const DirectoryEntry&> callback) {
	LOCK(lock);
	uint8_t buf[ext2fs().block_size()];
	size_t block = 0;
	while(read(block * ext2fs().block_size(), ext2fs().block_size(), KernelPointer<uint8_t>(buf), nullptr)) {
		size_t i = 0;
		while((i < ext2fs().block_size()) && (block * ext2fs().block_size() + i < _metadata.size)) {
			auto* dir = (ext2_directory*)(buf + i);
			if(dir->size == 0) break; // Guard against corrupt entries

			// Skip null/free entries (inode=0 is the null terminator or deleted entry)
			if(dir->inode == 0) {
				i += dir->size;
				continue;
			}

			size_t name_length = dir->name_length;
			if(name_length > NAME_MAXLEN - 1)
				name_length = NAME_MAXLEN - 1;

			DirectoryEntry result;
			result.name_length = name_length;
			result.id = dir->inode;
			result.type = dir->type;
			memcpy(result.name, &dir->type + 1, name_length);
			result.name[name_length] = '\0';
			ITER_RET(callback(result));

			i += dir->size;
		}
		block++;
	}
}

ino_t Ext2Inode::find_id(const kstd::string& find_name) {
	if(!metadata().is_directory()) return 0;
	LOCK(lock);
	ino_t ret = 0;
	uint8_t buf[ext2fs().block_size()];
	size_t blk = 0;
	// Use read() so we go through the same abstraction as iterate_entries,
	// ensuring any cached/dirty blocks are seen correctly.
	while(read(blk * ext2fs().block_size(), ext2fs().block_size(), KernelPointer<uint8_t>(buf), nullptr)) {
		auto* dir = reinterpret_cast<ext2_directory*>(buf);
		uint32_t add = 0;
		char name_buf[NAME_MAXLEN + 1];
		while(add < ext2fs().block_size()) {
			if(dir->size == 0) break;
			// Skip free/null entries (inode=0) — same fix as iterate_entries
			if(dir->inode == 0) {
				add += dir->size;
				dir = (ext2_directory*)((size_t)dir + dir->size);
				continue;
			}
			size_t nl = min((size_t)dir->name_length, (size_t)NAME_MAXLEN);
			memcpy(name_buf, &dir->type + 1, nl);
			name_buf[nl] = '\0';
			if(find_name == name_buf) {
				ret = dir->inode;
				break;
			}
			add += dir->size;
			dir = (ext2_directory*)((size_t)dir + dir->size);
		}
		if(ret) break;
		blk++;
	}
	return ret;
}

Result Ext2Inode::add_entry(const kstd::string& name, Inode& inode) {
	ASSERT(inode.fs.fsid() == ext2fs().fsid());
	if(!metadata().is_directory()) return Result(-ENOTDIR);
	if(!name.length() || name.length() > NAME_MAXLEN) return Result(-ENAMETOOLONG);

	LOCK(lock);

	kstd::vector<DirectoryEntry> entries;
	bool exist = false;
	iterate_entries([&](const DirectoryEntry& dirent) -> kstd::IterationAction {
		entries.push_back(dirent);
		if(name == dirent.name) {
			exist = true;
			return kstd::IterationAction::Break;
		}
		return kstd::IterationAction::Continue;
	});
	if(exist) return Result(-EEXIST);

	uint8_t type = EXT2_FT_UNKNOWN;
	if(inode.metadata().is_simple_file())      type = EXT2_FT_REG_FILE;
	else if(inode.metadata().is_directory())   type = EXT2_FT_DIR;
	else if(inode.metadata().is_symlink())     type = EXT2_FT_SYMLINK;
	else if(inode.metadata().is_block_device())     type = EXT2_FT_BLKDEV;
	else if(inode.metadata().is_character_device()) type = EXT2_FT_CHRDEV;

	((Ext2Inode&)inode).increase_hardlink_count();

	entries.push_back({inode.id, type, name});
	auto res = write_directory_entries(entries);
	if(res.is_error()) return res;
	if(_dirty) {
		res = write_to_disk();
		if(res.is_error()) return res;
	}

	return Result(SUCCESS);
}

ResultRet<kstd::Arc<Inode>> Ext2Inode::create_entry(const kstd::string& name, mode_t mode, uid_t uid, gid_t gid) {
	if(!name.length() || name.length() > NAME_MAXLEN) return Result(-ENAMETOOLONG);

	LOCK(lock);

	auto inode_or_err = ext2fs().allocate_inode(mode, uid, gid, 0, id);
	if(inode_or_err.is_error())
		return inode_or_err.result();

	if(IS_DIR(mode)) {
		raw.hard_links++;
		write_inode_entry();
	}

	auto entry_result = add_entry(name, *inode_or_err.value());
	if(entry_result.is_error()) {
		auto free_or_err = ext2fs().free_inode(*inode_or_err.value());
		if(free_or_err.is_error())
			KLog::err("ext2", "Error freeing inode {} after entry creation error! ({})\n",
			          inode_or_err.value()->id, free_or_err.code());
		return entry_result;
	}

	return static_cast<kstd::Arc<Inode>>(inode_or_err.value());
}

Result Ext2Inode::remove_entry(const kstd::string& name) {
	if(!metadata().is_directory()) return Result(-ENOTDIR);
	if(!name.length() || name.length() > NAME_MAXLEN) return Result(-ENAMETOOLONG);

	LOCK(lock);

	kstd::vector<DirectoryEntry> entries;
	int entry_index = 0;
	bool found = false;
	int i = 0;
	iterate_entries([&](const DirectoryEntry& dirent) -> kstd::IterationAction {
		entries.push_back(dirent);
		if(name == dirent.name) {
			entry_index = i;
			found = true;
		}
		i++;
		return kstd::IterationAction::Continue;
	});

	if(!found) return Result(-ENOENT);
	auto child_or_err = ext2fs().get_inode(entries[entry_index].id);
	if(child_or_err.is_error()) {
		KLog::warn("ext2", "Orphaned directory entry in inode {}", id);
		return child_or_err.result();
	}

	auto ext2ino = (kstd::Arc<Ext2Inode>)child_or_err.value();
	if(ext2ino->metadata().is_directory()) {
		auto result = ext2ino->try_remove_dir();
		if(result.is_error())
			return result;
	} else {
		ext2ino->reduce_hardlink_count();
	}

	entries.erase(entry_index);
	auto res = write_directory_entries(entries);
	if(res.is_error()) return res;
	if(_dirty) {
		res = write_to_disk();
		if(res.is_error()) return res;
	}
	return Result(SUCCESS);
}

Result Ext2Inode::truncate(off_t length) {
	if(length < 0) return Result(-EINVAL);
	if((size_t)length == _metadata.size) return Result(SUCCESS);
	LOCK(lock);

	uint32_t new_num_blocks = ((size_t)length + ext2fs().block_size() - 1) / ext2fs().block_size();

	if(new_num_blocks > num_blocks()) {
		// Expand: allocate new blocks
		uint32_t needed = new_num_blocks - num_blocks();
		auto new_blocks_res = ext2fs().allocate_blocks(needed, true);
		if(new_blocks_res.is_error())
			return new_blocks_res.result();

		auto new_blocks = new_blocks_res.value();
		if(new_blocks.size() != needed) {
			// Partial allocation — rollback and report ENOSPC
			ext2fs().free_blocks(new_blocks);
			return Result(-ENOSPC);
		}

		for(size_t i = 0; i < new_blocks.size(); i++)
			block_pointers.push_back(new_blocks[i]);

		// Update metadata AFTER successful allocation
		_metadata.size = (size_t)length;
		write_to_disk();
	} else if(new_num_blocks < num_blocks()) {
		// Shrink: free excess data blocks
		for(size_t i = num_blocks(); i > new_num_blocks; i--)
			ext2fs().free_block(get_block_pointer(i - 1));
		block_pointers.resize(new_num_blocks);

		size_t new_num_ptr_blocks = calculate_num_ptr_blocks(new_num_blocks);
		for(size_t i = pointer_blocks.size(); i > new_num_ptr_blocks; i--)
			ext2fs().free_block(pointer_blocks[i - 1]);
		pointer_blocks.resize(new_num_ptr_blocks);

		// Clear raw indirect pointer fields for levels that are no longer needed.
		// This prevents write_block_pointers() from reusing the now-freed blocks
		// on the next expansion, which caused the double-free warning.
		const size_t N = ext2fs().block_pointers_per_block;
		if(new_num_blocks <= 12)
			raw.s_pointer = 0;
		if(new_num_blocks <= 12 + N)
			raw.d_pointer = 0;
		if(new_num_blocks <= 12 + N + N * N)
			raw.t_pointer = 0;

		// Zero out unused portion of the last block
		if(length % ext2fs().block_size() && new_num_blocks > 0)
			ext2fs().truncate_block(get_block_pointer(new_num_blocks - 1), length % ext2fs().block_size());

		_metadata.size = (size_t)length;
		write_to_disk();
	} else {
		_metadata.size = (size_t)length;
		write_inode_entry();
	}

	return Result(SUCCESS);
}

Result Ext2Inode::chmod(mode_t mode) {
	LOCK(lock);
	_metadata.mode = mode;
	write_inode_entry();
	return Result(SUCCESS);
}

Result Ext2Inode::chown(uid_t uid, gid_t gid) {
	LOCK(lock);
	_metadata.uid = uid;
	_metadata.gid = gid;
	write_inode_entry();
	return Result(SUCCESS);
}

void Ext2Inode::read_singly_indirect(uint32_t singly_indirect_block, uint32_t& block_index) {
	if(!singly_indirect_block || block_index >= num_blocks()) return;
	uint8_t block_buf[ext2fs().block_size()];
	ext2fs().read_block(singly_indirect_block, block_buf);
	pointer_blocks.push_back(singly_indirect_block);
	for(uint32_t i = 0; i < ext2fs().block_pointers_per_block && block_index < num_blocks(); i++) {
		block_pointers.push_back(((uint32_t*)block_buf)[i]);
		block_index++;
	}
}

void Ext2Inode::read_doubly_indirect(uint32_t doubly_indirect_block, uint32_t& block_index) {
	if(!doubly_indirect_block || block_index >= num_blocks()) return;
	uint8_t block_buf[ext2fs().block_size()];
	ext2fs().read_block(doubly_indirect_block, block_buf);
	pointer_blocks.push_back(doubly_indirect_block);
	for(uint32_t i = 0; i < ext2fs().block_pointers_per_block && block_index < num_blocks(); i++)
		read_singly_indirect(((uint32_t*)block_buf)[i], block_index);
}

void Ext2Inode::read_triply_indirect(uint32_t triply_indirect_block, uint32_t& block_index) {
	if(!triply_indirect_block || block_index >= num_blocks()) return;
	uint8_t block_buf[ext2fs().block_size()];
	ext2fs().read_block(triply_indirect_block, block_buf);
	pointer_blocks.push_back(triply_indirect_block);
	for(uint32_t i = 0; i < ext2fs().block_pointers_per_block && block_index < num_blocks(); i++)
		read_doubly_indirect(((uint32_t*)block_buf)[i], block_index);
}

void Ext2Inode::read_block_pointers() {
	LOCK(lock);

	if(_metadata.is_symlink() && _metadata.size < 60) {
		pointer_blocks = kstd::vector<uint32_t>();
		block_pointers = kstd::vector<uint32_t>();
		return;
	}

	block_pointers = kstd::vector<uint32_t>();
	block_pointers.reserve(num_blocks());
	pointer_blocks = kstd::vector<uint32_t>();

	uint32_t block_index = 0;
	while(block_index < 12 && block_index < num_blocks()) {
		block_pointers.push_back(raw.block_pointers[block_index]);
		block_index++;
	}

	if(raw.s_pointer) read_singly_indirect(raw.s_pointer, block_index);
	if(raw.d_pointer) read_doubly_indirect(raw.d_pointer, block_index);
	if(raw.t_pointer) read_triply_indirect(raw.t_pointer, block_index);
}

Result Ext2Inode::write_to_disk() {
	LOCK(lock);

	Result res = write_block_pointers();
	if(res.is_error())
		return res;

	res = write_inode_entry();
	if(res.is_error())
		return res;

	return Result(SUCCESS);
}

Result Ext2Inode::write_block_pointers() {
	LOCK(lock);
	if(_metadata.is_symlink() && _metadata.size < 60) return Result(SUCCESS);

	pointer_blocks = kstd::vector<uint32_t>(0);
	pointer_blocks.reserve(calculate_num_ptr_blocks(num_blocks()));

	if(_metadata.is_device()) {
		// TODO: Update device inode
		return Result(SUCCESS);
	}

	// Direct block pointers (0–11)
	for(uint32_t i = 0; i < 12; i++)
		raw.block_pointers[i] = get_block_pointer(i);

	uint8_t block_buf[ext2fs().block_size()];

	// ─── Singly indirect (blocks 12 .. 12+N-1) ───────────────────────────────
	if(num_blocks() > 12) {
		if(!raw.s_pointer) {
			raw.s_pointer = ext2fs().allocate_block();
			if(!raw.s_pointer) return Result(-ENOSPC);
		}
		pointer_blocks.push_back(raw.s_pointer);

		memset(block_buf, 0, ext2fs().block_size());
		for(uint32_t i = 0; i < ext2fs().block_pointers_per_block; i++) {
			uint32_t bp_idx = 12 + i;
			((uint32_t*)block_buf)[i] = (bp_idx < block_pointers.size()) ? block_pointers[bp_idx] : 0;
		}
		ext2fs().write_block(raw.s_pointer, block_buf);
	} else {
		raw.s_pointer = 0;
	}

	// ─── Doubly indirect ────────────────────────────────────────────────────
	size_t dind_start = 12 + ext2fs().block_pointers_per_block;
	if(num_blocks() > dind_start) {
		if(!raw.d_pointer) {
			raw.d_pointer = ext2fs().allocate_block();
			if(!raw.d_pointer) return Result(-ENOSPC);
		}
		pointer_blocks.push_back(raw.d_pointer);

		// Read existing doubly-indirect block so we don't lose already-allocated singly blocks
		ext2fs().read_block(raw.d_pointer, block_buf);

		uint8_t sblock_buf[ext2fs().block_size()];
		uint32_t cur_block = dind_start;

		for(uint32_t di = 0; di < ext2fs().block_pointers_per_block; di++) {
			if(cur_block >= block_pointers.size()) break;

			uint32_t sblk = ((uint32_t*)block_buf)[di];
			if(!sblk) {
				sblk = ext2fs().allocate_block();
				if(!sblk) return Result(-ENOSPC);
				((uint32_t*)block_buf)[di] = sblk;
			}
			pointer_blocks.push_back(sblk);

			memset(sblock_buf, 0, ext2fs().block_size());
			for(uint32_t si = 0; si < ext2fs().block_pointers_per_block; si++) {
				((uint32_t*)sblock_buf)[si] = (cur_block < block_pointers.size()) ? block_pointers[cur_block] : 0;
				cur_block++;
			}
			ext2fs().write_block(sblk, sblock_buf);
		}

		ext2fs().write_block(raw.d_pointer, block_buf);
	} else {
		raw.d_pointer = 0;
	}

	// ─── Triply indirect ────────────────────────────────────────────────────
	size_t tind_start = dind_start + ext2fs().block_pointers_per_block * ext2fs().block_pointers_per_block;
	if(num_blocks() > tind_start) {
		if(!raw.t_pointer) {
			raw.t_pointer = ext2fs().allocate_block();
			if(!raw.t_pointer) return Result(-ENOSPC);
		}
		pointer_blocks.push_back(raw.t_pointer);

		uint8_t tblock_buf[ext2fs().block_size()];
		ext2fs().read_block(raw.t_pointer, tblock_buf);

		uint8_t dblock_buf[ext2fs().block_size()];
		uint8_t sblock_buf[ext2fs().block_size()];
		uint32_t cur_block = tind_start;

		for(uint32_t ti = 0; ti < ext2fs().block_pointers_per_block; ti++) {
			if(cur_block >= block_pointers.size()) break;

			uint32_t dblk = ((uint32_t*)tblock_buf)[ti];
			if(!dblk) {
				dblk = ext2fs().allocate_block();
				if(!dblk) return Result(-ENOSPC);
				((uint32_t*)tblock_buf)[ti] = dblk;
			}
			pointer_blocks.push_back(dblk);

			ext2fs().read_block(dblk, dblock_buf);

			for(uint32_t di = 0; di < ext2fs().block_pointers_per_block; di++) {
				if(cur_block >= block_pointers.size()) break;

				uint32_t sblk = ((uint32_t*)dblock_buf)[di];
				if(!sblk) {
					sblk = ext2fs().allocate_block();
					if(!sblk) return Result(-ENOSPC);
					((uint32_t*)dblock_buf)[di] = sblk;
				}
				pointer_blocks.push_back(sblk);

				memset(sblock_buf, 0, ext2fs().block_size());
				for(uint32_t si = 0; si < ext2fs().block_pointers_per_block; si++) {
					((uint32_t*)sblock_buf)[si] = (cur_block < block_pointers.size()) ? block_pointers[cur_block] : 0;
					cur_block++;
				}
				ext2fs().write_block(sblk, sblock_buf);
			}

			ext2fs().write_block(dblk, dblock_buf);
		}

		ext2fs().write_block(raw.t_pointer, tblock_buf);
	} else {
		raw.t_pointer = 0;
	}

	raw.logical_blocks = (block_pointers.size() + pointer_blocks.size()) * (ext2fs().block_size() / 512);
	_dirty = true;
	return Result(SUCCESS);
}

Result Ext2Inode::write_inode_entry() {
	LOCK(lock);
	uint8_t block_buf[ext2fs().block_size()];

	raw.size = _metadata.size;
	raw.mode = _metadata.mode;
	raw.uid  = _metadata.uid;
	raw.gid  = _metadata.gid;

	Ext2BlockGroup* bg = ext2fs().get_block_group(block_group());
	ext2fs().read_block(bg->inode_table_block + block(), block_buf);

	auto* inodeRaw = (Raw*)block_buf;
	inodeRaw += index() % ext2fs().inodes_per_block;
	memcpy(inodeRaw, &raw, sizeof(Ext2Inode::Raw));

	ext2fs().write_block(bg->inode_table_block + block(), block_buf);
	_dirty = false;

	return Result(SUCCESS);
}

Result Ext2Inode::write_directory_entries(kstd::vector<DirectoryEntry>& entries) {
	LOCK(lock);

	// Determine new file size
	size_t new_filesize = 0;
	for(size_t i = 0; i < entries.size(); i++) {
		size_t ent_size = sizeof(ext2_directory) + entries[i].name_length;
		ent_size += ent_size % 4 ? 4 - ent_size % 4 : 0;
		new_filesize += ent_size;
	}

	// Ensure room for a null entry at the end
	if(new_filesize % ext2fs().block_size() == 0) new_filesize++;
	new_filesize = ((new_filesize + ext2fs().block_size() - 1) / ext2fs().block_size()) * ext2fs().block_size();

	auto res = truncate((off_t)new_filesize);
	if(res.is_error()) return res;

	size_t cur_block = 0;
	size_t cur_byte  = 0;
	uint8_t block_buf[ext2fs().block_size()];
	memset(block_buf, 0, ext2fs().block_size());

	for(size_t i = 0; i < entries.size(); i++) {
		DirectoryEntry& ent = entries[i];

		ext2_directory raw_ent;
		raw_ent.name_length = ent.name_length;
		raw_ent.type   = ent.type;
		raw_ent.inode  = ent.id;
		raw_ent.size   = sizeof(raw_ent) + raw_ent.name_length;
		raw_ent.size  += raw_ent.size % 4 ? 4 - raw_ent.size % 4 : 0;

		if(raw_ent.size + cur_byte >= ext2fs().block_size()) {
			ext2fs().write_block(get_block_pointer(cur_block), block_buf);
			memset(block_buf, 0, ext2fs().block_size());
			cur_block++;
			cur_byte = 0;
		}

		memcpy(block_buf + cur_byte, &raw_ent, sizeof(raw_ent));
		memcpy(block_buf + cur_byte + sizeof(raw_ent), ent.name, ent.name_length + 1);
		cur_byte += raw_ent.size;
	}

	if(cur_byte >= ext2fs().block_size()) {
		ext2fs().write_block(get_block_pointer(cur_block), block_buf);
		memset(block_buf, 0, ext2fs().block_size());
		cur_block++;
		cur_byte = 0;
	}

	// Null terminator entry
	ext2_directory end_ent;
	end_ent.size        = ext2fs().block_size() - cur_byte;
	end_ent.type        = EXT2_FT_UNKNOWN;
	end_ent.name_length = 0;
	end_ent.inode       = 0;
	memcpy(block_buf + cur_byte, &end_ent, sizeof(end_ent));
	ext2fs().write_block(get_block_pointer(cur_block), block_buf);

	return Result(SUCCESS);
}

void Ext2Inode::create_metadata() {
	InodeMetadata meta;
	meta.mode     = raw.mode;
	meta.size     = raw.size;
	meta.uid      = raw.uid;
	meta.gid      = raw.gid;
	meta.inode_id = id;
	if(meta.is_device()) {
		unsigned device = raw.block_pointers[0];
		if(!device) device = raw.block_pointers[1];
		meta.dev_major = (device & 0xfff00u) >> 8u;
		meta.dev_minor = (device & 0xffu) | ((device >> 12u) & 0xfff00u);
	}
	_metadata = meta;
}

void Ext2Inode::reduce_hardlink_count() {
	LOCK(lock);
	raw.hard_links--;
	write_inode_entry();
	if(raw.hard_links == 0) {
		ext2fs().remove_cached_inode(id);
		ext2fs().free_inode(*this);
	}
}

void Ext2Inode::increase_hardlink_count() {
	LOCK(lock);
	raw.hard_links++;
	write_inode_entry();
}

Result Ext2Inode::try_remove_dir() {
	if(!metadata().is_directory()) return Result(-ENOTDIR);

	LOCK(lock);

	kstd::vector<DirectoryEntry> entries;
	iterate_entries([&](const DirectoryEntry& dirent) -> kstd::IterationAction {
		entries.push_back(dirent);
		return kstd::IterationAction::Continue;
	});

	// entries[0] = ".", entries[1] = ".."
	if(entries.size() > 2) return Result(-ENOTEMPTY);

	for(size_t i = 0; i < entries.size(); i++) {
		DirectoryEntry& ent = entries[i];
		if(ent.id != id) { // ".."
			auto parent_ino_or_err = ext2fs().get_inode(ent.id);
			if(parent_ino_or_err.is_error())
				return parent_ino_or_err.result();
			auto parent_ino = (kstd::Arc<Ext2Inode>)parent_ino_or_err.value();
			parent_ino->reduce_hardlink_count();
		}
	}

	raw.hard_links = 0;
	write_inode_entry();
	ext2fs().remove_cached_inode(id);
	ext2fs().free_inode(*this);

	return Result(SUCCESS);
}

uint32_t Ext2Inode::calculate_num_ptr_blocks(uint32_t num_data_blocks) {
	if(num_data_blocks <= 12) return 0;

	uint32_t ret = 0;
	uint32_t blocks_left = num_data_blocks - 12;
	const uint32_t N = ext2fs().block_pointers_per_block;

	// Singly indirect: 1 pointer block + up to N data blocks
	uint32_t s_data = min(blocks_left, N);
	blocks_left -= s_data;
	ret += 1; // the singly-indirect block itself
	if(blocks_left == 0) return ret;

	// Doubly indirect: 1 pointer block + ceil(blocks/N) singly blocks + data
	uint32_t d_data = min(blocks_left, N * N);
	blocks_left -= d_data;
	ret += 1 + (d_data + N - 1) / N;
	if(blocks_left == 0) return ret;

	// Triply indirect: 1 pointer block + ceil(blocks/(N*N)) doubly blocks + ...
	uint32_t t_data = blocks_left;
	uint32_t t_d_blocks = (t_data + N * N - 1) / (N * N);
	uint32_t t_s_blocks = (t_data + N - 1) / N;
	ret += 1 + t_d_blocks + t_s_blocks;

	return ret;
}

void Ext2Inode::open(FileDescriptor& fd, int options) {
}

void Ext2Inode::close(FileDescriptor& fd) {
}