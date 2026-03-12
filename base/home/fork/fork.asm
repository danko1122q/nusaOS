format ELF executable 3
segment readable executable

; Definisi sesuai syscall_numbers.h nusaOS
SYS_EXIT    equ 1
SYS_FORK    equ 2
SYS_EXECVE  equ 6
SYS_WAITPID equ 20

entry main

main:
    ; --- STEP 1: FORK ---
    mov eax, SYS_FORK
    int 0x80

    test eax, eax
    js  error_exit
    jz  child_process

parent_process:
    mov [child_pid], eax

    ; --- STEP 3: WAITPID ---
    mov eax, SYS_WAITPID
    mov ebx, [child_pid]
    xor ecx, ecx            ; status = NULL
    xor edx, edx            ; options = 0
    int 0x80
    jmp exit_program

child_process:
    ; --- STEP 2: EXECVE ---
    mov eax, SYS_EXECVE
    mov ebx, path           ; arg1: path
    mov ecx, argv           ; arg2: argv
    mov edx, envp           ; arg3: envp
    int 0x80
    
    ; Jika sampai sini, berarti execve gagal
    jmp error_exit

error_exit:
    mov eax, SYS_EXIT
    mov ebx, 1
    int 0x80

exit_program:
    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80

segment readable writeable
    path        db '/bin/open', 0
    a0          db 'open', 0
    a1          db '/apps/calculator.app', 0
    
    ; Array pointer (dd = 4 byte untuk i686)
    argv        dd a0
                dd a1
                dd 0        ; Null terminator
    
    envp        dd 0        ; Array envp kosong
    child_pid   dd 0