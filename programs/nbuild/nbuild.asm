; nbuild v1.1 — nusaOS Native Build System

format ELF
public main

macro ccall proc,[arg]
  { common
     local size
     size = 0
     mov ebp,esp
     if ~ arg eq
    forward
     size = size + 4
    common
     sub esp,size
     end if
     and esp,-16
     if ~ arg eq
     add esp,size
    reverse
     push dword arg
    common
     end if
     call proc
     mov esp,ebp }

; ── libc imports (identik dengan SYSTEM.INC) ─────────────────────────────────
extrn malloc
extrn free
extrn fopen
extrn fclose
extrn fread
extrn fwrite
extrn fgets
extrn feof
extrn remove
extrn system
extrn exit
extrn strlen
extrn strcpy
extrn strncpy
extrn strcmp
extrn strncmp
extrn strchr
extrn memcpy
extrn memset
extrn 'write'  as libc_write

; ── Constants ─────────────────────────────────────────────────────────────────
NB_MAX_VARS     = 64
NB_VAR_NAME_LEN = 32
NB_VAR_VAL_LEN  = 256
NB_LINE_LEN     = 1024
NB_CMD_LEN      = 2048

STDOUT = 1
STDERR = 2


section '.text' executable align 16


main:
        mov     ecx,[esp+4]
        mov     [g_argc],ecx
        mov     ebx,[esp+8]
        mov     [g_argv],ebx
        push    ebp
        mov     [g_stack],esp

        ; con_handle = 1 (stdout) — sama persis dengan FASM.ASM
        mov     [con_handle],STDOUT

        mov     esi,str_banner
        call    display_string

        call    init_vars
        call    parse_cmdline
        test    eax,eax
        jz      .do_build
        cmp     eax,2
        je      .do_version

.do_help:
        mov     esi,str_usage
        call    display_string
        xor     eax,eax
        jmp     .exit

.do_version:
        mov     esi,str_version
        call    display_string
        xor     eax,eax
        jmp     .exit

.do_build:
        call    process_buildfile
        jc      .build_error

        mov     esi,str_done
        call    display_string
        xor     eax,eax
        jmp     .exit

.build_error:
        mov     [con_handle],STDERR
        mov     esi,str_build_failed
        call    display_string
        mov     eax,1

.exit:
        push    eax
        ccall   free,[additional_memory]
        pop     eax
        ccall   exit,eax
        mov     esp,[g_stack]
        pop     ebp
        ret


; display_string

display_string:
        lodsb
        or      al,al
        jz      .done
        mov     dl,al
        call    display_character
        jmp     display_string
.done:  ret

display_character:
        mov     [character],dl
        ccall   libc_write,[con_handle],character,1
        ret


; parse_cmdline

parse_cmdline:
        mov     ecx,[g_argc]
        mov     ebx,[g_argv]
        add     ebx,4
        dec     ecx
        jz      .use_default
.next_arg:
        mov     esi,[ebx]
        cmp     byte [esi],'-'
        jne     .bare_file
        inc     esi
        mov     al,[esi]
        cmp     al,'h'
        je      .help
        cmp     al,'H'
        je      .help
        cmp     al,'v'
        je      .version
        cmp     al,'V'
        je      .version
        cmp     al,'f'
        je      .flag_f
        cmp     al,'F'
        je      .flag_f
        jmp     .advance
.flag_f:
        add     ebx,4
        dec     ecx
        jz      .use_default
        mov     esi,[ebx]
        mov     [g_buildfile],esi
        jmp     .advance
.bare_file:
        mov     [g_buildfile],esi
        jmp     .advance
.help:
        mov     eax,1
        ret
.version:
        mov     eax,2
        ret
.advance:
        add     ebx,4
        dec     ecx
        jnz     .next_arg
        xor     eax,eax
        ret
.use_default:
        mov     dword [g_buildfile],default_buildfile
        xor     eax,eax
        ret


; init_memory — alokasi working memory (seperti FASM.ASM)

init_memory:
        mov     ecx,1000000h        ; 16 MB default
        ccall   malloc,ecx
        or      eax,eax
        jz      .oom
        mov     [additional_memory],eax
        ret
.oom:
        mov     esi,str_oom
        call    display_string
        ccall   exit,1
        ret


