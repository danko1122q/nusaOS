# NSA Language — Panduan Lengkap

NSA adalah bahasa pemrograman yang berjalan langsung di atas NusaOS. Kamu tulis kode di file `.nsa`, kompilasi jadi bytecode `.nbin`, lalu jalankan dengan runtime NSA. Satu binary untuk segalanya — tidak ada dependency eksternal, tidak ada setup rumit.

Panduan ini ditulis untuk semua orang, termasuk yang belum pernah coding sebelumnya.

---

## Daftar Isi

1. [Instalasi & Cara Mulai](#1-instalasi--cara-mulai)
2. [Menulis Program Pertama](#2-menulis-program-pertama)
3. [Variabel — Menyimpan Data](#3-variabel--menyimpan-data)
4. [Menampilkan Output](#4-menampilkan-output)
5. [Menerima Input dari Pengguna](#5-menerima-input-dari-pengguna)
6. [Aritmetika](#6-aritmetika)
7. [Perbandingan & Logika](#7-perbandingan--logika)
8. [Operasi String](#8-operasi-string)
9. [Percabangan — if / else](#9-percabangan--if--else)
10. [Perulangan — loop](#10-perulangan--loop)
11. [Fungsi](#11-fungsi)
12. [Array](#12-array)
13. [Contoh Program Lengkap](#13-contoh-program-lengkap)
14. [Format File .nbin](#14-format-file-nbin)
15. [Referensi Error](#15-referensi-error)
16. [Ringkasan Semua Keyword](#16-ringkasan-semua-keyword)

---

## 1. Instalasi & Cara Mulai

Semua perintah NSA ada dalam satu binary bernama `nsa`.

```sh
# Kompilasi file .nsa menjadi .nbin
nsa build program.nsa

# Kompilasi dengan nama output manual
nsa build program.nsa hasil.nbin

# Jalankan program
nsa run program.nbin

# Tampilkan bantuan
nsa help

# Tampilkan versi
nsa version
```

Proses menulis program NSA selalu tiga langkah:

```
Tulis kode (.nsa)  →  nsa build  →  nsa run
```

---

## 2. Menulis Program Pertama

Buat file bernama `halo.nsa` dengan isi berikut:

```
// halo.nsa
print "Halo, NusaOS!"
```

Simpan, lalu jalankan:

```sh
nsa build halo.nsa
nsa run halo.nbin
```

Output:

```
Halo, NusaOS!
```

Selamat, program pertama berhasil.

### Aturan dasar penulisan

- Satu pernyataan per baris
- Tidak ada titik koma di akhir baris
- Tidak ada kurung kurawal
- Indentasi tidak wajib, tapi sangat dianjurkan agar kode mudah dibaca
- Huruf kecil dan besar dibedakan — `Print` bukan keyword, yang benar adalah `print`

### Komentar

Baris yang diawali `//` atau `#` adalah komentar — tidak dieksekusi.

```
// ini komentar gaya C
# ini juga komentar

print "baris ini dijalankan"
# print "baris ini tidak dijalankan"
```

---

## 3. Variabel — Menyimpan Data

Variabel adalah tempat menyimpan data sementara selama program berjalan. Di NSA, semua variabel harus dideklarasikan dulu dengan keyword `let` sebelum bisa dipakai.

### Tiga tipe data

| Tipe | Contoh | Keterangan |
|------|--------|------------|
| `int` | `42`, `-7`, `0` | Bilangan bulat |
| `string` | `"NusaOS"`, `"halo"` | Teks |
| `bool` | `true`, `false` | Nilai benar/salah |

### Integer

```
let x = 10
let tahun = 2026
let suhu = -5
let nol = 0
```

Rentang nilai: **-2,147,483,648** sampai **2,147,483,647** (integer 32-bit).

### String

```
let nama = "danko1122q"
let pesan = "Selamat datang di NusaOS!"
let kosong = ""
```

Panjang maksimal string: **254 karakter**.

Escape sequence yang bisa dipakai di dalam string:

| Penulisan | Artinya |
|-----------|---------|
| `\n` | Baris baru |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Tanda kutip ganda |
| `\'` | Tanda kutip tunggal |
| `\0` | Null byte |

Contoh:

```
let pesan = "Baris pertama\nBaris kedua"
print pesan
```

Output:
```
Baris pertama
Baris kedua
```

### Bool

```
let aktif = true
let selesai = false
```

Bool dicetak sebagai `true` atau `false`, bukan `1` atau `0`.

### Mengubah nilai variabel

Kamu bisa mengubah nilai dengan `let` lagi, selama tipenya sama:

```
let skor = 0
let skor = 100    // valid — skor sekarang 100
```

Tapi tidak bisa mengubah tipe:

```
let x = 10
let x = "teks"   // ERROR: x sudah dideklarasikan sebagai int
```

### Menyalin nilai antar variabel

```
let a = 42
let b = a      // b sekarang bernilai 42, tipe mengikuti a

copy dst src   // cara eksplisit — bekerja untuk semua tipe
```

Contoh `copy`:

```
let nilai = 99
let cadangan = 0
copy cadangan nilai   // cadangan = 99
```

Maksimal variabel per program: **200**.

---

## 4. Menampilkan Output

### print — dengan baris baru

Menampilkan nilai diikuti newline otomatis di akhir.

```
print "Halo!"
print 42
print nama
print aktif
```

Bisa mencetak variabel jenis apapun — int, string, maupun bool.

### println — tanpa baris baru

Menampilkan nilai **tanpa** newline di akhir. Berguna untuk membuat prompt di baris yang sama dengan input pengguna.

```
println "Masukkan nama: "
input nama
println "Halo, "
print nama
```

Output (tanpa jeda baris antara prompt dan jawaban):

```
Masukkan nama: Dava
Halo, Dava
```

Perbedaan `print` vs `println`:

```
print "A"
print "B"
```
Output:
```
A
B
```

```
println "A"
println "B"
```
Output:
```
AB
```

---

## 5. Menerima Input dari Pengguna

Keyword `input` membaca data dari keyboard ke dalam variabel.

```
input x
```

Perilakunya mengikuti tipe variabel:

- Variabel **int** → membaca angka
- Variabel **string** → membaca satu baris teks
- Variabel **belum dideklarasikan** → otomatis dibuat sebagai string

Contoh membaca angka:

```
let umur = 0
println "Umur kamu: "
input umur
println "Kamu berumur "
print umur
```

Contoh membaca teks:

```
let nama = ""
println "Nama kamu: "
input nama
println "Halo, "
print nama
```

Contoh variabel belum dideklarasikan:

```
println "Kota asal: "
input kota       // kota otomatis jadi string
print kota
```

> **Catatan:** `input` ke variabel bool tidak didukung. Gunakan int (0/1) lalu konversi secara manual jika diperlukan.

---

## 6. Aritmetika

Operasi matematika hanya bekerja pada variabel integer. Hasilnya selalu disimpan kembali ke variabel tujuan.

### Operasi dasar

```
add x 10      // x = x + 10
sub x 5       // x = x - 5
mul x 3       // x = x * 3
div x 2       // x = x / 2
mod x 7       // x = x % 7  (sisa bagi)
```

Operand kedua bisa berupa angka langsung atau variabel lain:

```
let a = 20
let b = 6

add a b       // a = 26
sub a b       // a = 14  (dari nilai sebelumnya)
mul a 2       // a = 52
div a b       // a = 4   (pembagian integer, sisa dibuang)
mod a b       // a = 2
```

### Increment dan decrement

Cara cepat menambah atau mengurangi 1:

```
let i = 0
inc i         // i = 1
inc i         // i = 2
dec i         // i = 1
```

### Negasi

```
let x = 5
neg x         // x = -5
neg x         // x = 5  (dua kali neg kembali ke semula)
```

### Hal yang perlu diingat

Pembagian di NSA adalah **pembagian integer** — bagian desimal langsung dibuang:

```
let a = 7
div a 2       // a = 3, bukan 3.5
```

Membagi dengan nol menyebabkan runtime error:

```
div a 0       // runtime error: division/modulo by zero
mod a 0       // runtime error: division/modulo by zero
```

Operasi aritmetika tidak bisa dilakukan pada string atau bool.

---

## 7. Perbandingan & Logika

### cmp — membandingkan dua variabel

`cmp` membandingkan dua variabel dan menyimpan hasilnya sebagai bool. Berfungsi untuk **integer** maupun **string** — tipe operand menentukan jenis perbandingan yang digunakan.

**Perbandingan integer** — semua enam operator tersedia:

```
cmp hasil a == b    // hasil = (a sama dengan b?)
cmp hasil a != b    // hasil = (a tidak sama dengan b?)
cmp hasil a <  b    // hasil = (a lebih kecil dari b?)
cmp hasil a >  b    // hasil = (a lebih besar dari b?)
cmp hasil a <= b    // hasil = (a lebih kecil atau sama dengan b?)
cmp hasil a >= b    // hasil = (a lebih besar atau sama dengan b?)
```

Contoh:

```
let a = 10
let b = 20
let lebih_besar = false

cmp lebih_besar b > a    // true, karena 20 > 10
print lebih_besar         // mencetak: true
```

**Perbandingan string** — hanya `==` dan `!=` yang didukung:

```
let s1 = "halo"
let s2 = "dunia"
let s3 = "halo"

cmp sama s1 == s3    // true  — teks sama
cmp beda s1 != s2    // true  — teks berbeda
```

Keyword `cmp` yang sama berlaku untuk keduanya — NSA mendeteksi tipe secara otomatis. Mencampur string dan integer dalam satu `cmp` adalah error.

### not — membalik nilai

Membalik bool antara `true` dan `false`, atau int antara `0` dan `1`:

```
let aktif = true
not aktif         // aktif = false
not aktif         // aktif = true  (kembali lagi)

let x = 1
not x             // x = 0
not x             // x = 1
```

### and / or — logika ganda

```
and hasil a b    // hasil = a DAN b (keduanya harus true)
or  hasil a b    // hasil = a ATAU b (salah satu cukup)
```

Contoh:

```
let cukup_umur = true
let punya_id = false

and bisa_masuk cukup_umur punya_id    // false — keduanya harus true
or  salah_satu cukup_umur punya_id    // true — salah satu sudah cukup

print bisa_masuk    // false
print salah_satu    // true
```

### Aturan truthy (kapan dianggap "true")

| Tipe | Dianggap false jika |
|------|---------------------|
| int | nilainya `0` |
| bool | nilainya `false` |
| string | string kosong `""` |

Artinya semua nilai selain itu dianggap true.

---

## 8. Operasi String

### concat — menggabungkan string

Menambahkan teks ke ujung variabel string.

```
let kalimat = "Halo"
concat kalimat ", dunia"    // kalimat = "Halo, dunia"
concat kalimat "!"          // kalimat = "Halo, dunia!"
print kalimat
```

Bisa juga menggabungkan dua variabel string:

```
let depan = "NusaOS"
let depan = "Halo"
let belakang = ", NusaOS!"
concat depan belakang       // depan = "Halo, NusaOS!"
```

### len — panjang string

Menyimpan jumlah karakter string ke variabel integer.

```
let teks = "halo"
let panjang = 0
len panjang teks    // panjang = 4
print panjang
```

Kalau variabel tujuan belum dideklarasikan, otomatis dibuat sebagai int:

```
let nama = "NusaOS"
len n nama    // n otomatis jadi int, nilainya 6
```

### to_str — integer ke string

```
let angka = 2026
let teks = ""
to_str teks angka    // teks = "2026"
```

### to_int — string ke integer

```
let input_user = "42"
let nilai = 0
to_int nilai input_user    // nilai = 42
```

Kalau string bukan angka yang valid, hasilnya `0`.

### Contoh gabungan — membangun kalimat dinamis

```
let nama = ""
let umur = 0

println "Nama: "
input nama
println "Umur: "
input umur

let pesan = "Halo, "
concat pesan nama
concat pesan "! Umur kamu "

let umur_str = ""
to_str umur_str umur
concat pesan umur_str
concat pesan " tahun."

print pesan
```

Output jika input `Dava` dan `20`:

```
Halo, Dava! Umur kamu 20 tahun.
```

---

## 9. Percabangan — if / else

Percabangan memungkinkan program mengambil keputusan berbeda tergantung kondisi.

### Bentuk dasar

```
if kondisi then
    // kode dijalankan kalau kondisi benar
end
```

Setiap `if` **wajib** ditutup dengan `end`.

### Dengan else

```
if kondisi then
    // dijalankan kalau kondisi benar
else
    // dijalankan kalau kondisi salah
end
```

### Kondisi yang bisa dipakai

**Membandingkan variabel dengan angka:**

```
if x == 10 then ... end
if x != 0  then ... end
if x <  5  then ... end
if x >  5  then ... end
if x <= 10 then ... end
if x >= 10 then ... end
```

**Truthy test — cek apakah variabel bernilai "benar":**

```
if aktif then
    print "aktif!"
end

if nama then
    print "nama sudah diisi"
end
```

### Contoh if/else sederhana

```
let nilai = 75

if nilai >= 70 then
    print "Lulus"
else
    print "Tidak lulus"
end
```

### if bertingkat

```
let nilai = 85

if nilai >= 90 then
    print "A"
else
    if nilai >= 80 then
        print "B"
    else
        if nilai >= 70 then
            print "C"
        else
            print "D"
        end
    end
end
```

Indentasi di sini sangat membantu keterbacaan, meski tidak wajib.

---

## 10. Perulangan — loop

Perulangan menjalankan sekelompok baris kode berulang kali.

### loop N times — perulangan tetap

Menjalankan blok tepat sebanyak N kali. N harus angka langsung (bukan variabel).

```
loop 5 times
    print "halo"
end
```

Output:
```
halo
halo
halo
halo
halo
```

Compiler mengelola counter-nya sendiri, kamu tidak perlu membuat variabel penghitung.

### loop while — perulangan bersyarat

Menjalankan blok selama kondisi masih benar. Kondisi dicek **sebelum** masuk ke blok.

**Dengan perbandingan:**

```
let i = 1
loop while i <= 5
    print i
    inc i
end
```

Output:
```
1
2
3
4
5
```

**Dengan truthy test:**

```
let jalan = true
let langkah = 0

loop while jalan
    inc langkah
    if langkah == 3 then
        let jalan = false
    end
end

print langkah
```

Output:
```
3
```

### Loop bersarang

Loop bisa ditaruh di dalam loop lain:

```
let baris = 1
loop while baris <= 3
    let kolom = 1
    loop while kolom <= 3
        println "* "
    end
    print ""
    inc baris
end
```

Output:
```
* * * 
* * * 
* * * 
```

> **Catatan:** VM memiliki batas **10 juta iterasi** per eksekusi untuk mencegah infinite loop menghang NusaOS. Kalau terlampaui, program berhenti dengan pesan error.

---

## 11. Fungsi

Fungsi adalah cara mengelompokkan kode yang bisa dipanggil berkali-kali dari mana saja. Ini membuat program lebih terstruktur dan tidak mengulang kode yang sama.

### Mendefinisikan fungsi

```
func nama_fungsi
    // isi fungsi
endfunc
```

Setiap fungsi dibuka dengan `func` dan ditutup dengan `endfunc`.

### Fungsi dengan parameter

Parameter adalah variabel yang dikirim dari pemanggil ke fungsi.

```
func sapa nama
    println "Halo, "
    print nama
endfunc
```

### Memanggil fungsi

```
let pengguna = "Dava"
call sapa pengguna
```

Parameter dikirim berurutan — posisi pertama di `call` masuk ke parameter pertama di `func`.

### Fungsi dengan nilai kembalian

Tambahkan `->` diikuti nama variabel untuk mengembalikan nilai:

```
func tambah a b -> hasil
    let hasil = 0
    add hasil a
    add hasil b
endfunc
```

Memanggil dan menangkap hasilnya:

```
let x = 10
let y = 25
call tambah x y -> jumlah
print jumlah    // 35
```

### return — keluar lebih awal

Pakai `return` untuk berhenti dari fungsi sebelum mencapai `endfunc`:

```
func cek_positif n
    if n > 0 then
        print "positif"
        return
    end
    print "nol atau negatif"
endfunc
```

### Variabel lokal

Semua variabel yang dibuat di dalam fungsi bersifat **lokal** — tidak terlihat dari luar fungsi, dan tidak mengganggu variabel global dengan nama yang sama.

```
let x = 100     // variabel global

func contoh a
    let x = 999     // ini variabel LOKAL, berbeda dari x global
    print x         // mencetak 999
endfunc

call contoh x
print x             // masih mencetak 100, tidak berubah
```

### Batasan fungsi

- Fungsi tidak bisa didefinisikan di dalam fungsi lain (tidak ada nested func)
- Maksimal 64 variabel lokal per fungsi
- Maksimal kedalaman pemanggilan bersarang: 64 tingkat
- Fungsi harus sudah didefinisikan sebelum dipanggil — atau di atas `call`-nya

### Contoh lengkap dengan beberapa fungsi

```
// math_utils.nsa

func kuadrat n -> hasil
    let hasil = 0
    copy hasil n
    mul hasil n
endfunc

func mutlak n -> hasil
    let hasil = 0
    copy hasil n
    if n < 0 then
        neg hasil
    end
endfunc

func maks a b -> hasil
    let lebih = false
    copy hasil a
    cmp lebih b > a
    if lebih then
        copy hasil b
    end
endfunc

// --- main ---
let angka = 7
call kuadrat angka -> kuadrat_7
println "7^2 = "
print kuadrat_7         // 49

let negatif = -15
call mutlak negatif -> abs_val
println "|(-15)| = "
print abs_val           // 15

let p = 30
let q = 42
call maks p q -> terbesar
println "max(30,42) = "
print terbesar          // 42
```

---

---

## 12. Array

Array adalah daftar nilai yang tersimpan berurutan di bawah satu nama — semua elemennya bertipe sama.

### Deklarasi array

```
arr int  skor   5    // array integer, 5 elemen, inisialisasi ke 0
arr str  nama   3    // array string, 3 elemen, inisialisasi ke ""
arr bool bendera 4   // array bool, 4 elemen, inisialisasi ke false
```

Sintaks: `arr <tipe> <n> <ukuran>`

- Tipe harus `int`, `str`, atau `bool`
- Ukuran harus bilangan bulat positif, maksimal **64**
- Array bersifat global — tidak bisa dideklarasikan di dalam fungsi

### Mengisi elemen — aset

```
arr int skor 5

aset skor 0 100    // skor[0] = 100
aset skor 1 85     // skor[1] = 85

let i = 2
let val = 92
aset skor i val    // skor[2] = 92  — indeks dan nilai dari variabel
```

Indeks bisa berupa variabel atau literal. Literal dicek batas di compile time. Variabel dicek batas di runtime.

### Membaca elemen — aget

```
aget x skor 0      // x = skor[0]  — indeks literal

let i = 1
aget x skor i      // x = skor[1]  — indeks dari variabel
```

Variabel tujuan dibuat otomatis jika belum ada, dengan tipe elemen array tersebut.

### Mendapatkan ukuran — alen

```
alen n skor    // n = 5  (ukuran yang dideklarasikan)
```

### Iterasi array

```
arr int skor 5
aset skor 0 100
aset skor 1 85
aset skor 2 92
aset skor 3 78
aset skor 4 95

let i = 0
loop while i < 5
    aget val skor i
    print val
    inc i
end
```

### Menjumlahkan semua elemen

```
let total = 0
let i = 0
loop while i < 5
    aget x skor i
    add total x
    inc i
end
print total    // 450
```

### Mencari nilai dalam array string

```
arr str buah 3
aset buah 0 "apel"
aset buah 1 "pisang"
aset buah 2 "ceri"

let target = "pisang"
let ditemukan = false
let i = 0
loop while i < 3
    aget item buah i
    cmp cocok item == target
    if cocok then
        let ditemukan = true
    end
    inc i
end

if ditemukan then
    print "pisang ditemukan!"
end
```

### Batasan

- Maksimal elemen per array: **64**
- Maksimal total variabel (termasuk slot array): **200**
- Array tidak didukung di dalam fungsi

---

## 13. Contoh Program Lengkap

### Kalkulator interaktif

```
// calc.nsa — kalkulator sederhana

func do_add a b -> result
    let result = 0
    add result a
    add result b
endfunc

func do_sub a b -> result
    let result = 0
    copy result a
    sub result b
endfunc

func do_mul a b -> result
    let result = 0
    copy result a
    mul result b
endfunc

func do_div a b -> result
    let result = 0
    copy result a
    div result b
endfunc

let running = 1
let a = 0
let b = 0
let op = 0
let jawaban = 0

print "=== Kalkulator NSA ==="

loop while running != 0
    print ""
    print "1 = Tambah"
    print "2 = Kurang"
    print "3 = Kali"
    print "4 = Bagi"
    print "0 = Keluar"
    println "Pilih: "
    input op

    if op == 0 then
        let running = 0
    end

    if op == 1 then
        println "Angka pertama: "
        input a
        println "Angka kedua: "
        input b
        call do_add a b -> jawaban
        println "Hasil: "
        print jawaban
    end

    if op == 2 then
        println "Angka pertama: "
        input a
        println "Angka kedua: "
        input b
        call do_sub a b -> jawaban
        println "Hasil: "
        print jawaban
    end

    if op == 3 then
        println "Angka pertama: "
        input a
        println "Angka kedua: "
        input b
        call do_mul a b -> jawaban
        println "Hasil: "
        print jawaban
    end

    if op == 4 then
        println "Angka pertama: "
        input a
        println "Angka kedua: "
        input b
        call do_div a b -> jawaban
        println "Hasil: "
        print jawaban
    end
end

print "Sampai jumpa!"
```

---

### FizzBuzz (1–20)

```
// fizzbuzz.nsa
let i = 1
loop while i <= 20
    let fm = 0
    let bm = 0
    copy fm i
    copy bm i
    mod fm 3
    mod bm 5

    if fm == 0 then
        if bm == 0 then
            print "FizzBuzz"
        else
            print "Fizz"
        end
    else
        if bm == 0 then
            print "Buzz"
        else
            print i
        end
    end

    inc i
end
```

---

### Faktorial dengan fungsi

```
// faktorial.nsa
func faktorial n -> hasil
    let hasil = 1
    loop while n != 0
        mul hasil n
        dec n
    end
endfunc

let angka = 6
call faktorial angka -> hasil
println "6! = "
print hasil    // 720
```

---

### Greeter interaktif

```
// greeter.nsa
let nama = ""
let kota = ""
let umur = 0

print "================================"
print "   Selamat datang di NusaOS!"
print "================================"

println "Nama kamu: "
input nama

println "Dari mana: "
input kota

println "Umur: "
input umur

print "--------------------------------"

let pesan = "Halo, "
concat pesan nama
concat pesan "!"
print pesan

let info = "Dari "
concat info kota
concat info ", umur "
let umur_str = ""
to_str umur_str umur
concat info umur_str
concat info " tahun."
print info

print "--------------------------------"
print "Program ini ditulis dalam NSA"
print "berjalan native di NusaOS."
```

---

### Pangkat dua (input dari pengguna)

```
// pangkat2.nsa
let basis = 0
let eksponen = 0
let hasil = 1

println "Basis: "
input basis
println "Eksponen: "
input eksponen

let exp_asli = 0
copy exp_asli eksponen

loop while eksponen != 0
    mul hasil basis
    dec eksponen
end

println "Hasilnya: "
print hasil
```

---

## 14. Format File .nbin

File `.nbin` adalah bytecode hasil kompilasi. Strukturnya:

```
Offset   Ukuran   Isi
------   ------   ---
0        6        Magic: 7F 4E 53 41 02 00  (\x7fNSA\x02\x00)
6        1        Jumlah variabel (uint8)
7        2        Ukuran bytecode dalam bytes (little-endian uint16)
9        N        Bytecode
```

Magic bytes berbeda dari NusaLang v1 (`\x7fNUSA\x01`) secara sengaja — file lama akan ditolak dengan pesan error yang jelas, bukan diam-diam salah berjalan.

Kamu tidak perlu memahami format ini untuk menulis program NSA. Ini hanya relevan jika kamu ingin membuat tools yang membaca `.nbin` secara langsung.

---

## 15. Referensi Error

### Error saat kompilasi (nsa build)

```
program.nsa:5: error: undeclared variable 'x'
```
Variabel `x` belum dideklarasikan dengan `let` sebelum dipakai.

```
program.nsa:3: error: 'skor' was previously declared as int, cannot redeclare as string
```
Tipe variabel tidak bisa diubah setelah dideklarasikan.

```
program.nsa:8: error: unclosed 'if' block (missing 'end')
```
Ada `if` yang tidak punya pasangan `end`.

```
program.nsa:12: error: division/modulo by zero
```
Kompiler mendeteksi pembagian dengan literal `0`.

```
program.nsa:1: error: 'let' is a reserved keyword
```
Nama variabel tidak boleh sama dengan keyword bahasa.

```
program.nsa:20: error: function 'tambah' expects 2 argument(s), got 1
```
Jumlah argumen saat `call` tidak sesuai dengan definisi `func`.

```
program.nsa: error: unclosed 'func' block (missing 'endfunc')
```
Ada `func` tanpa penutup `endfunc`.

### Error saat eksekusi (nsa run)

```
nsa run: runtime error at offset 42: division/modulo by zero
```
Program mencoba membagi dengan nol saat berjalan.

```
nsa run: runtime error at offset 17: ARITH_IMM: var 3 is not an integer
```
Operasi aritmetika dilakukan pada variabel yang bukan integer.

```
nsa run: runtime error at offset 0: infinite loop detected (limit 10M iterations)
```
Loop melebihi 10 juta iterasi.

```
nsa run: 'file.nbin': not a valid .nbin file (wrong magic or version)
```
File bukan hasil kompilasi NSA, atau dikompilasi oleh versi lama yang tidak kompatibel.

```
nsa run: 'file.nbin': No such file or directory
```
Nama file salah atau belum dikompilasi. Perhatikan huruf besar/kecil — NusaOS case-sensitive.

```
nsa run: runtime error at offset 9: call stack overflow (max depth 64)
```
Fungsi terlalu banyak memanggil fungsi lain secara bersarang (lebih dari 64 tingkat).

---

## 16. Ringkasan Semua Keyword

| Keyword | Fungsi |
|---------|--------|
| `let` | Deklarasi variabel |
| `copy` | Salin nilai antar variabel |
| `print` | Tampilkan nilai + newline |
| `println` | Tampilkan nilai tanpa newline |
| `input` | Baca nilai dari keyboard |
| `add` | Penjumlahan |
| `sub` | Pengurangan |
| `mul` | Perkalian |
| `div` | Pembagian integer |
| `mod` | Sisa bagi (modulo) |
| `inc` | Tambah 1 |
| `dec` | Kurang 1 |
| `neg` | Negasi (balik tanda) |
| `not` | Balik bool/int (0↔1) |
| `cmp` | Bandingkan dua integer atau dua string → bool |
| `and` | Logika DAN |
| `or` | Logika ATAU |
| `concat` | Gabungkan string |
| `len` | Panjang string |
| `to_str` | Integer ke string |
| `to_int` | String ke integer |
| `arr` | Deklarasi array: `arr int skor 5` |
| `aget` | Ambil elemen: `aget dst nama_array idx` |
| `aset` | Isi elemen: `aset nama_array idx nilai` |
| `alen` | Ukuran array: `alen n nama_array` |
| `if` | Mulai percabangan |
| `then` | Penanda kondisi if |
| `else` | Cabang alternatif |
| `end` | Tutup blok if/loop |
| `loop` | Mulai perulangan |
| `while` | Perulangan bersyarat |
| `times` | Perulangan N kali |
| `func` | Definisikan fungsi |
| `endfunc` | Tutup definisi fungsi |
| `return` | Keluar dari fungsi lebih awal |
| `call` | Panggil fungsi |
| `true` | Nilai bool benar |
| `false` | Nilai bool salah |

---

## Tentang Versi

Untuk melihat versi NSA yang terpasang di sistem kamu, jalankan:

```sh
nsa version
```

Bahasa ini terus dikembangkan sebagai bagian dari proyek NusaOS. Dokumentasi ini mencakup semua fitur yang saat ini didukung — jika suatu fitur tercantum di sini, berarti fitur tersebut berfungsi di versi yang terpasang.

---

*NSA adalah bagian dari proyek NusaOS — sistem operasi buatan sendiri yang ditulis dari nol dalam C++.*