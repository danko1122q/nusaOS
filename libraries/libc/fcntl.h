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

#ifndef NUSAOS_FCNTL_H
#define NUSAOS_FCNTL_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <kernel/api/fcntl.h>
#include <time.h>

__DECL_BEGIN

int open(const char* pathname, int flags, ...);
int openat(int dirfd, const char* pathname, int flags);
int fcntl(int fd, int cmd, ...);
int utimensat(int dirfd, char const* path, struct timespec const times[2], int flag);

__DECL_END

#endif //NUSAOS_FCNTL_H
