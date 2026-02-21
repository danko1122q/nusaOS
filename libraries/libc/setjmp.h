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

#ifndef NUSAOS_LIBC_SETJMP_H
#define NUSAOS_LIBC_SETJMP_H

#include <sys/cdefs.h>
#include <stddef.h>

__DECL_BEGIN

struct __jmp_struct {
#if defined(i386) || defined(__i386) || defined(__i386__)
	long int ebx;
	long int esi;
	long int edi;
	long int ebp;
	long int esp;
	long int eip;
#elif defined(__aarch64__)
	// TODO
#else
	IMPLEMENT OTHER ARCHES...
#endif
};
typedef struct __jmp_struct jmp_buf[1];

int setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int val);

__DECL_END

#endif //NUSAOS_LIBC_SETJMP_H
