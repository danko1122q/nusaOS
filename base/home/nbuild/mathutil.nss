// mathutil.nss — NSA module untuk demo nbuild
// Berisi fungsi-fungsi matematika dan utilitas

global int  VERSION   = 1
global str  MOD_NAME  = "mathutil"
global bool LOADED    = true

// Hitung nilai mutlak (absolute value)
// Catatan: cmp dengan literal negatif tidak didukung langsung,
// jadi kita bandingkan n < 0 via variabel sementara zero
func math_abs n -> result
    let result = 0
    copy result n
    let zero = 0
    cmp is_neg n < zero
    if is_neg then
        neg result
    end
endfunc

// Hitung nilai maksimum dari dua angka
func math_max a b -> result
    let result = 0
    copy result a
    cmp b_bigger b > a
    if b_bigger then
        copy result b
    end
endfunc

// Hitung nilai minimum dari dua angka
func math_min a b -> result
    let result = 0
    copy result a
    cmp a_bigger a > b
    if a_bigger then
        copy result b
    end
endfunc

// Hitung pangkat: base^exp (exp >= 0)
func math_pow base exp -> result
    let result = 1
    loop while exp != 0
        mul result base
        dec exp
    end
endfunc

// Cek apakah bilangan genap: result = true jika n genap
func math_is_even n -> result
    let result = false
    let rem = 0
    copy rem n
    mod rem 2
    let zero = 0
    cmp result rem == zero
endfunc

// Hitung faktorial n!
func math_factorial n -> result
    let result = 1
    loop while n != 0
        mul result n
        dec n
    end
endfunc

// Clamp: kembalikan val dalam rentang [lo, hi]
func math_clamp val lo hi -> result
    let result = 0
    copy result val
    cmp too_low val < lo
    if too_low then
        copy result lo
    end
    cmp too_high result > hi
    if too_high then
        copy result hi
    end
endfunc
