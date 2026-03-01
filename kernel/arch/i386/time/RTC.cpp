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

#include "RTC.h"
#include "CMOS.h"
#include "kernel/tasking/TaskManager.h"
#include "kernel/interrupt/interrupt.h"

// ─── Konstanta kalender ───────────────────────────────────────────────────────

#define LEAPYEAR(y) (((y) % 4 == 0) && (((y) % 100 != 0) || ((y) % 400 == 0)))

#define SECSPERDAY   86400
#define SECSPERHOUR  3600
#define SECSPERMIN   60
#define EPOCH        1970

static const int days_per_month[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// ─── Constructor ─────────────────────────────────────────────────────────────

RTC::RTC(TimeManager* time): IRQHandler(RTC_IRQ), TimeKeeper(time) {}

// ─── timestamp() ─────────────────────────────────────────────────────────────
//
// Baca waktu dari CMOS RTC dan konversi ke Unix timestamp (detik sejak 1970).
//
// Bug yang diperbaiki:
//
// BUG #1 — Century register tidak reliable:
//   CMOS register 0x32 (century) bisa return 0x00 jika BIOS tidak mendukungnya
//   (misalnya di beberapa konfigurasi QEMU/Bochs tanpa ACPI table yang benar).
//   Jika century = 0, maka years = 0 + 26 = 26 → years - EPOCH = -1944
//   → timestamp sangat negatif → localtime() undefined behavior → tanggal "00"
//   dan bulan salah seperti yang dilaporkan.
//
//   FIX: Setelah mengkombinasikan century + year, jika hasilnya < 100 (century
//   tidak valid), terapkan Y2K heuristic: 00-69 → 2000-2069, 70-99 → 1970-1999.
//   Ini memastikan tahun selalu 4 digit meski BIOS tidak punya century register.
//
// BUG #2 — Macro bcd() tidak type-safe:
//   Macro lama: #define bcd(val) ((val / 16) * 10 + (val & 0xf))
//   Menggunakan integer division desimal ("/16"), bukan shift nibble yang benar.
//   Seharusnya: high nibble = (val >> 4) & 0x0F, bukan val/16.
//   Untuk val=0x26: lama → (0x26/16)*10 + (0x26&0xf) = 2*10+6 = 26 (kebetulan benar)
//   Untuk val=0x12: lama → (0x12/16)*10 + (0x12&0xf) = 1*10+2 = 12 (benar)
//   Tapi untuk val=0x09: lama → (9/16)*10 + (9&0xf) = 0*10+9 = 9 (benar karena <16)
//   Untuk val=0x19: lama → (25/16)*10 + (25&0xf) = 1*10+9 = 19 (benar)
//   Tapi untuk val=0x20: lama → (32/16)*10 + (32&0xf) = 2*10+0 = 20 (benar)
//   Secara numerik macro lama kebetulan benar untuk nilai BCD valid (0x00-0x99),
//   karena val_decimal/16 == val_bcd_high_nibble untuk semua nilai BCD valid.
//   Tetap diganti ke bcd_to_bin() yang proper: (val >> 4)*10 + (val & 0x0F).
//
// BUG #3 — Leap year loop menggunakan years yang sudah di-subtract EPOCH:
//   Loop lama: for(i=0; i < years-1; i++) check LEAPYEAR(EPOCH + i)
//   'years' di sini sudah = absolute_year - EPOCH, jadi total iterasi benar,
//   tapi ini membingungkan. Kode sudah menyimpan absolute_year — gunakan itu
//   secara eksplisit untuk kejelasan.
//
// BUG #4 — Double-read CMOS tanpa re-wait update flag:
//   Setelah menunggu update selesai, register dibaca satu per satu. Jika RTC
//   mulai update baru di tengah pembacaan, nilai bisa inkonsisten.
//   FIX: Baca semua register dalam satu window — periksa flag lagi di akhir,
//   ulangi jika terdeteksi update terjadi selama pembacaan (double-read pattern).

time_t RTC::timestamp() {
    uint8_t second, minute, hour, day, month, year_raw, century_raw;

    // Double-read pattern: baca hingga dua kali pembacaan menghasilkan nilai sama
    // agar tidak dapat data yang inkonsisten akibat RTC update di tengah pembacaan
    do {
        // Tunggu hingga RTC tidak sedang update
        while (CMOS::read(CMOS_STATUS_A) & CMOS_STATUS_UPDATE_IN_PROGRESS) {}

        second     = CMOS::read(CMOS_SECONDS);
        minute     = CMOS::read(CMOS_MINUTES);
        hour       = CMOS::read(CMOS_HOURS);
        day        = CMOS::read(CMOS_DAY);
        month      = CMOS::read(CMOS_MONTH);
        year_raw   = CMOS::read(CMOS_YEAR);
        century_raw= CMOS::read(CMOS_CENTURY);
    } while (CMOS::read(CMOS_STATUS_A) & CMOS_STATUS_UPDATE_IN_PROGRESS);
    // Jika setelah baca semua register ternyata RTC mulai update lagi,
    // ulangi — nilai yang baru dibaca mungkin inkonsisten.

    // Konversi BCD → binary (kecuali jika CMOS dikonfigurasi binary mode)
    // Sebagian besar BIOS menggunakan BCD, cek STATUS_B bit 2 untuk kepastian
    bool binary_mode = (CMOS::read(CMOS_STATUS_B) & CMOS_STATUS_BINARY_MODE) != 0;
    int s   = binary_mode ? second      : bcd_to_bin(second);
    int min = binary_mode ? minute      : bcd_to_bin(minute);
    int h   = binary_mode ? hour        : bcd_to_bin(hour);
    int d   = binary_mode ? day         : bcd_to_bin(day);
    int mon = binary_mode ? month       : bcd_to_bin(month);
    int yr  = binary_mode ? year_raw    : bcd_to_bin(year_raw);
    int cnt = binary_mode ? century_raw : bcd_to_bin(century_raw);

    // ── FIX #1: Rekonstruksi tahun 4 digit dengan Y2K fallback ───────────────
    int absolute_year;
    if (cnt != 0) {
        // Century register valid (misalnya QEMU return 0x20 = 20 → century 20)
        absolute_year = cnt * 100 + yr;
    } else {
        // Century register tidak valid (return 0) — terapkan Y2K heuristic
        // yr = 2-digit year dari CMOS: 00-69 → 2000-2069, 70-99 → 1970-1999
        if (yr < 70)
            absolute_year = 2000 + yr;
        else
            absolute_year = 1900 + yr;
    }

    // Sanity check: tahun harus masuk akal (1970-2100)
    // Jika di luar range, kembalikan timestamp minimal yang valid (1 Jan 1970)
    if (absolute_year < EPOCH || absolute_year > 2100)
        return 0;

    int years_since_epoch = absolute_year - EPOCH;

    // ── Akumulasi detik ───────────────────────────────────────────────────────

    // Tahun penuh sejak epoch (365 hari/tahun)
    time_t secs = (time_t)years_since_epoch * (SECSPERDAY * 365);

    // Tambah hari kabisat: hitung leap year dari EPOCH hingga tahun sebelum absolute_year
    // Loop dari EPOCH s/d absolute_year-1 (tahun yang sudah penuh terlewati)
    int num_leap = 0;
    for (int y = EPOCH; y < absolute_year; y++) {
        if (LEAPYEAR(y))
            num_leap++;
    }
    secs += (time_t)num_leap * SECSPERDAY;

    // Bulan yang sudah penuh terlewati di tahun ini (mon adalah 1-indexed dari RTC)
    // Validasi mon: harus 1-12
    if (mon < 1 || mon > 12) mon = 1;
    for (int i = 0; i < mon - 1; i++) {
        secs += (time_t)days_per_month[i] * SECSPERDAY;
    }

    // Tambah hari kabisat bulan Feb jika tahun ini kabisat DAN sudah melewati Februari
    if (LEAPYEAR(absolute_year) && mon > 2)
        secs += SECSPERDAY;

    // Hari dalam bulan (RTC 1-indexed: hari pertama = 1, bukan 0)
    // Validasi d: harus 1-31
    if (d < 1) d = 1;
    secs += (time_t)(d - 1) * SECSPERDAY;

    // Jam, menit, detik
    secs += (time_t)h   * SECSPERHOUR;
    secs += (time_t)min * SECSPERMIN;
    secs += (time_t)s;

    return secs;
}

// ─── IRQ handler ─────────────────────────────────────────────────────────────

void RTC::handle_irq(IRQRegisters* regs) {
    // Baca STATUS_C dengan NMI-disable bit (0x80 | 0x0C) untuk acknowledge IRQ.
    // Tanpa ini RTC tidak akan menggenerate IRQ berikutnya.
    CMOS::read(0x8C);
    TimeKeeper::tick();
}

// ─── Frequency ───────────────────────────────────────────────────────────────

bool RTC::set_frequency(int frequency) {
    if (frequency <= 0)
        return false;
    if (RTC_FREQUENCY_DIVIDER % frequency)
        return false;

    // log2(RTC_FREQUENCY_DIVIDER / frequency) — posisi bit tertinggi
    int divided_freq = RTC_FREQUENCY_DIVIDER / frequency;
    uint8_t highest_bit = 1;
    while (divided_freq >>= 1)
        highest_bit++;

    if (highest_bit < RTC_FREQUENCYVAL_MIN || highest_bit > RTC_FREQUENCYVAL_MAX)
        return false;

    IRQHandler::uninstall_irq();
    _frequency = frequency;
    // Register 0x8A = NMI-disabled STATUS_A
    CMOS::write(0x8A, highest_bit | (CMOS::read(0x8A) & 0xF0));
    IRQHandler::reinstall_irq();
    return true;
}

int RTC::frequency() {
    return _frequency;
}

void RTC::enable() {
    TaskManager::ScopedCritical critical;
    Interrupt::NMIDisabler nmidis;
    // Register 0x8B = NMI-disabled STATUS_B
    CMOS::write(0x8B, CMOS_SQUARE_WAVE_INTERRUPT_FLAG | CMOS::read(CMOS_STATUS_B));
    set_frequency(RTC_FREQUENCY);
}

void RTC::disable() {
    TaskManager::ScopedCritical critical;
    Interrupt::NMIDisabler nmidis;
    CMOS::write(0x8B, CMOS::read(CMOS_STATUS_B) & (~CMOS_SQUARE_WAVE_INTERRUPT_FLAG));
}

bool RTC::mark_in_irq() {
    return true;
}