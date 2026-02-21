/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "../tasking/Process.h"
#include "../memory/SafePointer.h"
#include "../api/utsname.h"
#include <nusaos_version.h>

int Process::sys_uname(UserspacePointer<struct utsname> buf) {
	utsname ret {
			"nusaOS",
			"nusaOS",
			"",
			"",
			"i386"
	};

	strcpy(ret.release, NUSAOS_VERSION_STRING);
	strcpy(ret.version, NUSAOS_REVISION);

	buf.set(ret);
	return SUCCESS;
}