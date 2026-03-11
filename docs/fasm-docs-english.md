# FASM on nusaOS — Complete Guide

FASM (Flat Assembler) is a fast, self-hosting x86 assembler that runs natively on nusaOS. You write assembly source code in a `.asm` file, then FASM compiles it directly into an executable binary — no linker, no extra tools needed.

This guide covers everything you need to write, assemble, and run x86 assembly programs on nusaOS.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Your First Program](#2-your-first-program)
3. [Program Structure](#3-program-structure)
4. [x86 Registers](#4-x86-registers)
5. [Data Definitions](#5-data-definitions)
6. [Memory Access](#6-memory-access)
7. [Instructions — Data Movement](#7-instructions--data-movement)
8. [Instructions — Arithmetic](#8-instructions--arithmetic)
9. [Instructions — Logic & Bitwise](#9-instructions--logic--bitwise)
10. [Instructions — Control Flow](#10-instructions--control-flow)
11. [Instructions — Stack](#11-instructions--stack)
12. [Instructions — String Operations](#12-instructions--string-operations)
13. [nusaOS Syscalls](#13-nusaos-syscalls)
14. [Macros](#14-macros)
15. [Constants and Labels](#15-constants-and-labels)
16. [Conditional Assembly](#16-conditional-assembly)
17. [Including Files](#17-including-files)
18. [Full Program Examples](#18-full-program-examples)
19. [Common Errors](#19-common-errors)
20. [Instruction Quick Reference](#20-instruction-quick-reference)

---

## 1. Getting Started

FASM is a single binary located at `/bin/fasm` on nusaOS.

```sh
# Assemble source.asm into an executable named output
fasm source.asm output

# Assemble with default output name (same as input, no extension)
fasm source.asm

# Set memory limit to 512 KB
fasm -m 512 source.asm output

# Limit maximum passes to 10
fasm -p 10 source.asm output

# Pre-define a symbolic variable
fasm -d VERSION=2 source.asm output

# Dump symbol information to a file
fasm -s symbols.txt source.asm output
```

The whole workflow is two steps:

```
Write code (.asm)  →  fasm source.asm output  →  ./output
```

---

## 2. Your First Program

Create a file called `hello.asm`:

```asm
format ELF executable 3
entry start

segment readable executable

start:
    mov eax, 4          ; SYS_WRITE
    mov ebx, 1          ; stdout (fd = 1)
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, 1          ; SYS_EXIT
    xor ebx, ebx        ; exit code 0
    int 0x80

segment readable

msg     db 'Hello from nusaOS!', 0xA
msg_len = $ - msg
```

Assemble and run:

```sh
fasm hello.asm hello
./hello
```

Output:
```
Hello from nusaOS!
```

### Basic writing rules

- One instruction per line
- Labels end with a colon: `start:`
- Semicolons start a comment: `; this is a comment`
- Everything after `;` on a line is ignored
- FASM is **case-insensitive** for mnemonics — `MOV`, `mov`, `Mov` are all the same
- Labels **are** case-sensitive — `Start` and `start` are different labels

---

## 3. Program Structure

Every nusaOS executable written in FASM starts with three lines:

```asm
format ELF executable 3   ; output format
entry start               ; entry point label
```

Then one or more **segments**:

```asm
segment readable executable    ; code goes here

segment readable               ; read-only data (strings, constants)

segment readable writeable     ; read-write data (variables)
```

### Format directive

| Directive | Description |
|-----------|-------------|
| `format ELF` | Relocatable object file (needs linking — not executable alone) |
| `format ELF executable 3` | Standalone ELF executable for nusaOS |

Always use `format ELF executable 3` for programs you want to run directly with `./`.

### Segment attributes

| Attribute | Meaning |
|-----------|---------|
| `readable` | Segment can be read |
| `writeable` | Segment can be written (for variables) |
| `executable` | Segment contains runnable code |

Typical layout:

```asm
format ELF executable 3
entry start

segment readable executable    ; .text — all your code

start:
    ; ... your program ...
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable               ; .rodata — read-only strings & constants
    msg db 'Hello!', 0xA
    msg_len = $ - msg

segment readable writeable     ; .bss / .data — mutable variables
    buffer rb 256              ; reserve 256 bytes
    counter dd 0               ; 32-bit initialized to 0
```

---

## 4. x86 Registers

x86 (i386) has eight general-purpose registers. All are 32-bit.

### General-purpose registers

| 32-bit | 16-bit | 8-bit high | 8-bit low | Common use |
|--------|--------|------------|-----------|------------|
| `eax` | `ax` | `ah` | `al` | Accumulator, syscall number, return value |
| `ebx` | `bx` | `bh` | `bl` | Syscall arg 1, base pointer |
| `ecx` | `cx` | `ch` | `cl` | Syscall arg 2, loop counter |
| `edx` | `dx` | `dh` | `dl` | Syscall arg 3, data register |
| `esi` | `si` | — | — | Syscall arg 4, source index |
| `edi` | `di` | — | — | Syscall arg 5, destination index |
| `esp` | `sp` | — | — | Stack pointer (don't clobber casually) |
| `ebp` | `bp` | — | — | Base pointer (stack frame) |

### Special registers

| Register | Description |
|----------|-------------|
| `eip` | Instruction pointer — current instruction address (not directly writable) |
| `eflags` | Flags register — holds results of comparisons (ZF, CF, SF, OF, etc.) |

### Sub-register example

```asm
mov eax, 0x12345678
; eax = 0x12345678
; ax  = 0x5678  (lower 16 bits)
; ah  = 0x56    (bits 8–15)
; al  = 0x78    (bits 0–7)
```

---

## 5. Data Definitions

### Define bytes, words, dwords

| Directive | Size | Example |
|-----------|------|---------|
| `db` | 1 byte | `db 0x41` → stores byte 0x41 ('A') |
| `dw` | 2 bytes | `dw 0x1234` |
| `dd` | 4 bytes | `dd 0xDEADBEEF` |
| `dq` | 8 bytes | `dq 0` |

### Strings

```asm
msg     db 'Hello, nusaOS!', 0xA, 0    ; string + newline + null terminator
name    db "Dava", 0                   ; null-terminated string
```

Single quotes and double quotes are both accepted.

### Reserve space (uninitialized)

| Directive | Reserves |
|-----------|----------|
| `rb N` | N bytes |
| `rw N` | N words (2N bytes) |
| `rd N` | N dwords (4N bytes) |
| `rq N` | N qwords (8N bytes) |

```asm
segment readable writeable
    buffer  rb 256     ; reserve 256 bytes for a buffer
    number  rd 1       ; reserve space for one 32-bit integer
```

### Length calculation with `$`

`$` is the current address. Use it to compute string length:

```asm
msg     db 'Hello!', 0xA
msg_len = $ - msg           ; msg_len = 7 (6 chars + newline)
```

---

## 6. Memory Access

Square brackets `[ ]` mean "value at this memory address" — like dereferencing a pointer.

```asm
mov eax, [label]        ; load 32-bit value from memory at label
mov [label], eax        ; store 32-bit value to memory at label
mov al, [label]         ; load 1 byte
mov [label], al         ; store 1 byte
```

### Size override

Use `byte`, `word`, `dword` to be explicit about size:

```asm
mov byte [buffer], 0x41         ; store 1 byte
mov word [buffer], 0x1234       ; store 2 bytes
mov dword [counter], 100        ; store 4 bytes
```

### Indirect and indexed addressing

```asm
mov eax, [ebx]          ; load from address in ebx
mov eax, [ebx+4]        ; load from ebx + 4
mov eax, [ebx+ecx*4]    ; load from ebx + ecx * 4
mov eax, [ebx+ecx*4+8]  ; with displacement
```

### Example — write to a buffer

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

## 7. Instructions — Data Movement

### mov — move data

Copies a value from source to destination. The source is not changed.

```asm
mov eax, 42             ; eax = 42  (immediate)
mov eax, ebx            ; eax = ebx (register to register)
mov eax, [var]          ; eax = memory[var]
mov [var], eax          ; memory[var] = eax
mov eax, label          ; eax = address of label
```

You **cannot** move memory to memory directly:
```asm
mov [dst], [src]        ; INVALID — must use a register in between
```

### lea — load effective address

Computes an address and stores it in a register — does **not** read memory.

```asm
lea eax, [msg]          ; eax = address of msg
lea eax, [ebx+ecx*4]    ; eax = ebx + ecx*4 (no memory read)
```

Useful for computing offsets without touching memory.

### xchg — exchange

Swaps the values of two operands.

```asm
xchg eax, ebx           ; swap eax and ebx
```

### movzx — move with zero extension

Copies a smaller value into a larger register, filling upper bits with zero.

```asm
movzx eax, al           ; eax = zero-extend al to 32 bits
movzx eax, byte [buf]   ; eax = zero-extend byte at buf
```

### movsx — move with sign extension

Like `movzx` but preserves the sign bit.

```asm
movsx eax, al           ; sign-extend al to 32 bits
```

---

## 8. Instructions — Arithmetic

### add, sub — addition and subtraction

```asm
add eax, 10         ; eax = eax + 10
add eax, ebx        ; eax = eax + ebx
sub eax, 5          ; eax = eax - 5
sub eax, ecx        ; eax = eax - ecx
add [var], 1        ; memory variable += 1
```

### inc, dec — increment and decrement

```asm
inc eax             ; eax = eax + 1
dec ecx             ; ecx = ecx - 1
inc dword [counter] ; memory variable += 1
```

### mul — unsigned multiply

`mul` always uses `eax` as one operand. Result goes into `edx:eax` (64-bit).

```asm
mov eax, 6
mov ecx, 7
mul ecx             ; edx:eax = eax * ecx = 42
                    ; eax = 42, edx = 0 (if result fits in 32 bits)
```

### imul — signed multiply

```asm
imul eax, ecx           ; eax = eax * ecx
imul eax, ecx, 10       ; eax = ecx * 10
```

### div — unsigned divide

Divides `edx:eax` by the operand. Quotient → `eax`, remainder → `edx`.

```asm
xor edx, edx        ; clear edx first (upper 32 bits)
mov eax, 17
mov ecx, 5
div ecx             ; eax = 3 (quotient), edx = 2 (remainder)
```

> **Warning:** Always zero `edx` before `div` if you're dividing a 32-bit number. Failing to do so causes a division overflow crash.

### idiv — signed divide

Same as `div` but treats values as signed.

### neg — negate

```asm
neg eax             ; eax = -eax  (two's complement)
```

---

## 9. Instructions — Logic & Bitwise

### and, or, xor — bitwise operations

```asm
and eax, 0xFF       ; mask: keep only lower 8 bits
or  eax, 0x80       ; set bit 7
xor eax, eax        ; fastest way to zero eax (eax = 0)
xor eax, 0x01       ; flip bit 0
```

### not — bitwise NOT

```asm
not eax             ; flip all bits (one's complement)
```

### shl, shr — shift left / shift right

```asm
shl eax, 1          ; eax = eax * 2   (shift left 1 bit)
shl eax, 3          ; eax = eax * 8
shr eax, 1          ; eax = eax / 2   (unsigned, shift right 1 bit)
shr eax, cl         ; shift right by amount in cl
```

### sar — arithmetic shift right (preserves sign)

```asm
sar eax, 1          ; eax = eax / 2  (signed)
```

### cmp — compare (sets flags, no result stored)

```asm
cmp eax, ebx        ; sets flags based on eax - ebx
cmp eax, 0          ; compare eax to zero
```

`cmp` doesn't change any register — it only sets flags used by conditional jumps.

### test — bitwise AND (sets flags only)

```asm
test eax, eax       ; check if eax == 0 (sets ZF if so)
test al, 0x01       ; check if bit 0 is set
```

---

## 10. Instructions — Control Flow

### Unconditional jump

```asm
jmp label           ; jump to label unconditionally
```

### Conditional jumps (after cmp or test)

| Instruction | Condition | Signed? |
|-------------|-----------|---------|
| `je` / `jz` | Equal / Zero | both |
| `jne` / `jnz` | Not equal / Not zero | both |
| `jl` / `jnge` | Less than | signed |
| `jle` / `jng` | Less or equal | signed |
| `jg` / `jnle` | Greater than | signed |
| `jge` / `jnl` | Greater or equal | signed |
| `jb` / `jnae` | Below (unsigned less) | unsigned |
| `jbe` / `jna` | Below or equal | unsigned |
| `ja` / `jnbe` | Above (unsigned greater) | unsigned |
| `jae` / `jnb` | Above or equal | unsigned |
| `js` | Sign flag set (negative) | — |
| `jns` | Sign flag clear (positive) | — |
| `jc` | Carry flag set | — |
| `jnc` | Carry flag clear | — |

Example — if/else:

```asm
    cmp eax, 10
    jge .greater_or_equal

    ; eax < 10
    mov ebx, 0
    jmp .done

  .greater_or_equal:
    ; eax >= 10
    mov ebx, 1

  .done:
```

### call and ret — subroutines

```asm
    call my_function    ; pushes return address, jumps to my_function

my_function:
    ; ... do something ...
    ret                 ; pops return address, jumps back
```

### loop — loop counter using ecx

```asm
    mov ecx, 5          ; loop 5 times
  .repeat:
    ; ... body ...
    loop .repeat        ; dec ecx; jnz .repeat
```

> `loop` decrements `ecx` and jumps if `ecx` ≠ 0. It stops when `ecx` reaches 0.

### Local labels

Labels starting with a dot `.` are local to the nearest non-local label above them. This lets you reuse names like `.loop` or `.done` in multiple functions:

```asm
func_a:
    jmp .done           ; jumps to func_a's .done

  .done:
    ret

func_b:
    jmp .done           ; jumps to func_b's .done (different label)

  .done:
    ret
```

---

## 11. Instructions — Stack

The stack grows **downward** in memory. `esp` always points to the top of the stack (the last value pushed).

### push — push onto stack

```asm
push eax            ; esp -= 4, then [esp] = eax
push 42             ; push immediate value
push dword [var]    ; push memory value
```

### pop — pop from stack

```asm
pop eax             ; eax = [esp], then esp += 4
pop ecx
```

### Preserving registers in a subroutine

Always save registers you will modify and restore them before `ret`:

```asm
my_func:
    push ebx            ; save
    push ecx

    ; use ebx, ecx freely here

    pop ecx             ; restore in reverse order
    pop ebx
    ret
```

### pusha / popa — push/pop all general-purpose registers

```asm
pusha               ; save eax, ecx, edx, ebx, esp, ebp, esi, edi
popa                ; restore all
```

---

## 12. Instructions — String Operations

These instructions work with `esi` (source) and `edi` (destination), and auto-advance the pointers. The direction is controlled by the direction flag (cleared with `cld` → forward, set with `std` → backward).

Always use `cld` before string operations unless you specifically need reverse order.

| Instruction | Operation |
|-------------|-----------|
| `movsb` | Copy byte from `[esi]` to `[edi]`, advance both |
| `movsw` | Copy word |
| `movsd` | Copy dword |
| `stosb` | Store `al` to `[edi]`, advance `edi` |
| `stosw` | Store `ax` |
| `stosd` | Store `eax` |
| `lodsb` | Load byte from `[esi]` into `al`, advance `esi` |
| `lodsw` | Load word into `ax` |
| `lodsd` | Load dword into `eax` |
| `scasb` | Compare `al` with `[edi]`, advance `edi` |
| `cmpsb` | Compare `[esi]` with `[edi]`, advance both |

### rep prefix — repeat

Repeats the following string instruction `ecx` times:

```asm
; Zero out 256 bytes of buffer
    cld
    xor eax, eax
    mov edi, buffer
    mov ecx, 256
    rep stosb           ; store al=0 to [edi] 256 times

; Copy 16 bytes from src to dst
    cld
    mov esi, src
    mov edi, dst
    mov ecx, 16
    rep movsb
```

---

## 13. nusaOS Syscalls

nusaOS uses **Linux i386 syscall convention**: place the syscall number in `eax`, arguments in `ebx`, `ecx`, `edx`, `esi`, `edi`, then execute `int 0x80`. The return value comes back in `eax`.

```
int 0x80
  eax = syscall number
  ebx = arg1
  ecx = arg2
  edx = arg3
  esi = arg4
  edi = arg5
```

### Syscall table

| Number | Name | Arguments | Description |
|--------|------|-----------|-------------|
| 1 | `SYS_EXIT` | `ebx` = exit code | Terminate program |
| 2 | `SYS_FORK` | — | Fork process |
| 3 | `SYS_READ` | `ebx`=fd, `ecx`=buf, `edx`=count | Read from file/stdin |
| 4 | `SYS_WRITE` | `ebx`=fd, `ecx`=buf, `edx`=count | Write to file/stdout/stderr |
| 7 | `SYS_OPEN` | `ebx`=path, `ecx`=flags, `edx`=mode | Open a file |
| 8 | `SYS_CLOSE` | `ebx`=fd | Close file descriptor |
| 9 | `SYS_FSTAT` | `ebx`=fd, `ecx`=stat_buf | Get file info |
| 10 | `SYS_STAT` | `ebx`=path, `ecx`=stat_buf | Get file info by path |
| 11 | `SYS_LSEEK` | `ebx`=fd, `ecx`=offset, `edx`=whence | Move file position |
| 12 | `SYS_KILL` | `ebx`=pid, `ecx`=signal | Send signal |
| 13 | `SYS_GETPID` | — | Get process ID |
| 15 | `SYS_UNLINK` | `ebx`=path | Delete file |
| 16 | `SYS_GETTIMEOFDAY` | `ebx`=timeval_buf, `ecx`=0 | Get current time |
| 22 | `SYS_CHDIR` | `ebx`=path | Change directory |
| 23 | `SYS_GETCWD` | `ebx`=buf, `ecx`=size | Get current directory |
| 25 | `SYS_RMDIR` | `ebx`=path | Remove directory |
| 26 | `SYS_MKDIR` | `ebx`=path, `ecx`=mode | Create directory |
| 36 | `SYS_READLINK` | `ebx`=path, `ecx`=buf, `edx`=size | Read symbolic link |
| 59 | `SYS_MMAP` | `ebx`=addr, `ecx`=len, `edx`=prot, `esi`=flags, `edi`=fd | Map memory |
| 60 | `SYS_MUNMAP` | `ebx`=addr, `ecx`=len | Unmap memory |
| 75 | `SYS_ACCESS` | `ebx`=path, `ecx`=mode | Check file access |
| 77 | `SYS_UNAME` | `ebx`=utsname_buf | Get OS information |

### File descriptor constants

| Value | Meaning |
|-------|---------|
| `0` | stdin |
| `1` | stdout |
| `2` | stderr |

### Open flags (for SYS_OPEN)

| Flag | Value | Meaning |
|------|-------|---------|
| `O_RDONLY` | 0 | Read only |
| `O_WRONLY` | 1 | Write only |
| `O_RDWR` | 2 | Read and write |
| `O_CREAT` | 0x40 | Create if not exists |
| `O_TRUNC` | 0x200 | Truncate to zero length |
| `O_APPEND` | 0x400 | Append mode |

### Lseek whence values

| Value | Meaning |
|-------|---------|
| `0` | SEEK_SET — from beginning |
| `1` | SEEK_CUR — from current position |
| `2` | SEEK_END — from end |

### Syscall examples

**Write "Hello" to stdout:**
```asm
    mov eax, 4          ; SYS_WRITE
    mov ebx, 1          ; fd = stdout
    mov ecx, msg
    mov edx, msg_len
    int 0x80
```

**Read from stdin into buffer:**
```asm
    mov eax, 3          ; SYS_READ
    mov ebx, 0          ; fd = stdin
    mov ecx, buffer
    mov edx, 256        ; max bytes to read
    int 0x80
    ; eax = number of bytes actually read
```

**Exit with code 0:**
```asm
    mov eax, 1          ; SYS_EXIT
    xor ebx, ebx        ; exit code = 0
    int 0x80
```

**Open a file:**
```asm
    mov eax, 7          ; SYS_OPEN
    mov ebx, filename   ; null-terminated path
    mov ecx, 0          ; O_RDONLY
    xor edx, edx
    int 0x80
    ; eax = file descriptor (negative = error)
    mov [fd], eax
```

**Write to a file:**
```asm
    mov eax, 4          ; SYS_WRITE
    mov ebx, [fd]       ; file descriptor
    mov ecx, data
    mov edx, data_len
    int 0x80
```

**Close a file:**
```asm
    mov eax, 8          ; SYS_CLOSE
    mov ebx, [fd]
    int 0x80
```

---

## 14. Macros

FASM has a powerful macro system. Macros let you define reusable code patterns.

### Simple macro

```asm
macro exit code
{
    mov eax, 1
    mov ebx, code
    int 0x80
}

; usage:
exit 0
exit 42
```

### Macro with multiple arguments

```asm
macro write fd, buf, len
{
    mov eax, 4
    mov ebx, fd
    mov ecx, buf
    mov edx, len
    int 0x80
}

; usage:
write 1, msg, msg_len
```

### Variadic macro

```asm
macro println [text]
{
    common
    local .str, .len
    jmp .skip
  .str db text, 0xA
  .len = $ - .str
  .skip:
    mov eax, 4
    mov ebx, 1
    mov ecx, .str
    mov edx, .len
    int 0x80
}

; usage:
println 'Hello!'
println 'Line two'
```

### struc — structure definition

```asm
struc Point x, y
{
    .x dd x
    .y dd y
}

; declare a Point:
p1 Point 10, 20

; access fields:
    mov eax, [p1.x]     ; eax = 10
    mov ecx, [p1.y]     ; ecx = 20
```

---

## 15. Constants and Labels

### Numeric constants

```asm
STDIN  = 0
STDOUT = 1
STDERR = 2

SYS_EXIT  = 1
SYS_WRITE = 4
SYS_READ  = 3
```

Constants defined with `=` are compile-time values. They can be used anywhere a number is expected.

### String constants

```asm
VERSION_STRING equ '1.0'
```

`equ` defines a textual substitution — `VERSION_STRING` is replaced with `'1.0'` wherever used.

### Labels

A label marks an address in memory:

```asm
start:          ; regular label — marks current address
.loop:          ; local label — scoped to nearest parent label
```

Labels can be used as jump targets or as addresses (e.g., in `mov ecx, label`).

---

## 16. Conditional Assembly

```asm
DEBUG = 1

if DEBUG = 1
    ; this code is included
    mov eax, [debug_var]
    ...
end if

if DEBUG = 0
    ; this code is skipped
end if
```

Useful for toggling debug output or building different versions from the same source.

```asm
ARCH = 32

if ARCH = 64
    ; 64-bit only code
else
    ; 32-bit code
end if
```

---

## 17. Including Files

Split large programs across multiple files:

```asm
include 'utils.asm'        ; include relative to current file
include '/home/lib/io.asm' ; include with absolute path
```

The included file is inserted at that point as if you typed it inline.

Typical pattern — put helper macros and constants in a separate file:

```asm
; macros.asm
macro exit code { mov eax,1 ; mov ebx,code ; int 0x80 }
macro write fd,buf,len { mov eax,4 ; mov ebx,fd ; mov ecx,buf ; mov edx,len ; int 0x80 }

STDOUT = 1
SYS_READ  = 3
SYS_WRITE = 4
SYS_EXIT  = 1
```

```asm
; main.asm
format ELF executable 3
entry start

include 'macros.asm'

segment readable executable
start:
    write STDOUT, msg, msg_len
    exit 0

segment readable
    msg     db 'Hello!', 0xA
    msg_len = $ - msg
```

---

## 18. Full Program Examples

### Example 1 — Hello World

```asm
format ELF executable 3
entry start

segment readable executable

start:
    mov eax, 4
    mov ebx, 1
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable

msg     db 'Hello, nusaOS!', 0xA
msg_len = $ - msg
```

---

### Example 2 — Read input and echo it back

```asm
format ELF executable 3
entry start

segment readable executable

start:
    ; print prompt
    mov eax, 4
    mov ebx, 1
    mov ecx, prompt
    mov edx, prompt_len
    int 0x80

    ; read from stdin
    mov eax, 3
    mov ebx, 0
    mov ecx, buffer
    mov edx, 256
    int 0x80
    mov [read_count], eax   ; save bytes read

    ; write prefix
    mov eax, 4
    mov ebx, 1
    mov ecx, reply
    mov edx, reply_len
    int 0x80

    ; echo what was typed
    mov eax, 4
    mov ebx, 1
    mov ecx, buffer
    mov edx, [read_count]
    int 0x80

    ; exit
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable

prompt      db 'Enter your name: ', 0
prompt_len  = $ - prompt
reply       db 'Hello, ', 0
reply_len   = $ - reply

segment readable writeable

buffer      rb 256
read_count  dd 0
```

---

### Example 3 — Count from 1 to 10

```asm
format ELF executable 3
entry start

segment readable executable

; Converts a single-digit number in eax to ASCII and prints it
print_digit:
    push eax
    add al, '0'
    mov [digit_buf], al
    mov eax, 4
    mov ebx, 1
    mov ecx, digit_buf
    mov edx, 1
    int 0x80
    pop eax
    ret

; Print a newline
print_newline:
    mov eax, 4
    mov ebx, 1
    mov ecx, newline
    mov edx, 1
    int 0x80
    ret

start:
    mov ecx, 1              ; counter = 1

  .loop:
    cmp ecx, 10
    jg  .done               ; stop when counter > 10

    mov eax, ecx
    call print_digit
    call print_newline

    inc ecx
    jmp .loop

  .done:
    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable writeable
    digit_buf   rb 1

segment readable
    newline     db 0xA
```

---

### Example 4 — Read a file and print its contents

```asm
format ELF executable 3
entry start

O_RDONLY = 0

segment readable executable

start:
    ; open file
    mov eax, 7              ; SYS_OPEN
    mov ebx, filename
    mov ecx, O_RDONLY
    xor edx, edx
    int 0x80
    cmp eax, 0
    jl  .open_error
    mov [fd], eax

  .read_loop:
    ; read chunk
    mov eax, 3              ; SYS_READ
    mov ebx, [fd]
    mov ecx, buffer
    mov edx, 512
    int 0x80
    cmp eax, 0
    jle .close              ; 0 = EOF, negative = error

    ; write to stdout
    mov edx, eax            ; bytes read = bytes to write
    mov eax, 4              ; SYS_WRITE
    mov ebx, 1
    mov ecx, buffer
    int 0x80
    jmp .read_loop

  .close:
    mov eax, 8              ; SYS_CLOSE
    mov ebx, [fd]
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

  .open_error:
    mov eax, 4
    mov ebx, 2              ; stderr
    mov ecx, err_msg
    mov edx, err_len
    int 0x80

    mov eax, 1
    mov ebx, 1
    int 0x80

segment readable

filename    db '/home/test.txt', 0
err_msg     db 'Error: cannot open file', 0xA
err_len     = $ - err_msg

segment readable writeable

fd          dd 0
buffer      rb 512
```

---

### Example 5 — Subroutine with stack-based argument passing

```asm
format ELF executable 3
entry start

segment readable executable

; add_numbers(a, b) -> result in eax
; called as: push b; push a; call add_numbers
add_numbers:
    push ebp
    mov  ebp, esp
    mov  eax, [ebp+8]   ; a (first argument)
    add  eax, [ebp+12]  ; b (second argument)
    pop  ebp
    ret  8              ; pop 8 bytes of arguments on return

start:
    push 25             ; b
    push 17             ; a
    call add_numbers    ; eax = 17 + 25 = 42

    ; print result digit
    add al, '0'
    mov [result_buf], al
    mov eax, 4
    mov ebx, 1
    mov ecx, result_buf
    mov edx, 1
    int 0x80

    mov eax, 4
    mov ebx, 1
    mov ecx, newline
    mov edx, 1
    int 0x80

    mov eax, 1
    xor ebx, ebx
    int 0x80

segment readable
    newline db 0xA

segment readable writeable
    result_buf rb 4
```

---

## 19. Common Errors

### `illegal instruction`

The most common error. Usually caused by using the wrong directive for your format:

| If format is | Use | Not |
|---|---|---|
| `format ELF executable` | `segment readable executable` | `section '.text' executable` |
| `format ELF` | `section '.text' executable` | `segment readable executable` |

```
test.asm [4]:
section '.text' executable
error: illegal instruction.
```

Fix: Change `section` to `segment` when using `format ELF executable`.

---

### `undefined symbol`

A label or constant is used but never defined.

```
error: undefined symbol 'msg_len'.
```

Fix: Make sure the symbol is defined before use, or define it after with `=`:

```asm
msg     db 'Hello', 0xA
msg_len = $ - msg       ; must come right after the string
```

---

### `cannot find -lc` (during build on host)

This happens when building the nusaOS toolchain if libc isn't ready yet. FASM programs assembled for nusaOS don't need `-lc` — they use raw syscalls.

---

### `value out of range`

An immediate value is too large for the operand:

```asm
mov al, 256     ; ERROR — al is 8-bit, max value 255
mov al, 0xFF    ; OK
```

---

### `No such file or directory` when running `./program`

Three possible causes:
1. **You ran `./program.o`** — the `.o` extension means it's a relocatable object, not an executable. Use `format ELF executable 3` and specify the output: `fasm source.asm program`
2. **The ELF interpreter path doesn't exist** — nusaOS uses its own dynamic linker. Make sure your binary is statically linked (pure syscalls, no libc).
3. **Missing execute permission** — FASM normally sets this, but check with `ls -l`.

---

### Segmentation fault / crash

Common reasons:
- Accessing memory before setting up a valid address (e.g., forgetting `mov ecx, buffer` before using `[ecx]`)
- Stack imbalance — mismatched `push`/`pop` counts
- Dividing by zero — always check before `div`
- `edx` not zeroed before `div` — causes division overflow

---

## 20. Instruction Quick Reference

### Data movement

| Instruction | Description |
|-------------|-------------|
| `mov dst, src` | Copy src to dst |
| `lea dst, [expr]` | Load effective address |
| `xchg a, b` | Swap a and b |
| `movzx dst, src` | Move, zero-extend |
| `movsx dst, src` | Move, sign-extend |
| `push src` | Push to stack |
| `pop dst` | Pop from stack |

### Arithmetic

| Instruction | Description |
|-------------|-------------|
| `add dst, src` | dst = dst + src |
| `sub dst, src` | dst = dst - src |
| `mul src` | edx:eax = eax * src |
| `imul dst, src` | dst = dst * src (signed) |
| `div src` | eax = edx:eax / src |
| `inc dst` | dst = dst + 1 |
| `dec dst` | dst = dst - 1 |
| `neg dst` | dst = -dst |

### Logic & comparison

| Instruction | Description |
|-------------|-------------|
| `and dst, src` | Bitwise AND |
| `or dst, src` | Bitwise OR |
| `xor dst, src` | Bitwise XOR |
| `not dst` | Bitwise NOT |
| `shl dst, n` | Shift left |
| `shr dst, n` | Shift right (unsigned) |
| `sar dst, n` | Shift right (signed) |
| `cmp a, b` | Set flags for a - b |
| `test a, b` | Set flags for a AND b |

### Control flow

| Instruction | Description |
|-------------|-------------|
| `jmp label` | Unconditional jump |
| `je / jz` | Jump if equal / zero |
| `jne / jnz` | Jump if not equal / not zero |
| `jl / jg` | Jump if less / greater (signed) |
| `jle / jge` | Jump if ≤ / ≥ (signed) |
| `jb / ja` | Jump if below / above (unsigned) |
| `call label` | Call subroutine |
| `ret` | Return from subroutine |
| `loop label` | Decrement ecx, jump if ≠ 0 |

### String

| Instruction | Description |
|-------------|-------------|
| `movsb/w/d` | Copy [esi] to [edi], advance |
| `stosb/w/d` | Store al/ax/eax to [edi], advance |
| `lodsb/w/d` | Load [esi] into al/ax/eax, advance |
| `scasb/w/d` | Compare al/ax/eax with [edi] |
| `rep` | Repeat next instruction ecx times |
| `cld` | Clear direction flag (forward) |
| `std` | Set direction flag (backward) |