; init_vars — set built-in variables

init_vars:
        mov     dword [g_var_count],0
        ccall   malloc,1000000h
        or      eax,eax
        jz      .skip
        mov     [additional_memory],eax
.skip:
        mov     esi,bvar_nsa_name
        mov     edi,bvar_nsa_val
        call    set_var
        mov     esi,bvar_fasm_name
        mov     edi,bvar_fasm_val
        call    set_var
        mov     esi,bvar_outdir_name
        mov     edi,bvar_dot
        call    set_var
        mov     esi,bvar_srcdir_name
        mov     edi,bvar_dot
        call    set_var
        ret


; process_buildfile

process_buildfile:
        ccall   fopen,[g_buildfile],open_r
        test    eax,eax
        jz      .no_file
        mov     [g_nb_fp],eax
        mov     dword [g_lineno],0
.read_loop:
        ccall   fgets,line_buf,NB_LINE_LEN,[g_nb_fp]
        test    eax,eax
        jz      .eof
        inc     dword [g_lineno]
        call    strip_line
        call    dispatch_line
        jc      .line_error
        jmp     .read_loop
.eof:
        ccall   fclose,[g_nb_fp]
        clc
        ret
.line_error:
        ccall   fclose,[g_nb_fp]
        stc
        ret
.no_file:
        mov     [con_handle],STDERR
        mov     esi,str_err_open
        call    display_string
        mov     esi,[g_buildfile]
        call    display_string
        mov     dl,0xA
        call    display_character
        mov     [con_handle],STDOUT
        stc
        ret


; dispatch_line

dispatch_line:
        mov     esi,line_buf
        call    skip_ws
        mov     al,[esi]
        or      al,al
        jz      .ok
        cmp     al,'#'
        je      .ok
        cmp     al,';'
        je      .ok
        cmp     al,'@'
        je      .do_assign

        ; read keyword
        mov     edi,tok_buf
        call    read_token
        call    skip_ws

        ; expand rest of line
        mov     edi,exp_buf
        call    expand_vars

        ; compare keyword
        mov     edi,tok_buf
        mov     esi,kw_nsa
        call    str_eq
        jz      .do_nsa

        mov     edi,tok_buf
        mov     esi,kw_nss
        call    str_eq
        jz      .do_nss

        mov     edi,tok_buf
        mov     esi,kw_asm_kw
        call    str_eq
        jz      .do_asm

        mov     edi,tok_buf
        mov     esi,kw_run
        call    str_eq
        jz      .do_run

        mov     edi,tok_buf
        mov     esi,kw_echo
        call    str_eq
        jz      .do_echo

        mov     edi,tok_buf
        mov     esi,kw_clean
        call    str_eq
        jz      .do_clean

        ; unknown — warn
        mov     [con_handle],STDERR
        mov     esi,str_warn_unknown
        call    display_string
        mov     esi,tok_buf
        call    display_string
        mov     dl,0xA
        call    display_character
        mov     [con_handle],STDOUT
        jmp     .ok

.do_assign:
        inc     esi
        call    skip_ws
        mov     edi,tok_buf
        call    read_token_eq       ; read until '='
        cmp     byte [esi],'='
        jne     .ok
        inc     esi
        call    skip_ws
        mov     edi,exp_buf
        call    expand_vars
        mov     esi,tok_buf
        mov     edi,exp_buf
        call    set_var
        jmp     .ok

.do_nsa:
        mov     edi,cmd_buf
        mov     esi,str_nsa_key
        call    var_lookup_copy
        mov     esi,str_sp_build_sp
        call    copy_str_di
        mov     esi,exp_buf
        call    copy_str_di
        mov     byte [edi],0
        call    run_cmd
        jmp     .ok

.do_nss:
        mov     edi,cmd_buf
        mov     esi,str_nsa_key
        call    var_lookup_copy
        mov     esi,str_sp_build_sp
        call    copy_str_di
        mov     esi,exp_buf
        call    copy_str_di
        mov     byte [edi],0
        call    run_cmd
        jmp     .ok

.do_asm:
        mov     edi,cmd_buf
        mov     esi,str_fasm_key
        call    var_lookup_copy
        mov     esi,str_space
        call    copy_str_di
        mov     esi,exp_buf
        call    copy_str_di
        mov     byte [edi],0
        call    run_cmd
        jmp     .ok

