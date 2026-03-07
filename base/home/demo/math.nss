// math.nss — basic math module for nusaOS
// Provides common math functions and constants

global int PI_X100  = 314
global int E_X100   = 271
global int VERSION  = 1

// Return the larger of two integers
func max a b -> result
    let result = b
    cmp is_greater a > b
    if is_greater then
        copy result a
    end
endfunc

// Return the smaller of two integers
func min a b -> result
    let result = b
    cmp is_smaller a < b
    if is_smaller then
        copy result a
    end
endfunc

// Return the sum of two integers  (add is a keyword, use math_add)
func math_add a b -> result
    let result = a
    add result b
endfunc

// Return a minus b
func math_sub a b -> result
    let result = a
    sub result b
endfunc

// Return the product of two integers
func math_mul a b -> result
    let result = a
    mul result b
endfunc

// Return the absolute value of n
func abs n -> result
    let result = n
    cmp is_negative n < 0
    if is_negative then
        neg result
    end
endfunc
