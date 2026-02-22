/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <libsys/Memory.h>
#include <sys/utsname.h>
#include <cstdio>
#include <cstdlib>

using namespace Sys;

static void get_uptime_str(char* buf, size_t buflen) {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) { snprintf(buf, buflen, "N/A"); return; }
    long seconds = -1;
    fscanf(f, "%ld", &seconds);
    fclose(f);
    if (seconds < 0) { snprintf(buf, buflen, "N/A"); return; }
    long days    = seconds / 86400;
    long hours   = (seconds % 86400) / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs    = seconds % 60;
    if (days > 0)
        snprintf(buf, buflen, "%ldd %02ld:%02ld:%02ld", days, hours, minutes, secs);
    else
        snprintf(buf, buflen, "%02ld:%02ld:%02ld", hours, minutes, secs);
}

int main() {
    utsname os;
    uname(&os);
    auto mem_res = Mem::get_info();

    const char* CYN = "\033[1;36m";
    const char* GRN = "\033[1;32m";
    const char* RST = "\033[0m";
    const char* BLD = "\033[1m";

    struct Row { const char* col; const char* txt; };
    Row logo[] = {
        { GRN, "                   .----.    " },
        { GRN, "  .---------.      | == |    " },
        { GRN, "  |\".......\"|      |----|    " },
        { GRN, "  ||       ||      | == |    " },
        { GRN, "  ||       ||      |----|    " },
        { GRN, "  |'-.....-'|      |::::|    " },
        { GRN, "  `\"\")---(\"\"\"      |___.|    " },
        { GRN, " /:::::::::::\"   _       "    },
        { GRN, "/:::=======:::\" \\`\\       "   },
        { GRN, "\"\"\"\"\"\"\"\"\"\"\"\"\"    '-'   "      },
    };

    const int LOGO_ROWS = 10;

    char user_host[256];
    snprintf(user_host, sizeof(user_host), "%s%suser%s@%s%s%s",
        BLD, CYN, RST, BLD, os.nodename, RST);

    char mem_str[64] = "N/A";
    if (!mem_res.is_error()) {
        auto& mem = mem_res.value();
        snprintf(mem_str, sizeof(mem_str), "%s / %s",
            mem.used.readable().c_str(),
            mem.usable.readable().c_str());
    }

    char uptime_str[64] = "N/A";
    get_uptime_str(uptime_str, sizeof(uptime_str));

    // Info rows — tidak perlu padding kosong di bawah lagi
    struct InfoRow { const char* label; const char* value; };
    const int INFO_ROWS = 6;
    InfoRow info[INFO_ROWS] = {
        { "",         user_host   },
        { "OS:",      os.sysname  },
        { "Version:",  os.release  },
        { "Arch:",    os.machine  },
        { "Uptime:",  uptime_str  },
        { "Memory:",  mem_str     },
    };

    printf("\n");

    // Cetak logo + info sejajar
    int rows = LOGO_ROWS > INFO_ROWS ? LOGO_ROWS : INFO_ROWS;
    for (int i = 0; i < rows; i++) {
        // Kolom kiri: logo
        if (i < LOGO_ROWS)
            printf("%s%s%s  ", GRN, logo[i].txt, RST);
        else
            printf("%-32s  ", ""); // padding kosong kalau logo habis

        // Kolom kanan: info
        if (i < INFO_ROWS) {
            const char* lbl = info[i].label;
            const char* val = info[i].value;
            if (lbl[0] != '\0')
                printf("%s%s%-9s%s %s", CYN, BLD, lbl, RST, val);
            else if (val[0] != '\0')
                printf("%s", val);
        }
        printf("\n");
    }

    // Color bar di bawah info — seperti neofetch
    // Hitung indentasi: lebar logo + 2 spasi = ~32 karakter
    printf("\n%-34s", "");
    for (int i = 0; i < 8; i++) printf("\033[%dm   ", 40 + i);
    printf("\033[0m\033[40m\n%-34s", "");
    for (int i = 0; i < 8; i++) printf("\033[%dm   ", 100 + i);
    printf("\033[0m\033[40m\n\n");

    return 0;
}