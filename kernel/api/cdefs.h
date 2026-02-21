/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#pragma once

#ifndef NUSAOS_CDEFS_H
#define NUSAOS_CDEFS_H

#define _POSIX_VERSION 200809L

#ifdef __cplusplus
#ifndef __DECL_BEGIN
#define __DECL_BEGIN extern "C" {
#define __DECL_END }
#endif
#else
#ifndef __DECL_BEGIN
		#define __DECL_BEGIN
		#define __DECL_END
	#endif
#endif

#endif //NUSAOS_CDEFSL_H