.do_run:
        mov     edi,cmd_buf
        mov     esi,exp_buf
        call    copy_str_di
        mov     byte [edi],0
        call    run_cmd
        jmp     .ok

.do_echo:
        mov     esi,str_echo_pfx
        call    display_string
        mov     esi,exp_buf
        call    display_string
        mov     dl,0xA
        call    display_character
        jmp     .ok

.do_clean:
        mov     esi,exp_buf
.clean_loop:
        call    skip_ws_esi
        mov     al,[esi]
        or      al,al
        jz      .ok
        mov     edi,tok_buf
.clean_copy:
        mov     al,[esi]
        or      al,al
        jz      .clean_exec
        cmp     al,' '
        je      .clean_exec
        cmp     al,9
        je      .clean_exec
        stosb
        inc     esi
        jmp     .clean_copy
.clean_exec:
        mov     byte [edi],0
        push    esi
        mov     esi,str_clean_pfx
        call    display_string
        mov     esi,tok_buf
        call    display_string
        mov     dl,0xA
        call    display_character
        ccall   remove,tok_buf
        pop     esi
        jmp     .clean_loop

.ok:
        clc
        ret


; run_cmd — print + system()

run_cmd:
        mov     esi,str_run_pfx
        call    display_string
        mov     esi,cmd_buf
        call    display_string
        mov     dl,0xA
        call    display_character
        ccall   system,cmd_buf
        test    eax,eax
        jz      .ok
        mov     [con_handle],STDERR
        mov     esi,str_cmd_fail
        call    display_string
        mov     [con_handle],STDOUT
        stc
        ret
.ok:
        clc
        ret


; Variable table

set_var:
        ; esi=name, edi=value
        mov     [sv_name],esi
        mov     [sv_val],edi
        mov     ecx,[g_var_count]
        xor     ebx,ebx
        test    ecx,ecx
        jz      .new
.search:
        push    ecx
        mov     eax,ebx
        imul    eax,(NB_VAR_NAME_LEN+NB_VAR_VAL_LEN)
        add     eax,var_table
        mov     esi,[sv_name]
        mov     edi,eax
        call    strcmp_fasm
        pop     ecx
        jz      .write
        inc     ebx
        loop    .search
.new:
        mov     ebx,[g_var_count]
        cmp     ebx,NB_MAX_VARS
        jae     .full
        inc     dword [g_var_count]
.write:
        mov     eax,ebx
        imul    eax,(NB_VAR_NAME_LEN+NB_VAR_VAL_LEN)
        add     eax,var_table
        ccall   strncpy,eax,[sv_name],(NB_VAR_NAME_LEN-1)
        mov     eax,ebx
        imul    eax,(NB_VAR_NAME_LEN+NB_VAR_VAL_LEN)
        add     eax,var_table
        add     eax,NB_VAR_NAME_LEN
        ccall   strncpy,eax,[sv_val],(NB_VAR_VAL_LEN-1)
.full:
        ret

; var_lookup_copy: lookup $esi var name, copy value to [edi], advance edi
var_lookup_copy:
        push    esi
        call    var_value
        call    copy_str_di
        pop     esi
        ret

; var_value: esi=name → esi=value ptr
var_value:
        push    edi ecx ebx
        mov     ecx,[g_var_count]
        test    ecx,ecx
        jz      .notfound
        xor     ebx,ebx
.s:
        push    ecx
        mov     eax,ebx
        imul    eax,(NB_VAR_NAME_LEN+NB_VAR_VAL_LEN)
        add     eax,var_table
        mov     edi,eax
        call    strcmp_fasm
        pop     ecx
        jz      .found
        inc     ebx
        loop    .s
.notfound:
        mov     esi,str_empty
        pop     ebx ecx edi
        ret
.found:
        mov     eax,ebx
        imul    eax,(NB_VAR_NAME_LEN+NB_VAR_VAL_LEN)
        add     eax,var_table
        add     eax,NB_VAR_NAME_LEN
        mov     esi,eax
        pop     ebx ecx edi
        ret


