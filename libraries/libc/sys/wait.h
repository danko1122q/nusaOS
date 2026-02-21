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

#ifndef NUSAOS_LIBC_WAIT_H
#define NUSAOS_LIBC_WAIT_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <kernel/api/wait.h>

__DECL_BEGIN

pid_t waitpid(pid_t pid, int* wstatus, int options);
pid_t wait(int* wstatus);

__DECL_END

#endif //NUSAOS_WAIT_H
