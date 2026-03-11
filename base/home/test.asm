format ELF executable 3
entry _start

segment readable executable

_start:
    mov eax, 4
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable writeable
    msg db 'Halo dari FASM yang jalan di nusaOS!', 0xA
    msg_len = $ - msg