; expand_vars: copy [esi]→[edi] expanding $VAR / ${VAR}
; esi and edi are used directly (no push/pop confusion)

expand_vars:
.lp:
        mov     al,[esi]
        or      al,al
        jz      .done
        cmp     al,'$'
        jne     .copy_char
        inc     esi
        mov     al,[esi]
        cmp     al,'{'
        je      .braced
        ; bare $VAR
        mov     ebx,ev_name_buf
        xor     ecx,ecx
.bare:
        mov     al,[esi]
        call    is_varname_char
        jz      .bare_done
        mov     [ebx+ecx],al
        inc     ecx
        inc     esi
        jmp     .bare
.bare_done:
        mov     byte [ebx+ecx],0
        jmp     .subst
.braced:
        inc     esi         ; skip '{'
        mov     ebx,ev_name_buf
        xor     ecx,ecx
.brace_rd:
        mov     al,[esi]
        or      al,al
        jz      .bare_done
        cmp     al,'}'
        je      .brace_end
        mov     [ebx+ecx],al
        inc     ecx
        inc     esi
        jmp     .brace_rd
.brace_end:
        inc     esi
        mov     byte [ebx+ecx],0
.subst:
        ; lookup ev_name_buf, copy value inline
        push    esi
        mov     esi,ev_name_buf
        call    var_value           ; esi → value ptr
.copy_val:
        mov     al,[esi]
        or      al,al
        jz      .subst_done
        stosb
        inc     esi
        jmp     .copy_val
.subst_done:
        pop     esi
        jmp     .lp
.copy_char:
        stosb
        inc     esi
        jmp     .lp
.done:
        mov     byte [edi],0
        ret


; String helpers

read_token:
        mov     al,[esi]
        or      al,al
        jz      .d
        cmp     al,' '
        je      .d
        cmp     al,9
        je      .d
        stosb
        inc     esi
        jmp     read_token
.d:     mov     byte [edi],0
        ret

read_token_eq:
        mov     al,[esi]
        or      al,al
        jz      .d
        cmp     al,'='
        je      .d
        cmp     al,' '
        je      .d
        cmp     al,9
        je      .d
        stosb
        inc     esi
        jmp     read_token_eq
.d:
        ; rtrim
        dec     edi
.rt:    cmp     byte [edi],' '
        jne     .rtd
        dec     edi
        jmp     .rt
.rtd:   inc     edi
        mov     byte [edi],0
        ret

skip_ws:
        mov     al,[esi]
        cmp     al,' '
        je      .s
        cmp     al,9
        je      .s
        ret
.s:     inc     esi
        jmp     skip_ws

skip_ws_esi:
        mov     al,[esi]
        cmp     al,' '
        je      .s
        cmp     al,9
        je      .s
        ret
.s:     inc     esi
        jmp     skip_ws_esi

copy_str_di:
        mov     al,[esi]
        or      al,al
        jz      .d
        stosb
        inc     esi
        jmp     copy_str_di
.d:     ret

strip_line:
        push    esi edi ebx
        mov     esi,line_buf
.find_end:
        mov     al,[esi]
        or      al,al
        jz      .found_end
        inc     esi
        jmp     .find_end
.found_end:
        dec     esi
.strip:
        cmp     esi,line_buf
        jb      .done
        mov     al,[esi]
        cmp     al,0xA
        je      .zap
        cmp     al,0xD
        je      .zap
        jmp     .done
.zap:   mov     byte [esi],0
        dec     esi
        jmp     .strip
.done:
        pop     ebx edi esi
        ret

; strcmp_fasm: compare [esi] vs [edi], ZF=1 if equal
strcmp_fasm:
        push    esi edi
.lp:    mov     al,[esi]
        mov     ah,[edi]
        cmp     al,ah
        jne     .ne
        or      al,al
        jz      .eq
        inc     esi
        inc     edi
        jmp     .lp
.eq:    pop     edi esi
        xor     eax,eax
        ret
.ne:    pop     edi esi
        or      eax,1
        ret

; str_eq: compare [esi] (keyword) vs [edi] (token), ZF=1 if equal
str_eq:
        push    esi edi
        ; swap: esi=keyword, edi=tok — compare them
