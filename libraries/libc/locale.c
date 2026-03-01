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

	Copyright (c) danko1122q 2026. All rights reserved.
*/

#include "locale.h"
#include <stddef.h>

// ─── "C" / POSIX locale ───────────────────────────────────────────────────────
// Ini adalah locale default yang wajib ada di setiap implementasi libc.
// Semua nilai menggunakan POSIX/C standard:
//   - decimal_point  = "."   (titik sebagai pemisah desimal)
//   - thousands_sep  = ""    (tidak ada pemisah ribuan)
//   - currency, sign = ""    (tidak ada simbol mata uang)
//   - char fields    = CHAR_MAX  (nilai sentinel "tidak didefinisikan")
//
// Mengapa tidak return NULL:
//   localeconv() HARUS return pointer valid — banyak kode (termasuk printf
//   internal di beberapa implementasi) memanggil localeconv() dan langsung
//   dereference hasilnya tanpa null-check. Return NULL → segfault.

#define CHAR_MAX_VAL ((char)127)

static struct lconv c_locale = {
	/* decimal_point       */ ".",
	/* thousands_sep       */ "",
	/* grouping            */ "",
	/* int_curr_symbol     */ "",
	/* currency_symbol     */ "",
	/* mon_decimal_point   */ "",
	/* mon_thousands_sep   */ "",
	/* mon_grouping        */ "",
	/* positive_sign       */ "",
	/* negative_sign       */ "",
	/* int_frac_digits     */ CHAR_MAX_VAL,
	/* frac_digits         */ CHAR_MAX_VAL,
	/* p_cs_precedes       */ CHAR_MAX_VAL,
	/* p_sep_by_space      */ CHAR_MAX_VAL,
	/* n_cs_precedes       */ CHAR_MAX_VAL,
	/* n_sep_by_space      */ CHAR_MAX_VAL,
	/* p_sign_posn         */ CHAR_MAX_VAL,
	/* n_sign_posn         */ CHAR_MAX_VAL,
	/* int_p_cs_precedes   */ CHAR_MAX_VAL,
	/* int_p_sep_by_space  */ CHAR_MAX_VAL,
	/* int_n_cs_precedes   */ CHAR_MAX_VAL,
	/* int_n_sep_by_space  */ CHAR_MAX_VAL,
	/* int_p_sign_posn     */ CHAR_MAX_VAL,
	/* int_n_sign_posn     */ CHAR_MAX_VAL,
};

// Nama locale aktif — setlocale() hanya mendukung "C" dan "POSIX"
static char current_locale[] = "C";

// ─── setlocale() ─────────────────────────────────────────────────────────────
//
// POSIX: setlocale(category, NULL) → query locale saat ini tanpa mengubahnya
//        setlocale(category, "")   → set ke locale environment (tidak didukung,
//                                    kita tidak punya env locale, return "C")
//        setlocale(category, "C")  → set ke C/POSIX locale
//        setlocale(category, lain) → tidak didukung, return NULL
//
// Return value: pointer ke string nama locale, atau NULL jika gagal.
// String yang dikembalikan TIDAK boleh dimodifikasi oleh caller.
char* setlocale(int category, const char* locale) {
	// Query: return locale saat ini tanpa mengubah apapun
	if (locale == NULL)
		return current_locale;

	// Set ke C atau POSIX locale — satu-satunya yang kita dukung
	if (locale[0] == '\0'       // "" → gunakan env locale → fallback ke C
	    || (locale[0] == 'C' && locale[1] == '\0')          // "C"
	    || (locale[0] == 'P' && locale[1] == 'O'            // "POSIX"
	        && locale[2] == 'S' && locale[3] == 'I'
	        && locale[4] == 'X' && locale[5] == '\0'))
	{
		// category diabaikan karena kita hanya punya satu locale
		(void) category;
		return current_locale;
	}

	// Locale lain tidak didukung
	return NULL;
}

// ─── localeconv() ────────────────────────────────────────────────────────────
//
// Return pointer ke struct lconv yang mendeskripsikan locale saat ini.
// Pointer ini valid selama program berjalan dan tidak boleh di-free().
// Isinya tidak boleh dimodifikasi oleh caller (POSIX requirement).
struct lconv* localeconv(void) {
	return &c_locale;
}