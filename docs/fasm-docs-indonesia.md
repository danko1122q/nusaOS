# FASM di nusaOS — Panduan Lengkap

FASM (Flat Assembler) adalah assembler x86 yang cepat dan berjalan langsung di atas nusaOS. Kamu tulis kode assembly di file `.asm`, lalu FASM mengompilasinya langsung menjadi binary yang bisa dijalankan — tanpa linker, tanpa alat tambahan.

Panduan ini mencakup semua hal yang kamu butuhkan untuk menulis, mengompilasi, dan menjalankan program assembly x86 di nusaOS.

---

## Daftar Isi

1. [Cara Mulai](#1-cara-mulai)
2. [Program Pertama](#2-program-pertama)
3. [Struktur Program](#3-struktur-program)
4. [Register x86](#4-register-x86)
5. [Definisi Data](#5-definisi-data)
6. [Akses Memori](#6-akses-memori)
7. [Instruksi — Perpindahan Data](#7-instruksi--perpindahan-data)
8. [Instruksi — Aritmetika](#8-instruksi--aritmetika)
9. [Instruksi — Logika & Bitwise](#9-instruksi--logika--bitwise)
10. [Instruksi — Alur Kontrol](#10-instruksi--alur-kontrol)
11. [Instruksi — Stack](#11-instruksi--stack)
12. [Instruksi — Operasi String](#12-instruksi--operasi-string)
13. [Syscall nusaOS](#13-syscall-nusaos)
14. [Makro](#14-makro)
15. [Konstanta dan Label](#15-konstanta-dan-label)
16. [Assembly Kondisional](#16-assembly-kondisional)
17. [Include File](#17-include-file)
18. [Contoh Program Lengkap](#18-contoh-program-lengkap)
19. [Kesalahan Umum](#19-kesalahan-umum)
20. [Referensi Cepat Instruksi](#20-referensi-cepat-instruksi)

---

## 1. Cara Mulai

FASM adalah satu binary yang ada di `/bin/fasm` di dalam nusaOS.

```sh
# Kompilasi source.asm menjadi executable bernama output
fasm source.asm output

# Kompilasi dengan nama output otomatis (sama dengan input, tanpa ekstensi)
fasm source.asm

# Batasi memori ke 512 KB
fasm -m 512 source.asm output

# Batasi jumlah maksimal passes ke 10
fasm -p 10 source.asm output

# Definisikan variabel simbolik sebelum kompilasi
fasm -d VERSI=2 source.asm output

# Simpan informasi simbol ke file
fasm -s simbol.txt source.asm output
```

Alur kerjanya hanya dua langkah:

```
Tulis kode (.asm)  →  fasm source.asm output  →  ./output
```

---

## 2. Program Pertama

Buat file bernama `halo.asm`:

```asm
format ELF executable 3
entry start

segment readable executable

start:
    mov eax, 4          ; SYS_WRITE
    mov ebx, 1          ; stdout (fd = 1)
    mov ecx, pesan
    mov edx, panjang
    int 0x80

    mov eax, 1          ; SYS_EXIT
    xor ebx, ebx        ; kode keluar = 0
    int 0x80

segment readable

pesan   db 'Halo dari nusaOS!', 0xA
panjang = $ - pesan
```

Kompilasi dan jalankan:

```sh
fasm halo.asm halo
./halo
```

Output:
```
Halo dari nusaOS!
```

### Aturan dasar penulisan

- Satu instruksi per baris
- Label diakhiri dengan titik dua: `start:`
- Titik koma memulai komentar: `; ini komentar`
- Semua teks setelah `;` di baris tersebut diabaikan
- FASM **tidak membedakan huruf besar/kecil** untuk mnemonic — `MOV`, `mov`, `Mov` semuanya sama
- Label **membedakan huruf besar/kecil** — `Start` dan `start` adalah label berbeda

---

## 3. Struktur Program

Setiap executable nusaOS yang ditulis dengan FASM dimulai dengan dua baris:

```asm
format ELF executable 3   ; format output
entry start               ; label titik masuk program
```

Lalu satu atau lebih **segment**:

```asm
segment readable executable    ; kode program ada di sini

segment readable               ; data hanya-baca (string, konstanta)

segment readable writeable     ; data yang bisa diubah (variabel)
```

### Direktif format

| Direktif | Keterangan |
|----------|------------|
| `format ELF` | Object file relocatable (butuh linker — tidak bisa langsung dijalankan) |
| `format ELF executable 3` | ELF executable standalone untuk nusaOS |

Selalu pakai `format ELF executable 3` untuk program yang ingin langsung dijalankan dengan `./`.

### Atribut segment

| Atribut | Artinya |
|---------|---------|
| `readable` | Segment bisa dibaca |
| `writeable` | Segment bisa ditulis (untuk variabel) |
| `executable` | Segment berisi kode yang bisa dijalankan |

Layout tipikal:

```asm
format ELF executable 3
entry start

segment readable executable    ; .text — semua kode program

start:
    ; ... program kamu ...
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable               ; .rodata — string dan konstanta
    pesan    db 'Halo!', 0xA
    panjang  = $ - pesan

segment readable writeable     ; .bss / .data — variabel yang bisa diubah
    buffer   rb 256            ; cadangkan 256 byte
    counter  dd 0              ; 32-bit diinisialisasi ke 0
```

---

## 4. Register x86

x86 (i386) memiliki delapan register tujuan umum, semuanya 32-bit.

### Register tujuan umum

| 32-bit | 16-bit | 8-bit atas | 8-bit bawah | Kegunaan umum |
|--------|--------|------------|-------------|---------------|
| `eax` | `ax` | `ah` | `al` | Akumulator, nomor syscall, nilai kembalian |
| `ebx` | `bx` | `bh` | `bl` | Argumen syscall 1, base pointer |
| `ecx` | `cx` | `ch` | `cl` | Argumen syscall 2, counter loop |
| `edx` | `dx` | `dh` | `dl` | Argumen syscall 3, register data |
| `esi` | `si` | — | — | Argumen syscall 4, source index |
| `edi` | `di` | — | — | Argumen syscall 5, destination index |
| `esp` | `sp` | — | — | Stack pointer (jangan sembarangan diubah) |
| `ebp` | `bp` | — | — | Base pointer (stack frame) |

### Register khusus

| Register | Keterangan |
|----------|------------|
| `eip` | Instruction pointer — alamat instruksi yang sedang dieksekusi (tidak bisa langsung ditulis) |
| `eflags` | Register flag — menyimpan hasil perbandingan (ZF, CF, SF, OF, dll.) |

### Contoh sub-register

```asm
mov eax, 0x12345678
; eax = 0x12345678
; ax  = 0x5678  (16 bit bawah)
; ah  = 0x56    (bit 8–15)
; al  = 0x78    (bit 0–7)
```

---

## 5. Definisi Data

### Definisi byte, word, dword

| Direktif | Ukuran | Contoh |
|----------|--------|--------|
| `db` | 1 byte | `db 0x41` → simpan byte 0x41 ('A') |
| `dw` | 2 byte | `dw 0x1234` |
| `dd` | 4 byte | `dd 0xDEADBEEF` |
| `dq` | 8 byte | `dq 0` |

### String

```asm
pesan   db 'Halo, nusaOS!', 0xA, 0    ; string + newline + null terminator
nama    db "Dava", 0                   ; string null-terminated
```

Tanda kutip tunggal dan ganda keduanya diterima.

### Cadangkan ruang (belum diinisialisasi)

| Direktif | Mencadangkan |
|----------|-------------|
| `rb N` | N byte |
| `rw N` | N word (2N byte) |
| `rd N` | N dword (4N byte) |
| `rq N` | N qword (8N byte) |

```asm
segment readable writeable
    buffer  rb 256     ; cadangkan 256 byte untuk buffer
    angka   rd 1       ; cadangkan ruang untuk satu integer 32-bit
```

### Hitung panjang dengan `$`

`$` adalah alamat saat ini. Gunakan untuk menghitung panjang string:

```asm
pesan    db 'Halo!', 0xA
panjang  = $ - pesan    ; panjang = 6 (5 karakter + newline)
```

---

## 6. Akses Memori

Tanda kurung siku `[ ]` berarti "nilai di alamat memori ini" — seperti dereferensiasi pointer.

```asm
mov eax, [label]        ; ambil nilai 32-bit dari memori di alamat label
mov [label], eax        ; simpan nilai 32-bit ke memori di alamat label
mov al, [label]         ; ambil 1 byte
mov [label], al         ; simpan 1 byte
```

### Penentu ukuran

Gunakan `byte`, `word`, `dword` untuk menegaskan ukuran secara eksplisit:

```asm
mov byte [buffer], 0x41         ; simpan 1 byte
mov word [buffer], 0x1234       ; simpan 2 byte
mov dword [counter], 100        ; simpan 4 byte
```

### Pengalamatan tidak langsung dan terindeks

```asm
mov eax, [ebx]          ; ambil dari alamat yang ada di ebx
mov eax, [ebx+4]        ; ambil dari ebx + 4
mov eax, [ebx+ecx*4]    ; ambil dari ebx + ecx * 4
mov eax, [ebx+ecx*4+8]  ; dengan displacement
```

### Contoh — tulis ke buffer

```asm
segment readable writeable
    buffer rb 16

segment readable executable
start:
    mov byte [buffer],   'H'
    mov byte [buffer+1], 'i'
    mov byte [buffer+2], 0xA   ; newline
```

---

## 7. Instruksi — Perpindahan Data

### mov — pindahkan data

Menyalin nilai dari sumber ke tujuan. Sumber tidak berubah.

```asm
mov eax, 42             ; eax = 42  (nilai langsung)
mov eax, ebx            ; eax = ebx (register ke register)
mov eax, [var]          ; eax = memori[var]
mov [var], eax          ; memori[var] = eax
mov eax, label          ; eax = alamat label
```

**Tidak bisa** memindahkan memori ke memori secara langsung:
```asm
mov [dst], [src]        ; TIDAK VALID — harus lewat register dulu
```

### lea — muat alamat efektif

Menghitung alamat dan menyimpannya di register — **tidak** membaca memori.

```asm
lea eax, [pesan]        ; eax = alamat pesan
lea eax, [ebx+ecx*4]    ; eax = ebx + ecx*4 (tanpa baca memori)
```

Berguna untuk menghitung offset tanpa menyentuh memori.

### xchg — tukar nilai

Menukar nilai dua operand.

```asm
xchg eax, ebx           ; tukar eax dan ebx
```

### movzx — pindah dengan perluasan nol

Menyalin nilai yang lebih kecil ke register yang lebih besar, mengisi bit atas dengan nol.

```asm
movzx eax, al           ; eax = perluas nol al ke 32 bit
movzx eax, byte [buf]   ; eax = perluas nol byte di buf
```

### movsx — pindah dengan perluasan tanda

Seperti `movzx` tapi mempertahankan bit tanda (untuk angka negatif).

```asm
movsx eax, al           ; perluas tanda al ke 32 bit
```

---

## 8. Instruksi — Aritmetika

### add, sub — penjumlahan dan pengurangan

```asm
add eax, 10         ; eax = eax + 10
add eax, ebx        ; eax = eax + ebx
sub eax, 5          ; eax = eax - 5
sub eax, ecx        ; eax = eax - ecx
add [var], 1        ; variabel memori += 1
```

### inc, dec — tambah dan kurang satu

```asm
inc eax             ; eax = eax + 1
dec ecx             ; ecx = ecx - 1
inc dword [counter] ; variabel memori += 1
```

### mul — perkalian tanpa tanda (unsigned)

`mul` selalu menggunakan `eax` sebagai satu operand. Hasilnya masuk ke `edx:eax` (64-bit).

```asm
mov eax, 6
mov ecx, 7
mul ecx             ; edx:eax = eax * ecx = 42
                    ; eax = 42, edx = 0 (jika hasil muat 32 bit)
```

### imul — perkalian bertanda (signed)

```asm
imul eax, ecx           ; eax = eax * ecx
imul eax, ecx, 10       ; eax = ecx * 10
```

### div — pembagian unsigned

Membagi `edx:eax` dengan operand. Hasil bagi → `eax`, sisa → `edx`.

```asm
xor edx, edx        ; nolkan edx dulu (32 bit atas)
mov eax, 17
mov ecx, 5
div ecx             ; eax = 3 (hasil bagi), edx = 2 (sisa)
```

> **Peringatan:** Selalu nolkan `edx` sebelum `div` jika membagi angka 32-bit. Jika tidak, akan terjadi overflow pembagian dan program crash.

### idiv — pembagian bertanda

Sama seperti `div` tapi memperlakukan nilai sebagai bertanda.

### neg — negasi

```asm
neg eax             ; eax = -eax  (komplemen dua)
```

---

## 9. Instruksi — Logika & Bitwise

### and, or, xor — operasi bitwise

```asm
and eax, 0xFF       ; mask: hanya pertahankan 8 bit bawah
or  eax, 0x80       ; set bit 7
xor eax, eax        ; cara tercepat menolkan eax (eax = 0)
xor eax, 0x01       ; balik bit 0
```

### not — NOT bitwise

```asm
not eax             ; balik semua bit (komplemen satu)
```

### shl, shr — geser kiri / kanan

```asm
shl eax, 1          ; eax = eax * 2   (geser kiri 1 bit)
shl eax, 3          ; eax = eax * 8
shr eax, 1          ; eax = eax / 2   (unsigned, geser kanan 1 bit)
shr eax, cl         ; geser kanan sebanyak nilai di cl
```

### sar — geser kanan aritmetika (mempertahankan tanda)

```asm
sar eax, 1          ; eax = eax / 2  (bertanda)
```

### cmp — bandingkan (set flag, tidak simpan hasil)

```asm
cmp eax, ebx        ; set flag berdasarkan eax - ebx
cmp eax, 0          ; bandingkan eax dengan nol
```

`cmp` tidak mengubah register apapun — hanya set flag yang dipakai oleh jump kondisional.

### test — AND bitwise (hanya set flag)

```asm
test eax, eax       ; cek apakah eax == 0 (ZF ter-set jika ya)
test al, 0x01       ; cek apakah bit 0 ter-set
```

---

## 10. Instruksi — Alur Kontrol

### Lompat tanpa syarat

```asm
jmp label           ; lompat ke label tanpa syarat
```

### Lompat kondisional (setelah cmp atau test)

| Instruksi | Kondisi | Bertanda? |
|-----------|---------|-----------|
| `je` / `jz` | Sama / Nol | keduanya |
| `jne` / `jnz` | Tidak sama / Tidak nol | keduanya |
| `jl` / `jnge` | Kurang dari | bertanda |
| `jle` / `jng` | Kurang dari atau sama | bertanda |
| `jg` / `jnle` | Lebih dari | bertanda |
| `jge` / `jnl` | Lebih dari atau sama | bertanda |
| `jb` / `jnae` | Di bawah (unsigned kurang) | unsigned |
| `jbe` / `jna` | Di bawah atau sama | unsigned |
| `ja` / `jnbe` | Di atas (unsigned lebih) | unsigned |
| `jae` / `jnb` | Di atas atau sama | unsigned |
| `js` | Flag tanda ter-set (negatif) | — |
| `jns` | Flag tanda tidak ter-set (positif) | — |
| `jc` | Flag carry ter-set | — |
| `jnc` | Flag carry tidak ter-set | — |

Contoh — if/else:

```asm
    cmp eax, 10
    jge .lebih_besar

    ; eax < 10
    mov ebx, 0
    jmp .selesai

  .lebih_besar:
    ; eax >= 10
    mov ebx, 1

  .selesai:
```

### call dan ret — subrutin

```asm
    call fungsi_ku      ; push alamat kembali, lompat ke fungsi_ku

fungsi_ku:
    ; ... lakukan sesuatu ...
    ret                 ; pop alamat kembali, lompat balik
```

### loop — counter loop menggunakan ecx

```asm
    mov ecx, 5          ; ulangi 5 kali
  .ulang:
    ; ... isi loop ...
    loop .ulang         ; dec ecx; jnz .ulang
```

> `loop` mengurangi `ecx` dan melompat jika `ecx` ≠ 0. Berhenti ketika `ecx` mencapai 0.

### Label lokal

Label yang diawali titik `.` bersifat lokal terhadap label non-lokal terdekat di atasnya. Ini memungkinkan kamu menggunakan nama seperti `.loop` atau `.selesai` di beberapa fungsi tanpa konflik:

```asm
fungsi_a:
    jmp .selesai        ; lompat ke .selesai milik fungsi_a

  .selesai:
    ret

fungsi_b:
    jmp .selesai        ; lompat ke .selesai milik fungsi_b (berbeda)

  .selesai:
    ret
```

---

## 11. Instruksi — Stack

Stack tumbuh **ke bawah** di memori. `esp` selalu menunjuk ke puncak stack (nilai terakhir yang di-push).

### push — masukkan ke stack

```asm
push eax            ; esp -= 4, lalu [esp] = eax
push 42             ; push nilai langsung
push dword [var]    ; push nilai dari memori
```

### pop — ambil dari stack

```asm
pop eax             ; eax = [esp], lalu esp += 4
pop ecx
```

### Menyimpan register di subrutin

Selalu simpan register yang akan kamu ubah dan kembalikan sebelum `ret`:

```asm
fungsi_ku:
    push ebx            ; simpan
    push ecx

    ; gunakan ebx, ecx dengan bebas di sini

    pop ecx             ; kembalikan dalam urutan terbalik
    pop ebx
    ret
```

### pusha / popa — push/pop semua register tujuan umum

```asm
pusha               ; simpan eax, ecx, edx, ebx, esp, ebp, esi, edi
popa                ; kembalikan semua
```

---

## 12. Instruksi — Operasi String

Instruksi-instruksi ini bekerja dengan `esi` (sumber) dan `edi` (tujuan), dan otomatis memajukan pointer. Arah dikendalikan oleh direction flag (bersihkan dengan `cld` → maju, set dengan `std` → mundur).

Selalu gunakan `cld` sebelum operasi string kecuali kamu memang butuh urutan terbalik.

| Instruksi | Operasi |
|-----------|---------|
| `movsb` | Salin byte dari `[esi]` ke `[edi]`, majukan keduanya |
| `movsw` | Salin word |
| `movsd` | Salin dword |
| `stosb` | Simpan `al` ke `[edi]`, majukan `edi` |
| `stosw` | Simpan `ax` |
| `stosd` | Simpan `eax` |
| `lodsb` | Muat byte dari `[esi]` ke `al`, majukan `esi` |
| `lodsw` | Muat word ke `ax` |
| `lodsd` | Muat dword ke `eax` |
| `scasb` | Bandingkan `al` dengan `[edi]`, majukan `edi` |
| `cmpsb` | Bandingkan `[esi]` dengan `[edi]`, majukan keduanya |

### Prefiks rep — ulangi

Mengulang instruksi string berikutnya sebanyak `ecx` kali:

```asm
; Nolkan 256 byte buffer
    cld
    xor eax, eax
    mov edi, buffer
    mov ecx, 256
    rep stosb           ; simpan al=0 ke [edi] 256 kali

; Salin 16 byte dari sumber ke tujuan
    cld
    mov esi, sumber
    mov edi, tujuan
    mov ecx, 16
    rep movsb
```

---

## 13. Syscall nusaOS

nusaOS menggunakan **konvensi syscall Linux i386**: taruh nomor syscall di `eax`, argumen di `ebx`, `ecx`, `edx`, `esi`, `edi`, lalu jalankan `int 0x80`. Nilai kembalian ada di `eax`.

```
int 0x80
  eax = nomor syscall
  ebx = argumen 1
  ecx = argumen 2
  edx = argumen 3
  esi = argumen 4
  edi = argumen 5
```

### Tabel syscall

| Nomor | Nama | Argumen | Keterangan |
|-------|------|---------|------------|
| 1 | `SYS_EXIT` | `ebx` = kode keluar | Hentikan program |
| 2 | `SYS_FORK` | — | Fork proses |
| 3 | `SYS_READ` | `ebx`=fd, `ecx`=buf, `edx`=jumlah | Baca dari file/stdin |
| 4 | `SYS_WRITE` | `ebx`=fd, `ecx`=buf, `edx`=jumlah | Tulis ke file/stdout/stderr |
| 7 | `SYS_OPEN` | `ebx`=path, `ecx`=flags, `edx`=mode | Buka file |
| 8 | `SYS_CLOSE` | `ebx`=fd | Tutup file descriptor |
| 9 | `SYS_FSTAT` | `ebx`=fd, `ecx`=stat_buf | Info file via fd |
| 10 | `SYS_STAT` | `ebx`=path, `ecx`=stat_buf | Info file via path |
| 11 | `SYS_LSEEK` | `ebx`=fd, `ecx`=offset, `edx`=whence | Pindahkan posisi file |
| 12 | `SYS_KILL` | `ebx`=pid, `ecx`=sinyal | Kirim sinyal |
| 13 | `SYS_GETPID` | — | Dapatkan ID proses |
| 15 | `SYS_UNLINK` | `ebx`=path | Hapus file |
| 16 | `SYS_GETTIMEOFDAY` | `ebx`=timeval_buf, `ecx`=0 | Dapatkan waktu saat ini |
| 22 | `SYS_CHDIR` | `ebx`=path | Ganti direktori |
| 23 | `SYS_GETCWD` | `ebx`=buf, `ecx`=ukuran | Dapatkan direktori saat ini |
| 25 | `SYS_RMDIR` | `ebx`=path | Hapus direktori |
| 26 | `SYS_MKDIR` | `ebx`=path, `ecx`=mode | Buat direktori |
| 36 | `SYS_READLINK` | `ebx`=path, `ecx`=buf, `edx`=ukuran | Baca symbolic link |
| 59 | `SYS_MMAP` | `ebx`=addr, `ecx`=len, `edx`=prot, `esi`=flags, `edi`=fd | Petakan memori |
| 60 | `SYS_MUNMAP` | `ebx`=addr, `ecx`=len | Lepas pemetaan memori |
| 75 | `SYS_ACCESS` | `ebx`=path, `ecx`=mode | Cek akses file |
| 77 | `SYS_UNAME` | `ebx`=utsname_buf | Dapatkan info OS |

### Konstanta file descriptor

| Nilai | Artinya |
|-------|---------|
| `0` | stdin |
| `1` | stdout |
| `2` | stderr |

### Flag buka file (untuk SYS_OPEN)

| Flag | Nilai | Artinya |
|------|-------|---------|
| `O_RDONLY` | 0 | Hanya baca |
| `O_WRONLY` | 1 | Hanya tulis |
| `O_RDWR` | 2 | Baca dan tulis |
| `O_CREAT` | 0x40 | Buat jika belum ada |
| `O_TRUNC` | 0x200 | Kosongkan ke panjang nol |
| `O_APPEND` | 0x400 | Mode tambah di akhir |

### Nilai whence untuk lseek

| Nilai | Artinya |
|-------|---------|
| `0` | SEEK_SET — dari awal file |
| `1` | SEEK_CUR — dari posisi saat ini |
| `2` | SEEK_END — dari akhir file |

### Contoh penggunaan syscall

**Tulis "Halo" ke stdout:**
```asm
    mov eax, 4          ; SYS_WRITE
    mov ebx, 1          ; fd = stdout
    mov ecx, pesan
    mov edx, panjang
    int 0x80
```

**Baca dari stdin ke buffer:**
```asm
    mov eax, 3          ; SYS_READ
    mov ebx, 0          ; fd = stdin
    mov ecx, buffer
    mov edx, 256        ; maksimal byte yang dibaca
    int 0x80
    ; eax = jumlah byte yang benar-benar terbaca
```

**Keluar dengan kode 0:**
```asm
    mov eax, 1          ; SYS_EXIT
    xor ebx, ebx        ; kode keluar = 0
    int 0x80
```

**Buka file:**
```asm
    mov eax, 7          ; SYS_OPEN
    mov ebx, namafile   ; path null-terminated
    mov ecx, 0          ; O_RDONLY
    xor edx, edx
    int 0x80
    ; eax = file descriptor (negatif = error)
    mov [fd], eax
```

**Tulis ke file:**
```asm
    mov eax, 4          ; SYS_WRITE
    mov ebx, [fd]       ; file descriptor
    mov ecx, data
    mov edx, panjang_data
    int 0x80
```

**Tutup file:**
```asm
    mov eax, 8          ; SYS_CLOSE
    mov ebx, [fd]
    int 0x80
```

---

## 14. Makro

FASM memiliki sistem makro yang powerful. Makro memungkinkan kamu mendefinisikan pola kode yang bisa digunakan ulang.

### Makro sederhana

```asm
macro keluar kode
{
    mov eax, 1
    mov ebx, kode
    int 0x80
}

; pemakaian:
keluar 0
keluar 42
```

### Makro dengan beberapa argumen

```asm
macro tulis fd, buf, pjg
{
    mov eax, 4
    mov ebx, fd
    mov ecx, buf
    mov edx, pjg
    int 0x80
}

; pemakaian:
tulis 1, pesan, panjang_pesan
```

### Makro variadic (argumen tak terbatas)

```asm
macro cetak_baris [teks]
{
    common
    local .str, .len
    jmp .lewati
  .str db teks, 0xA
  .len = $ - .str
  .lewati:
    mov eax, 4
    mov ebx, 1
    mov ecx, .str
    mov edx, .len
    int 0x80
}

; pemakaian:
cetak_baris 'Halo!'
cetak_baris 'Baris kedua'
```

### struc — definisi struktur

```asm
struc Titik x, y
{
    .x dd x
    .y dd y
}

; deklarasi Titik:
p1 Titik 10, 20

; akses field:
    mov eax, [p1.x]     ; eax = 10
    mov ecx, [p1.y]     ; ecx = 20
```

---

## 15. Konstanta dan Label

### Konstanta numerik

```asm
STDIN  = 0
STDOUT = 1
STDERR = 2

SYS_EXIT  = 1
SYS_WRITE = 4
SYS_READ  = 3
```

Konstanta yang didefinisikan dengan `=` adalah nilai waktu kompilasi. Bisa digunakan di mana saja angka diharapkan.

### Konstanta string

```asm
STRING_VERSI equ '1.0'
```

`equ` mendefinisikan penggantian teks — `STRING_VERSI` digantikan dengan `'1.0'` di mana pun digunakan.

### Label

Label menandai sebuah alamat di memori:

```asm
start:          ; label biasa — menandai alamat saat ini
.loop:          ; label lokal — cakupannya ke label induk terdekat
```

Label bisa digunakan sebagai target lompatan atau sebagai alamat (misalnya di `mov ecx, label`).

---

## 16. Assembly Kondisional

```asm
DEBUG = 1

if DEBUG = 1
    ; kode ini disertakan
    mov eax, [debug_var]
    ...
end if

if DEBUG = 0
    ; kode ini dilewati
end if
```

Berguna untuk mengaktifkan/menonaktifkan output debug atau membangun versi berbeda dari satu source yang sama.

```asm
ARSITEKTUR = 32

if ARSITEKTUR = 64
    ; kode khusus 64-bit
else
    ; kode 32-bit
end if
```

---

## 17. Include File

Pecah program besar menjadi beberapa file:

```asm
include 'utils.asm'         ; include relatif terhadap file saat ini
include '/home/lib/io.asm'  ; include dengan path absolut
```

File yang di-include disisipkan di titik tersebut seolah kamu mengetiknya langsung.

Pola yang umum — taruh makro dan konstanta helper di file terpisah:

```asm
; makro.asm
macro keluar kode { mov eax,1 ; mov ebx,kode ; int 0x80 }
macro tulis fd,buf,pjg { mov eax,4 ; mov ebx,fd ; mov ecx,buf ; mov edx,pjg ; int 0x80 }

STDOUT    = 1
SYS_READ  = 3
SYS_WRITE = 4
SYS_EXIT  = 1
```

```asm
; utama.asm
format ELF executable 3
entry start

include 'makro.asm'

segment readable executable
start:
    tulis STDOUT, pesan, panjang_pesan
    keluar 0

segment readable
    pesan          db 'Halo!', 0xA
    panjang_pesan  = $ - pesan
```

---

## 18. Contoh Program Lengkap

### Contoh 1 — Halo Dunia

```asm
format ELF executable 3
entry start

segment readable executable

start:
    mov eax, 4
    mov ebx, 1
    mov ecx, pesan
    mov edx, panjang
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable

pesan    db 'Halo, nusaOS!', 0xA
panjang  = $ - pesan
```

---

### Contoh 2 — Baca input dan cetak kembali

```asm
format ELF executable 3
entry start

segment readable executable

start:
    ; cetak prompt
    mov eax, 4
    mov ebx, 1
    mov ecx, prompt
    mov edx, panjang_prompt
    int 0x80

    ; baca dari stdin
    mov eax, 3
    mov ebx, 0
    mov ecx, buffer
    mov edx, 256
    int 0x80
    mov [jumlah_baca], eax

    ; cetak awalan
    mov eax, 4
    mov ebx, 1
    mov ecx, balasan
    mov edx, panjang_balasan
    int 0x80

    ; cetak ulang yang diketik
    mov eax, 4
    mov ebx, 1
    mov ecx, buffer
    mov edx, [jumlah_baca]
    int 0x80

    ; keluar
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable

prompt          db 'Masukkan nama kamu: ', 0
panjang_prompt  = $ - prompt
balasan         db 'Halo, ', 0
panjang_balasan = $ - balasan

segment readable writeable

buffer      rb 256
jumlah_baca dd 0
```

---

### Contoh 3 — Hitung dari 1 sampai 10

```asm
format ELF executable 3
entry start

segment readable executable

; Mengubah angka satu digit di eax ke ASCII dan mencetaknya
cetak_digit:
    push eax
    add al, '0'
    mov [buf_digit], al
    mov eax, 4
    mov ebx, 1
    mov ecx, buf_digit
    mov edx, 1
    int 0x80
    pop eax
    ret

; Cetak baris baru
cetak_baris_baru:
    mov eax, 4
    mov ebx, 1
    mov ecx, baris_baru
    mov edx, 1
    int 0x80
    ret

start:
    mov ecx, 1              ; counter = 1

  .loop:
    cmp ecx, 10
    jg  .selesai            ; hentikan ketika counter > 10

    mov eax, ecx
    call cetak_digit
    call cetak_baris_baru

    inc ecx
    jmp .loop

  .selesai:
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable writeable
    buf_digit   rb 1

segment readable
    baris_baru  db 0xA
```

---

### Contoh 4 — Baca file dan cetak isinya

```asm
format ELF executable 3
entry start

O_RDONLY = 0

segment readable executable

start:
    ; buka file
    mov eax, 7              ; SYS_OPEN
    mov ebx, namafile
    mov ecx, O_RDONLY
    xor edx, edx
    int 0x80
    cmp eax, 0
    jl  .error_buka
    mov [fd], eax

  .loop_baca:
    ; baca potongan
    mov eax, 3              ; SYS_READ
    mov ebx, [fd]
    mov ecx, buffer
    mov edx, 512
    int 0x80
    cmp eax, 0
    jle .tutup              ; 0 = EOF, negatif = error

    ; tulis ke stdout
    mov edx, eax            ; byte terbaca = byte yang ditulis
    mov eax, 4              ; SYS_WRITE
    mov ebx, 1
    mov ecx, buffer
    int 0x80
    jmp .loop_baca

  .tutup:
    mov eax, 8              ; SYS_CLOSE
    mov ebx, [fd]
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

  .error_buka:
    mov eax, 4
    mov ebx, 2              ; stderr
    mov ecx, pesan_error
    mov edx, panjang_error
    int 0x80

    mov eax, 1
    mov ebx, 1
    int 0x80

segment readable

namafile      db '/home/tes.txt', 0
pesan_error   db 'Error: tidak bisa membuka file', 0xA
panjang_error = $ - pesan_error

segment readable writeable

fd      dd 0
buffer  rb 512
```

---

### Contoh 5 — Subrutin dengan passing argumen lewat stack

```asm
format ELF executable 3
entry start

segment readable executable

; tambah_angka(a, b) -> hasil di eax
; dipanggil dengan: push b; push a; call tambah_angka
tambah_angka:
    push ebp
    mov  ebp, esp
    mov  eax, [ebp+8]   ; a (argumen pertama)
    add  eax, [ebp+12]  ; b (argumen kedua)
    pop  ebp
    ret  8              ; pop 8 byte argumen saat kembali

start:
    push 25             ; b
    push 17             ; a
    call tambah_angka   ; eax = 17 + 25 = 42

    ; cetak digit hasil
    add al, '0'
    mov [buf_hasil], al
    mov eax, 4
    mov ebx, 1
    mov ecx, buf_hasil
    mov edx, 1
    int 0x80

    mov eax, 4
    mov ebx, 1
    mov ecx, baris_baru
    mov edx, 1
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable
    baris_baru db 0xA

segment readable writeable
    buf_hasil rb 4
```

---

## 19. Kesalahan Umum

### `illegal instruction`

Kesalahan paling sering. Biasanya karena memakai direktif yang salah untuk format yang dipilih:

| Jika format | Gunakan | Bukan |
|---|---|---|
| `format ELF executable` | `segment readable executable` | `section '.text' executable` |
| `format ELF` | `section '.text' executable` | `segment readable executable` |

```
test.asm [4]:
section '.text' executable
error: illegal instruction.
```

Perbaikan: Ganti `section` menjadi `segment` ketika menggunakan `format ELF executable`.

---

### `undefined symbol`

Label atau konstanta dipakai tapi tidak pernah didefinisikan.

```
error: undefined symbol 'panjang'.
```

Perbaikan: Pastikan simbol didefinisikan dan letaknya tepat setelah string:

```asm
pesan    db 'Halo', 0xA
panjang  = $ - pesan       ; harus tepat setelah string
```

---

### `value out of range`

Nilai langsung terlalu besar untuk operandnya:

```asm
mov al, 256     ; ERROR — al adalah 8-bit, nilai maks 255
mov al, 0xFF    ; OK
```

---

### `No such file or directory` saat menjalankan `./program`

Tiga kemungkinan penyebab:
1. **Kamu menjalankan `./program.o`** — ekstensi `.o` berarti itu object file relocatable, bukan executable. Gunakan `format ELF executable 3` dan tentukan nama output: `fasm source.asm program`
2. **Path interpreter ELF tidak ada** — nusaOS punya dynamic linker sendiri. Pastikan binary kamu statically linked (murni syscall, tanpa libc).
3. **Tidak ada izin eksekusi** — FASM biasanya mengatur ini, tapi cek dengan `ls -l`.

---

### Segmentation fault / crash

Penyebab umum:
- Mengakses memori sebelum mengatur alamat yang valid (misalnya lupa `mov ecx, buffer` sebelum menggunakan `[ecx]`)
- Ketidakseimbangan stack — jumlah `push`/`pop` tidak cocok
- Pembagian dengan nol — selalu cek sebelum `div`
- `edx` tidak dinolkan sebelum `div` — menyebabkan overflow pembagian

---

## 20. Referensi Cepat Instruksi

### Perpindahan data

| Instruksi | Keterangan |
|-----------|------------|
| `mov dst, src` | Salin src ke dst |
| `lea dst, [expr]` | Muat alamat efektif |
| `xchg a, b` | Tukar a dan b |
| `movzx dst, src` | Pindah, perluas nol |
| `movsx dst, src` | Pindah, perluas tanda |
| `push src` | Masukkan ke stack |
| `pop dst` | Ambil dari stack |

### Aritmetika

| Instruksi | Keterangan |
|-----------|------------|
| `add dst, src` | dst = dst + src |
| `sub dst, src` | dst = dst - src |
| `mul src` | edx:eax = eax * src |
| `imul dst, src` | dst = dst * src (bertanda) |
| `div src` | eax = edx:eax / src |
| `inc dst` | dst = dst + 1 |
| `dec dst` | dst = dst - 1 |
| `neg dst` | dst = -dst |

### Logika & perbandingan

| Instruksi | Keterangan |
|-----------|------------|
| `and dst, src` | AND bitwise |
| `or dst, src` | OR bitwise |
| `xor dst, src` | XOR bitwise |
| `not dst` | NOT bitwise |
| `shl dst, n` | Geser kiri |
| `shr dst, n` | Geser kanan (unsigned) |
| `sar dst, n` | Geser kanan (bertanda) |
| `cmp a, b` | Set flag untuk a - b |
| `test a, b` | Set flag untuk a AND b |

### Alur kontrol

| Instruksi | Keterangan |
|-----------|------------|
| `jmp label` | Lompat tanpa syarat |
| `je / jz` | Lompat jika sama / nol |
| `jne / jnz` | Lompat jika tidak sama / tidak nol |
| `jl / jg` | Lompat jika kurang / lebih (bertanda) |
| `jle / jge` | Lompat jika ≤ / ≥ (bertanda) |
| `jb / ja` | Lompat jika di bawah / di atas (unsigned) |
| `call label` | Panggil subrutin |
| `ret` | Kembali dari subrutin |
| `loop label` | Kurangi ecx, lompat jika ≠ 0 |

### String

| Instruksi | Keterangan |
|-----------|------------|
| `movsb/w/d` | Salin [esi] ke [edi], majukan keduanya |
| `stosb/w/d` | Simpan al/ax/eax ke [edi], majukan |
| `lodsb/w/d` | Muat [esi] ke al/ax/eax, majukan |
| `scasb/w/d` | Bandingkan al/ax/eax dengan [edi] |
| `rep` | Ulangi instruksi berikutnya ecx kali |
| `cld` | Bersihkan direction flag (maju) |
| `std` | Set direction flag (mundur) |
