# NSA Language — Complete Guide

NSA is a programming language that runs natively on NusaOS. You write code in a `.nsa` file, compile it to a `.nbin` bytecode binary, and run it with the NSA runtime. One binary does everything — no external dependencies, no setup hassle.

This guide is written for everyone, including people who have never programmed before.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Your First Program](#2-your-first-program)
3. [Variables](#3-variables)
4. [Output](#4-output)
5. [Input](#5-input)
6. [Arithmetic](#6-arithmetic)
7. [Comparison & Logic](#7-comparison--logic)
8. [String Operations](#8-string-operations)
9. [Conditionals — if / else](#9-conditionals--if--else)
10. [Loops](#10-loops)
11. [Functions](#11-functions)
12. [Arrays](#12-arrays)
13. [Full Program Examples](#13-full-program-examples)
14. [The .nbin File Format](#14-the-nbin-file-format)
15. [Error Reference](#15-error-reference)
16. [Keyword Reference](#16-keyword-reference)
17. [Module System (.nss files)](#17-module-system-nss-files)
18. [Floating Point](#18-floating-point)
19. [String Indexing](#19-string-indexing)
20. [File I/O](#20-file-io)
21. [System Calls & Process Control](#21-system-calls--process-control-v252)
22. [Advanced String Operations](#22-advanced-string-operations-v253)
23. [Loop Control — break and continue](#23-loop-control--break-and-continue-v253)

---

## 1. Getting Started

Everything lives in one binary called `nsa`.

```sh
# Compile a .nsa file into a .nbin binary
nsa build program.nsa

# Compile with an explicit output name
nsa build program.nsa output.nbin

# Run a compiled program
nsa run program.nbin

# Show the built-in language reference
nsa help

# Print version info
nsa version
```

Writing an NSA program is always three steps:

```
Write code (.nsa)  →  nsa build  →  nsa run
```

---

## 2. Your First Program

Create a file called `hello.nsa` with this content:

```
// hello.nsa
print "Hello, NusaOS!"
```

Save it, then run:

```sh
nsa build hello.nsa
nsa run hello.nbin
```

Output:

```
Hello, NusaOS!
```

That's it — your first program works.

### Basic writing rules

- One statement per line
- No semicolons at the end of lines
- No curly braces
- Indentation is not required, but strongly recommended — it makes nested blocks much easier to read
- Case matters — `Print` is not a keyword, `print` is

### Comments
NSA supports three comment styles:
```
// single-line comment (C-style)
# single-line comment (hash-style)
print "this line runs"
# print "this line does not run"

/* block comment on one line */

/*
   block comment spanning
   multiple lines —
   none of this is executed
*/

let x = 10  /* inline block comment */
```

> **Note:** Block comments `/* */` cannot be nested. The text `/* ... */` inside a string literal is not treated as a comment.

---

## 3. Variables

A variable is a named box that holds data while your program runs. In NSA, every variable must be declared with `let` before you can use it.

### Three data types

| Type | Examples | Description |
|------|----------|-------------|
| `int` | `42`, `-7`, `0` | Whole numbers |
| `string` | `"NusaOS"`, `"hello"` | Text |
| `bool` | `true`, `false` | True or false |

### Integer

```
let x = 10
let year = 2026
let temperature = -5
let nothing = 0
```

Valid range: **-2,147,483,648** to **2,147,483,647** (signed 32-bit integer).

### String

```
let name = "danko1122q"
let message = "Welcome to NusaOS!"
let empty = ""
```

Maximum string length: **254 characters**.

Escape sequences you can use inside strings:

| Sequence | Result |
|----------|--------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |
| `\'` | Single quote |
| `\0` | Null byte |

Example:

```
let msg = "Line one\nLine two"
print msg
```

Output:
```
Line one
Line two
```

### Bool

```
let active = true
let done = false
```

Bools print as `true` or `false`, not `1` or `0`.

### Reassigning variables

You can reassign a variable using `let` again, as long as the type stays the same:

```
let score = 0
let score = 100    // valid — score is now 100
```

Changing the type is not allowed:

```
let x = 10
let x = "text"    // ERROR: x was previously declared as int
```

### Copying between variables

```
let a = 42
let b = a      // b is now 42, type is inferred from a

copy dst src   // explicit copy — works for any type
```

`copy` example:

```
let original = 99
let backup = 0
copy backup original    // backup = 99
```

Maximum variables per program: **200**.

---

## 4. Output

### print — with newline

Prints a value followed by a newline.

```
print "Hello!"
print 42
print name
print active
```

Works with any type — int, string, or bool.

### println — without newline

Prints a value **without** a trailing newline. Useful for inline prompts.

```
println "Enter your name: "
input name
println "Hello, "
print name
```

Output (no line break between the prompt and the answer):

```
Enter your name: Dava
Hello, Dava
```

The difference in practice:

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

## 5. Input

The `input` keyword reads a value from the keyboard into a variable.

```
input x
```

What gets read depends on the variable's type:

- **int** variable → reads a number
- **string** variable → reads a full line of text
- **undeclared** variable → automatically created as a string

Reading a number:

```
let age = 0
println "Your age: "
input age
println "You are "
println age
print " years old."
```

Reading text:

```
let name = ""
println "Your name: "
input name
println "Hello, "
print name
```

Auto-declared string:

```
println "Your city: "
input city       // city is automatically a string
print city
```

> **Note:** Reading into a bool variable is not supported. Use an int (0 or 1) instead.

---

## 6. Arithmetic

Math operations only work on integer variables. The result is always stored back into the destination variable.

### Basic operations

```
add x 10      // x = x + 10
sub x 5       // x = x - 5
mul x 3       // x = x * 3
div x 2       // x = x / 2
mod x 7       // x = x % 7  (remainder)
```

The second operand can be a number or another variable:

```
let a = 20
let b = 6

add a b       // a = 26
sub a b       // a = 20  (from current value)
mul a 2       // a = 40
div a b       // a = 6   (integer division, remainder discarded)
mod a b       // a = 2
```

### Increment and decrement

The quick way to add or subtract 1:

```
let i = 0
inc i         // i = 1
inc i         // i = 2
dec i         // i = 1
```

### Negate

```
let x = 5
neg x         // x = -5
neg x         // x = 5  (negate again to get back)
```

### Things to keep in mind

Division in NSA is **integer division** — the decimal part is thrown away:

```
let a = 7
div a 2       // a = 3, not 3.5
```

Dividing or taking modulo by zero causes a runtime error:

```
div a 0       // runtime error: division/modulo by zero
mod a 0       // runtime error: division/modulo by zero
```

Arithmetic cannot be used on string or bool variables.

---

## 7. Comparison & Logic

### cmp — compare two variables

`cmp` compares two variables and stores the result as a bool. It works with both **integers** and **strings** — the type of the operands determines which comparison is used.

**Integer comparison** — all six operators are available:

```
cmp result a == b    // result = (a equals b)
cmp result a != b    // result = (a does not equal b)
cmp result a <  b    // result = (a is less than b)
cmp result a >  b    // result = (a is greater than b)
cmp result a <= b    // result = (a is less than or equal to b)
cmp result a >= b    // result = (a is greater than or equal to b)
```

Example:

```
let a = 10
let b = 20
let bigger = false

cmp bigger b > a    // true, because 20 > 10
print bigger        // prints: true
```

**String comparison** — only `==` and `!=` are supported:

```
let s1 = "hello"
let s2 = "world"
let s3 = "hello"

cmp eq s1 == s3    // true  — same text
cmp ne s1 != s2    // true  — different text
```

The same `cmp` keyword works for both — NSA detects the type automatically. Mixing a string and an integer in the same `cmp` is an error.

### not — flip a value

Flips a bool between `true` and `false`, or an int between `0` and `1`:

```
let active = true
not active          // active = false
not active          // active = true  (flipped back)

let x = 1
not x               // x = 0
not x               // x = 1
```

### and / or — combine conditions

```
and result a b    // result = a AND b  (both must be true)
or  result a b    // result = a OR b   (either one is enough)
```

Example:

```
let old_enough = true
let has_id = false

and can_enter old_enough has_id    // false — both required
or  either_one old_enough has_id   // true — one is enough

print can_enter     // false
print either_one    // true
```

### Truthy rules — what counts as "true"

| Type | Considered false when |
|------|-----------------------|
| int | value is `0` |
| bool | value is `false` |
| string | empty string `""` |

Everything else is considered true.

---

## 8. String Operations

### concat — join strings together

Appends text to the end of a string variable.

```
let sentence = "Hello"
concat sentence ", world"    // sentence = "Hello, world"
concat sentence "!"          // sentence = "Hello, world!"
print sentence
```

You can also append one string variable to another:

```
let first = "Hello"
let second = ", NusaOS!"
concat first second          // first = "Hello, NusaOS!"
```

### len — length of a string

Stores the number of characters in a string into an integer variable.

```
let text = "hello"
let length = 0
len length text    // length = 5
print length
```

If the destination variable hasn't been declared yet, it's automatically created as an int:

```
let name = "NusaOS"
len n name    // n is automatically an int, value is 6
```

### to_str — integer to string

```
let number = 2026
let text = ""
to_str text number    // text = "2026"
```

### to_int — string to integer

```
let raw = "42"
let value = 0
to_int value raw    // value = 42
```

If the string is not a valid number, the result is `0`.

### Building a sentence dynamically

```
let name = ""
let age = 0

println "Name: "
input name
println "Age: "
input age

let msg = "Hello, "
concat msg name
concat msg "! You are "

let age_str = ""
to_str age_str age
concat msg age_str
concat msg " years old."

print msg
```

Output if you enter `Dava` and `20`:

```
Hello, Dava! You are 20 years old.
```

---

## 9. Conditionals — if / else

Conditionals let your program take different paths depending on a condition.

### Basic form

```
if condition then
    // code runs when condition is true
end
```

Every `if` **must** be closed with `end`.

### With else

```
if condition then
    // runs when condition is true
else
    // runs when condition is false
end
```

### Supported condition forms

**Compare a variable to a number:**

```
if x == 10 then ... end
if x != 0  then ... end
if x <  5  then ... end
if x >  5  then ... end
if x <= 10 then ... end
if x >= 10 then ... end
```

**Truthy test — check if a variable has a non-zero/non-empty value:**

```
if active then
    print "it's active!"
end

if name then
    print "name is not empty"
end
```

### Simple if/else example

```
let score = 75

if score >= 70 then
    print "Pass"
else
    print "Fail"
end
```

### Chained conditions

```
let score = 85

if score >= 90 then
    print "A"
else
    if score >= 80 then
        print "B"
    else
        if score >= 70 then
            print "C"
        else
            print "D"
        end
    end
end
```

Indentation isn't required but makes nested blocks much easier to follow.

---

## 10. Loops

Loops run a block of code repeatedly.

### loop N times — fixed count

Runs the block exactly N times. N must be a literal number, not a variable.

```
loop 5 times
    print "hello"
end
```

Output:
```
hello
hello
hello
hello
hello
```

The compiler manages the counter internally — you don't need a variable for it.

### loop while — conditional loop

Runs the block as long as the condition is true. The condition is checked **before** each iteration.

**With a comparison:**

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

**With a truthy test:**

```
let running = true
let count = 0

loop while running
    inc count
    if count == 3 then
        let running = false
    end
end

print count    // 3
```

### Nested loops

Loops can be placed inside other loops:

```
let row = 1
loop while row <= 3
    let col = 1
    loop while col <= 4
        println "* "
        inc col
    end
    print ""
    inc row
end
```

Output:
```
* * * * 
* * * * 
* * * * 
```

> **Note:** The VM enforces a limit of **10 million backward jumps** per run to prevent infinite loops from hanging NusaOS. If a loop hits this limit, the program stops with an error message.

---

## 11. Functions

A function is a named block of code you can call from anywhere in your program. Functions let you avoid repeating the same code, and they make large programs much easier to organize.

### Defining a function

```
func function_name
    // body
endfunc
```

Every function is opened with `func` and closed with `endfunc`.

### Function with parameters

Parameters are values passed in from the caller.

```
func greet name
    println "Hello, "
    print name
endfunc
```

### Calling a function

```
let user = "Dava"
call greet user
```

Arguments are matched by position — the first one in `call` goes to the first parameter in `func`.

### Function with a return value

Add `->` followed by a variable name to return a value:

```
func add a b -> result
    let result = 0
    add result a
    add result b
endfunc
```

Calling it and capturing the result:

```
let x = 10
let y = 25
call add x y -> sum
print sum    // 35
```

### return — exit early

Use `return` to leave a function before reaching `endfunc`:

```
func check_positive n
    if n > 0 then
        print "positive"
        return
    end
    print "zero or negative"
endfunc
```

### Local variables

Variables declared inside a function are **local** — they are invisible outside the function and do not affect globals with the same name.

```
let x = 100     // global variable

func example a
    let x = 999     // this is a LOCAL x — completely separate
    print x         // prints 999
endfunc

call example x
print x             // still prints 100 — unchanged
```

### Limits

- Functions cannot be defined inside other functions
- Maximum 64 local variables per function
- Maximum call depth: 64 nested calls
- A function must be defined before it is called (define above the `call`)

### Multiple functions working together

```
// math_utils.nsa

func square n -> result
    let result = 0
    copy result n
    mul result n
endfunc

func absolute n -> result
    let result = 0
    copy result n
    if n < 0 then
        neg result
    end
endfunc

func max_of a b -> result
    let bigger = false
    copy result a
    cmp bigger b > a
    if bigger then
        copy result b
    end
endfunc

// --- main ---
let n = 7
call square n -> sq
println "7 squared = "
print sq           // 49

let neg_val = -15
call absolute neg_val -> abs
println "|-15| = "
print abs          // 15

let p = 30
let q = 42
call max_of p q -> biggest
println "max(30, 42) = "
print biggest      // 42
```

---

## 12. Arrays

An array is an ordered list of values — all of the same type — stored under one name.

### Declaring an array

```
arr int  scores 5    // integer array, 5 elements, initialized to 0
arr str  names  3    // string array, 3 elements, initialized to ""
arr bool flags  4    // bool array, 4 elements, initialized to false
```

Syntax: `arr <type> <name> <size>`

- Type must be `int`, `str`, or `bool`
- Size must be a positive integer literal, maximum **64**
- Arrays are global — they cannot be declared inside a function

### Setting elements — aset

```
arr int scores 5

aset scores 0 100    // scores[0] = 100
aset scores 1 85     // scores[1] = 85

let i = 2
let val = 92
aset scores i val    // scores[2] = 92  — variable index and value
```

The index can be a variable or a literal. Literals are bounds-checked at compile time. Variables are bounds-checked at runtime.

### Getting elements — aget

```
aget x scores 0      // x = scores[0]  — literal index

let i = 1
aget x scores i      // x = scores[1]  — variable index
```

The destination variable is declared automatically if it doesn't exist yet, with the element type of the array.

### Getting the size — alen

```
alen n scores    // n = 5  (the declared size)
```

### Iterating over an array

```
arr int scores 5
aset scores 0 100
aset scores 1 85
aset scores 2 92
aset scores 3 78
aset scores 4 95

let i = 0
loop while i < 5
    aget val scores i
    print val
    inc i
end
```

### Summing an array

```
let sum = 0
let i = 0
loop while i < 5
    aget x scores i
    add sum x
    inc i
end
print sum    // 450
```

### Searching for a value in a string array

```
arr str fruits 3
aset fruits 0 "apple"
aset fruits 1 "banana"
aset fruits 2 "cherry"

let target = "banana"
let found = false
let i = 0
loop while i < 3
    aget item fruits i
    cmp match item == target
    if match then
        let found = true
    end
    inc i
end

if found then
    print "found it!"
end
```

### Limits

- Maximum element count per array: **64**
- Maximum total variables (including array slots): **200**
- Arrays are not supported inside functions

---

## 13. Full Program Examples

### Interactive calculator

```
// calc.nsa

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

func do_mod a b -> result
    let result = 0
    copy result a
    mod result b
endfunc

let running = 1
let a = 0
let b = 0
let op = 0
let answer = 0

print "================================"
print "    NSA Calculator"
print "      on NusaOS  :)"
print "================================"

loop while running != 0
    print ""
    print "  1 = Add       ( + )"
    print "  2 = Subtract  ( - )"
    print "  3 = Multiply  ( * )"
    print "  4 = Divide    ( / )"
    print "  5 = Modulo    ( % )"
    print "  0 = Exit"
    print ""
    println "Choose: "
    input op

    if op == 0 then
        let running = 0
    end

    if op == 1 then
        println "First number  : "
        input a
        println "Second number : "
        input b
        call do_add a b -> answer
        println "= "
        print answer
        print "--------------------------------"
    end

    if op == 2 then
        println "First number  : "
        input a
        println "Second number : "
        input b
        call do_sub a b -> answer
        println "= "
        print answer
        print "--------------------------------"
    end

    if op == 3 then
        println "First number  : "
        input a
        println "Second number : "
        input b
        call do_mul a b -> answer
        println "= "
        print answer
        print "--------------------------------"
    end

    if op == 4 then
        println "First number  : "
        input a
        println "Second number : "
        input b
        call do_div a b -> answer
        println "= "
        print answer
        print "--------------------------------"
    end

    if op == 5 then
        println "First number  : "
        input a
        println "Second number : "
        input b
        call do_mod a b -> answer
        println "= "
        print answer
        print "--------------------------------"
    end
end

print ""
print "Goodbye! — NSA Calculator"
```

---

### FizzBuzz (1 to 20)

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

### Factorial using a function

```
// factorial.nsa
func factorial n -> result
    let result = 1
    loop while n != 0
        mul result n
        dec n
    end
endfunc

let num = 6
call factorial num -> answer
println "6! = "
print answer    // 720
```

---

### Interactive greeter

```
// greeter.nsa
let name = ""
let city = ""
let age = 0

print "================================"
print "   Welcome to NusaOS!"
print "================================"

println "Your name: "
input name

println "Where are you from: "
input city

println "Your age: "
input age

print "--------------------------------"

let greeting = "Hello, "
concat greeting name
concat greeting "!"
print greeting

let info = "From "
concat info city
concat info ", age "
let age_str = ""
to_str age_str age
concat info age_str
concat info "."
print info

print "--------------------------------"
print "Written in NSA Language"
print "running natively on NusaOS."
```

---

### Powers of 2

```
// power2.nsa
let base = 2
let exp = 10
let result = 1

loop while exp != 0
    mul result base
    dec exp
end

println "2^10 = "
print result    // 1024
```

---

### Countdown with user input

```
// countdown.nsa
println "Count down from: "
let n = 0
input n

loop while n != 0
    print n
    dec n
end

print "Done!"
```

---

### Bool logic demo

```
// logic.nsa
let a = 5
let b = 10
let c = 5

cmp eq1 a == c    // true  — a equals c
cmp lt1 a <  b    // true  — a is less than b
and both eq1 lt1  // true  — both are true

print eq1     // true
print lt1     // true
print both    // true
```

---

## 14. The .nbin File Format

A `.nbin` file is the compiled bytecode output. You don't need to understand this to write NSA programs — it's only relevant if you're building tools that read `.nbin` files directly.

```
Offset   Size   Description
------   ----   -----------
0        6      Magic: 7F 4E 53 41 02 00  (\x7fNSA\x02\x00)
6        1      Variable count (uint8)
7        2      Bytecode size in bytes (little-endian uint16)
9        N      Bytecode
```

The magic bytes are intentionally different from NusaLang v1 (`\x7fNUSA\x01`). Old `.nbin` files are rejected with a clear error message rather than silently misbehaving.

---

## 15. Error Reference

### Compile-time errors

These are caught by `nsa build` before the program ever runs.

```
program.nsa:5: error: undeclared variable 'x'
```
You used a variable before declaring it with `let`.

---

```
program.nsa:3: error: 'score' was previously declared as int, cannot redeclare as string
```
A variable's type cannot change after it's first declared.

---

```
program.nsa:8: error: unclosed 'if' block (missing 'end')
```
An `if` block was never closed with `end`.

---

```
program.nsa:12: error: division/modulo by zero
```
The compiler caught a division by the literal `0`.

---

```
program.nsa:1: error: 'let' is a reserved keyword
```
You tried to use a keyword as a variable name.

---

```
program.nsa:20: error: function 'add' expects 2 argument(s), got 1
```
The number of arguments in `call` doesn't match the function definition.

---

```
program.nsa: error: unclosed 'func' block (missing 'endfunc')
```
A `func` was never closed with `endfunc`.

---

```
program.nsa:15: error: unknown function 'calculate'
```
You called a function that doesn't exist, or defined it after the `call`.

---

### Runtime errors

These appear when `nsa run` is executing the program.

```
nsa run: runtime error at offset 42: division/modulo by zero
```
The program tried to divide by a variable that turned out to be zero.

---

```
nsa run: runtime error at offset 17: ARITH_IMM: var 3 is not an integer
```
An arithmetic operation was used on a non-integer variable.

---

```
nsa run: runtime error at offset 0: infinite loop detected (limit 10M iterations)
```
A loop ran more than 10 million times.

---

```
nsa run: 'program.nbin': not a valid .nbin file (wrong magic or version)
```
The file isn't a valid NSA binary, or was compiled by an older incompatible version.

---

```
nsa run: 'program.nbin': No such file or directory
```
The file doesn't exist or the name is wrong. NusaOS is case-sensitive — `Prog.nbin` and `prog.nbin` are different files.

---

```
nsa run: runtime error at offset 9: call stack overflow (max depth 64)
```
Functions are calling other functions more than 64 levels deep.

---

## 16. Keyword Reference

Every reserved word in NSA, grouped by purpose.

### Variables & types

| Keyword | What it does |
|---------|-------------|
| `let` | Declare a variable |
| `copy` | Copy a value from one variable to another |
| `true` | Bool literal — true |
| `false` | Bool literal — false |

### Output & Input

| Keyword | What it does |
|---------|-------------|
| `print` | Print a value with a newline |
| `println` | Print a value without a newline |
| `input` | Read a value from the keyboard |

### Arithmetic

| Keyword | What it does |
|---------|-------------|
| `add` | Addition |
| `sub` | Subtraction |
| `mul` | Multiplication |
| `div` | Integer division |
| `mod` | Modulo (remainder) |
| `inc` | Add 1 to a variable |
| `dec` | Subtract 1 from a variable |
| `neg` | Negate (flip the sign) |

### Logic & Comparison

| Keyword | What it does |
|---------|-------------|
| `not` | Flip a bool or int (0↔1, true↔false) |
| `cmp` | Compare two integers or two strings → store result as bool |
| `and` | Logical AND of two values |
| `or` | Logical OR of two values |

### Strings

| Keyword | What it does |
|---------|-------------|
| `concat` | Append a string or literal to a string variable |
| `len` | Get the length of a string |
| `to_str` | Convert an integer to its string form |
| `to_int` | Parse a string as an integer |
| `strcmp` | Compare two strings → int (0 = equal, <0, >0) |
| `strfind` | Find first occurrence of a substring → index or -1 |
| `strtrim` | Strip leading and trailing whitespace |
| `strupper` | Convert string to uppercase |
| `strlower` | Convert string to lowercase |
| `strreplace` | Replace first occurrence of a substring |
| `strsplit` | Split a string by delimiter into a string array |

### Arrays

| Keyword | What it does |
|---------|-------------|
| `arr` | Declare an array: `arr int scores 5` |
| `aget` | Get an element: `aget dst array_name idx` |
| `aset` | Set an element: `aset array_name idx value` |
| `alen` | Get the declared size of an array: `alen n array_name` |

### Control flow

| Keyword | What it does |
|---------|-------------|
| `if` | Start a conditional block |
| `then` | Marks the end of an if condition |
| `else` | Alternative branch |
| `end` | Close an if or loop block |
| `loop` | Start a loop |
| `while` | Keyword for conditional loops |
| `times` | Keyword for fixed-count loops |
| `break` | Exit the innermost loop immediately |
| `continue` | Skip to the next loop iteration |

### System & Process

| Keyword | What it does |
|---------|-------------|
| `getpid` | Get the current process ID |
| `sleep` | Pause for N milliseconds |
| `getenv` | Read an environment variable by name |
| `peek` | Read a 32-bit integer from a memory address |
| `poke` | Write a 32-bit integer to a memory address |
| `peek8` | Read a byte from a memory address |
| `poke8` | Write a byte to a memory address |
| `fork` | Create a child process |
| `exec` | Replace the current process with another program |
| `waitpid` | Wait for a child process to exit |

### Functions

| Keyword | What it does |
|---------|-------------|
| `func` | Define a function |
| `endfunc` | Close a function definition |
| `return` | Exit a function early |
| `call` | Call a function |

---

---

## 17. Module System (.nss files)

NSA v2.4 introduces the **NSS Module System** — a way to split reusable code into
separate `.nss` (NSA Shared Source) files that can be imported by any `.nsa` program.

### What is a .nss file?

A `.nss` file is a self-contained module that exports **global constants** and
**functions**. Think of it as a `.h` and `.c` file combined into one, but with
automatic dead-code elimination (tree-shaking): only the symbols your program
actually uses are included in the final `.nbin`.

### Writing a .nss module

```nss
// math.nss — basic math module

global int  PI_X100  = 314      // exported integer constant
global str  APP_NAME = "nusa"   // exported string constant
global bool DEBUG    = false    // exported bool constant

// exported function — same syntax as in .nsa
func max a b -> result
    let result = b
    cmp is_greater a > b
    if is_greater then
        copy result a
    end
endfunc

func abs n -> result
    let result = n
    cmp is_negative n < 0
    if is_negative then
        neg result
    end
endfunc
```

**Rules for .nss files:**
- Top-level statements may only be `global` declarations or `func`/`endfunc` blocks.
- Function names must not clash with NSA keywords (`add`, `sub`, `mul`, etc.).
  Use a prefix such as `math_add` to avoid conflicts.
- All globals and functions are automatically exported — there is no `export` keyword.

### Importing a module

Place the `.nss` file in the same directory as your `.nsa` source, then add an
`import` statement at the top of your program:

```nsa
import "math"          // loads math.nss from the same directory
import "libs/strings"  // sub-paths are supported
```

The compiler searches for the module file in:
1. The directory of the `.nsa` source file.
2. Any extra directories passed via `--nss-path <dir[:dir...]>`.

### Using imported symbols

```nsa
import "math"
import "strutil"

// Read a module global into a local variable first
let sep    = strutil.SEPARATOR
let pi_val = math.PI_X100

// Call an imported function
call math.max x y -> largest
call math.abs neg_num -> pos_num
```

> **Note:** Module globals must be copied into a local variable with `let` before
> they can be passed to other statements. Direct use in `print`, `add`, etc. is
> not yet supported.

### Tree-shaking (dead-code elimination)

The compiler automatically removes any module function or global that is never
referenced by your program. This keeps `.nbin` files small even when importing
large modules.

```nsa
import "math"   // math.nss exports 6 functions

call math.max x y -> r   // only max() is used

// Result: only max() bytecode is linked into the .nbin.
// The other 5 functions are silently dropped.
```

### Validating a module

```sh
nsa build-nss math.nss
```

Output example:
```
nsa build-nss: OK  module 'math.nss'  6 function(s)  3 global(s)
  func  abs       (1 param(s) → return)
  func  math_add  (2 param(s) → return)
  func  max       (2 param(s) → return)
  ...
  global int  PI_X100
  global int  VERSION
```

### Keyword conflicts

The following NSA keywords **cannot** be used as function names inside a `.nss`
file: `add`, `sub`, `mul`, `div`, `mod`, `inc`, `dec`, `neg`, `not`, `len`,
`concat`, `copy`, `input`, `print`, `println`, `if`, `else`, `end`, `loop`,
`while`, `times`, `func`, `endfunc`, `return`, `call`, `and`, `or`, `cmp`,
`let`, `arr`, `aget`, `aset`, `alen`, `to_int`, `to_str`.

Use a descriptive prefix instead — e.g. `math_add`, `str_len`, `vec_mul`.

### Supported statements inside .nss functions

All statements that work in a `.nsa` function body are also supported inside
`.nss` functions: `let`, `copy`, `print`, `println`, `add`, `sub`, `mul`,
`div`, `mod`, `inc`, `dec`, `neg`, `not`, `cmp`, `and`, `or`, `if`/`else`/`end`,
`loop`, `concat`, `len`, `to_str`, `to_int`, `return`, and `call` (intra-module).

---



---

## 18. Floating Point

NSA v2.5 adds native floating-point support. Float variables are declared with a decimal literal and use a separate set of operations prefixed with `f`.

### Declaring float variables

```nsa
let pi    = 3.14159
let r     = 4.0
let score = -1.5
let zero  = 0.0
```

Any literal containing a decimal point (`.`) is automatically treated as a float. Negative float literals are written as `let x = -3.14`.

### Float arithmetic

```nsa
let a = 10.0
let b = 3.0
let result = 0.0

fadd result a b     // result = a + b  →  13.0
fsub result a b     // result = a - b  →  7.0
fmul result a b     // result = a * b  →  30.0
fdiv result a b     // result = a / b  →  3.33333
fneg result         // result = -result  (in-place negate)
```

`fdiv` raises a runtime error on division by zero.

### Conversion

```nsa
let n   = 7
let f   = 0.0
let i   = 0
let s   = ""

itof f n      // int variable → float variable
ftoi i f      // float → int (truncates toward zero)
ftos s f      // float → string  ("3.14159", "33.3333", etc.)
```

### Comparison

```nsa
let a    = 3.14
let b    = 2.71
let big  = false

fcmp big a > b      // big = (a > b)  →  true

// Supported operators: == != < > <= >=
fcmp eq a == b
fcmp ne a != b
fcmp lt a <  b
fcmp ge a >= b
```

### Printing floats

```nsa
let pi = 3.14159
print pi            // prints: 3.14159
println pi          // prints without trailing newline
```

Floats are printed with up to 6 significant digits and trailing zeros trimmed (`33.3333`, `50.2654`, `1000000`, `0.001`).

### Full example — circle area

```nsa
let pi   = 3.14159
let r    = 5.0
let luas = 0.0

fmul luas r r       // r²
fmul luas pi luas   // π × r²

ftos s luas
print "Area = "
print s
```

---

## 19. String Indexing

NSA v2.5 adds three statements for character-level string access.

### sget — get character at index

```nsa
let s = "nusaOS"
let i = 0
let c = ""

sget c s i      // c = "n"   (character at index 0)
let i = 3
sget c s i      // c = "a"
```

The result is always a 1-character string. Index out of bounds causes a runtime error.

### sset — replace character at index

```nsa
let s = "nusaOS"
let i = 5
let x = "!"

sset s i x      // s is now "nusaO!"
```

`sset` modifies the string variable in-place. The source must be a non-empty string; only its first character is used.

### ssub — extract substring

```nsa
let s     = "nusaOS"
let start = 0
let len   = 4
let part  = ""

ssub part s start len   // part = "nusa"
```

If `start + len` exceeds the string length, the substring is clamped to the end of the string. Result is capped at 254 characters.

### Combined example

```nsa
let word  = "Hello"
let i     = 0
let ch    = ""
let upper = "h"

// Read first char
sget ch word i

// Replace it
sset word i upper   // word = "hello"

// Extract middle
let s2 = 1
let ln = 3
let mid = ""
ssub mid word s2 ln  // mid = "ell"

print word
print mid
```

---

## 20. File I/O

NSA v2.5 adds file read/write support using the `fopen`, `fclose`, `fread`, `fwrite`, and `fexists` statements.

### Opening a file

```nsa
let path = "/home/notes.txt"
fopen f path "w"    // open for writing  (creates or truncates)
fopen f path "r"    // open for reading
fopen f path "a"    // open for appending
```

| Mode | Meaning |
|------|---------|
| `"r"` | Read-only. File must exist. |
| `"w"` | Write. Creates file if missing, truncates if it exists. |
| `"a"` | Append. Creates file if missing. Writes go to end. |

The file handle `f` is stored as a special `file` variable. A negative internal value means the open failed (e.g. file not found in `"r"` mode).

### Writing to a file

```nsa
fwrite f "Hello, nusaOS!
"

let msg = "Line 2
"
fwrite f msg
```

Both string literals and string variables are accepted.

### Reading from a file

```nsa
fopen  g path "r"
fread  content g    // reads entire file into string variable
fclose g
print content
```

`fread` reads up to 254 characters (the string length limit). For larger files only the first 254 characters are kept.

### Closing a file

```nsa
fclose f
```

Always close a file after use. The handle becomes invalid after `fclose`.

### Checking if a file exists

```nsa
let path = "/home/data.txt"
fexists ok path

if ok then
    print "File found"
end
```

`fexists` sets the destination variable to `true` or `false` without opening the file.

### Full example

```nsa
let path = "/home/log.txt"

// Write
fopen  f path "w"
fwrite f "Start
"
fclose f

// Append
fopen  f path "a"
fwrite f "More data
"
fclose f

// Read back
fexists ok path
if ok then
    fopen  g path "r"
    fread  content g
    fclose g
    print content
end
```

---

## 21. System Calls & Process Control (v2.5.2)

NSA v2.5.2 exposes operating system primitives directly — process IDs, sleeping, environment variables, raw memory access, and process spawning.

### getpid — get current process ID

```nsa
let pid = 0
getpid pid      // pid = process ID of the running program
print pid
```

### sleep — pause execution

Pauses the program for a given number of milliseconds.

```nsa
print "waiting..."
sleep 1000      // sleep for 1 second
print "done"
```

The argument can be a variable or a literal integer.

### getenv — read an environment variable

```nsa
let path = ""
getenv path "PATH"      // reads $PATH into path
print path
```

If the variable does not exist, the result is an empty string.

### peek / poke — raw memory access

Read or write a 32-bit integer at an absolute memory address. Use with care.

```nsa
let addr  = 0x00400000
let value = 0

peek value addr     // value = *(int*)addr
poke addr 42        // *(int*)addr = 42
```

`peek8` and `poke8` work the same way but read/write a single byte.

```nsa
peek8 value addr    // value = *(uint8_t*)addr
poke8 addr 0xFF
```

### fork — create a child process

Creates a copy of the current process. Returns `0` in the child and the child's PID in the parent.

```nsa
let pid = 0
fork pid

if pid == 0 then
    print "I am the child"
else
    print "I am the parent"
end
```

### exec — replace process image

Replaces the current process with a new program. The current process does not continue after a successful `exec`.

```nsa
let prog = "/bin/ls"
exec prog               // run ls with no arguments

// with arguments:
exec prog "-l" "/home"
```

Up to 15 arguments may be passed after the program path.

### waitpid — wait for a child process to finish

```nsa
let child_pid = 0
fork child_pid

if child_pid == 0 then
    exec "/bin/echo" "hello from child"
else
    let status = 0
    waitpid child_pid status    // blocks until child exits
    println "child exited, status: "
    print status
end
```

---

## 22. Advanced String Operations (v2.5.3)

### strcmp — compare two strings

Returns an integer: `0` if equal, negative if `a < b`, positive if `a > b`.

```nsa
let sa = "hello"
let sb = "hello"
let sc = "world"
let cmp1 = 0
let cmp2 = 0

strcmp cmp1 sa sb     // cmp1 = 0  (equal)
strcmp cmp2 sa sc     // cmp2 = negative  (h < w)

if cmp1 == 0 then
    print "strings are equal"
end
```

Unlike `cmp`, `strcmp` gives you a numeric result — useful when you need to know the ordering, not just equality.

### strfind — search inside a string

Finds the first occurrence of a needle in a haystack. Returns the 0-based index, or `-1` if not found.

```nsa
let hay = "hello nusaOS"
let idx = 0

strfind idx hay "nusa"    // idx = 6
strfind idx hay "xyz"     // idx = -1

if idx != -1 then
    print "found!"
end
```

### strtrim — strip whitespace

Removes leading and trailing whitespace from a string.

```nsa
let dirty = "  hello nusa  "
let clean = ""

strtrim clean dirty     // clean = "hello nusa"
print clean
```

### strupper / strlower — change case

```nsa
let msg   = "Hello NusaOS"
let upper = ""
let lower = ""

strupper upper msg    // upper = "HELLO NUSAOS"
strlower lower msg    // lower = "hello nusaos"

print upper
print lower
```

### strreplace — replace first occurrence

Replaces the first occurrence of a substring with another.

```nsa
let base   = "I love NusaLang"
let result = ""

strreplace result base "NusaLang" "NusaOS"
print result    // I love NusaOS
```

If the old string is not found, result is a copy of the original.

### strsplit — split a string into an array

Splits a string by a delimiter and stores the parts in a string array. The array must be declared first with `arr str`.

```nsa
let csv = "one,two,three"
arr str parts 5
let count = 0

strsplit parts csv ","

alen count parts    // count = 3

let i0 = 0
let i1 = 1
let i2 = 2
let p0 = ""
let p1 = ""
let p2 = ""

aget p0 parts i0    // p0 = "one"
aget p1 parts i1    // p1 = "two"
aget p2 parts i2    // p2 = "three"

print p0
print p1
print p2
```

---

## 23. Loop Control — break and continue (v2.5.3)

### break — exit a loop early

`break` immediately jumps out of the innermost enclosing loop.

```nsa
let i = 0
loop while i < 10
    inc i
    if i == 5 then
        break       // stop when i reaches 5
    end
end

print i    // 5
```

### continue — skip to the next iteration

`continue` skips the rest of the current iteration and jumps back to the loop condition check.

```nsa
let i  = 0
let sum = 0
loop while i < 6
    inc i
    if i == 3 then
        continue    // skip 3
    end
    add sum i
end

print sum    // 18  (1+2+4+5+6)
```

Both `break` and `continue` work inside `loop while` and `loop N times`. They apply to the **innermost** loop only — to break out of nested loops, set a flag variable.

---

## About Versions

To see which version of NSA is installed on your system, run:

```sh
nsa version
```

The language is actively developed as part of the NusaOS project. This documentation covers NSA v2.5.3 and all currently supported features — if a feature is listed here, it works in your installed version.

---

*NSA is part of the NusaOS project — a hobby operating system written from scratch in C++.*