.lp:    mov     al,[esi]
        mov     ah,[edi]
        cmp     al,ah
        jne     .ne
        or      al,al
        jz      .eq
        inc     esi
        inc     edi
        jmp     .lp
.eq:    pop     edi esi
        xor     eax,eax
        ret
.ne:    pop     edi esi
        or      eax,1
        ret

is_varname_char:
        cmp     al,'_'
        je      .y
        cmp     al,'A'
        jb      .cl
        cmp     al,'Z'
        jbe     .y
.cl:    cmp     al,'a'
        jb      .cd
        cmp     al,'z'
        jbe     .y
.cd:    cmp     al,'0'
        jb      .n
        cmp     al,'9'
        jbe     .y
.n:     xor     al,al
        ret
.y:     or      al,1
        ret


section '.data' writeable align 4


; built-in var names/defaults
bvar_nsa_name    db 'NSA',0
bvar_nsa_val     db 'nsa',0
bvar_fasm_name   db 'FASM',0
bvar_fasm_val    db 'fasm',0
bvar_outdir_name db 'OUTDIR',0
bvar_srcdir_name db 'SRCDIR',0
bvar_dot         db '.',0

; keywords
kw_nsa    db 'nsa',0
kw_nss    db 'nss',0
kw_asm_kw db 'asm',0
kw_run    db 'run',0
kw_echo   db 'echo',0
kw_clean  db 'clean',0

; var name keys
str_nsa_key   db 'NSA',0
str_fasm_key  db 'FASM',0

; file modes
open_r  db 'r',0

; default buildfile
default_buildfile db 'build.nb',0

; strings
str_banner db 'nbuild 1.1 - nusaOS Build System',0xA,0

str_version db 'nbuild 1.1',0xA
            db 'nusaOS Build System',0xA
            db 'Copyright (c) 2026 danko1122q / nusaOS project',0xA
            db 'License: GPL-3.0-or-later',0xA,0

str_usage db 0xA
          db 'Usage:  nbuild [buildfile]',0xA
          db '        nbuild -f <buildfile>',0xA
          db '        nbuild -h',0xA
          db 0xA
          db 'Directives (.nb):',0xA
          db '  # comment',0xA
          db '  @VAR = value        set variable',0xA
          db '  nsa  <src.nsa>      compile NSA -> .nbin',0xA
          db '  nss  <src.nss>      compile NSS -> .nbin',0xA
          db '  asm  <src.asm>      assemble with fasm',0xA
          db '  run  <command>      run arbitrary command',0xA
          db '  echo <message>      print message',0xA
          db '  clean <file...>     delete file(s)',0xA
          db 0xA
          db 'Built-in variables:',0xA
          db '  $NSA    nsa binary   (default: nsa)',0xA
          db '  $FASM   fasm binary  (default: fasm)',0xA
          db '  $OUTDIR output dir   (default: .)',0xA
          db '  $SRCDIR source dir   (default: .)',0xA
          db 0

str_done         db '[nbuild] Build finished.',0xA,0
str_build_failed db '[nbuild] Build FAILED.',0xA,0
str_run_pfx      db '  >> ',0
str_echo_pfx     db '[echo] ',0
str_clean_pfx    db '[clean] ',0
str_warn_unknown db '[nbuild] warning: unknown directive: ',0
str_err_open     db '[nbuild] error: cannot open buildfile: ',0
str_cmd_fail     db '[nbuild] error: command failed.',0xA,0
str_sp_build_sp  db ' build ',0
str_space        db ' ',0
str_oom          db '[nbuild] error: out of memory.',0xA,0
str_empty        db 0

section '.bss' writeable align 4

g_argc           dd ?
g_argv           dd ?
g_stack          dd ?
g_buildfile      dd ?
g_nb_fp          dd ?
g_lineno         dd ?
g_var_count      dd ?
additional_memory dd ?

sv_name          dd ?
sv_val           dd ?

con_handle       dd ?
character        db ?

var_table        rb NB_MAX_VARS * (NB_VAR_NAME_LEN + NB_VAR_VAL_LEN)

line_buf         rb NB_LINE_LEN
tok_buf          rb NB_LINE_LEN
exp_buf          rb NB_CMD_LEN
cmd_buf          rb NB_CMD_LEN
ev_name_buf      rb NB_VAR_NAME_LEN