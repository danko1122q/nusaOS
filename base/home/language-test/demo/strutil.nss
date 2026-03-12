// strutil.nss — string utility module for nusaOS

global str  NEWLINE    = "\n"
global str  SEPARATOR  = "=========="
global int  MAX_LEN    = 254

// Return the length of string s
func str_len s -> length
    let length = 0
    len length s
endfunc

// Append string b onto dst  (dst = dst + b)
func str_append dst b -> dst
    concat dst b
endfunc
