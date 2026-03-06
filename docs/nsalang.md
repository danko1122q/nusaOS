# NSA Language — Documentation v2.0

NSA is a programming language that runs natively on NusaOS. Write code in a `.nsa` file, compile it to a `.nbin` bytecode binary, and execute it with the NSA runtime. Minimal by design, but fully functional — with a real lexer, compiler, and bytecode VM all written in C++.

---

## Getting Started

```sh
nsa build program.nsa           # compile → program.nbin (auto-named)
nsa build program.nsa out.nbin  # compile → out.nbin (explicit output)
nsa run program.nbin            # execute
nsa help                        # full language reference
nsa version                     # print version info
```

Everything is in one binary (`nsa`). Nothing extra to install.

---

## File Structure

A `.nsa` file is plain text — one statement per line. No semicolons, no curly braces, no mandatory indentation (though indenting your blocks is strongly recommended for readability).

```
// this is a comment
# this is also a comment

let x = 10
print x
```

---

## Comments

Two styles, both valid:

```
// C-style comment
# shell-style comment
```

Comments are single-line only. There is no multi-line comment syntax.

---

## Variables

All variables must be declared with `let` before use. There are three types: **integer**, **string**, and **bool**.

### Integer

```
let x = 42
let year = 2026
let negative = -100
```

Range: −2,147,483,648 to 2,147,483,647 (signed 32-bit integer).

### String

```
let name = "NusaOS"
let message = "Hello, world!"
```

Maximum string length: **254 characters**.

Supported escape sequences inside strings:

| Escape | Result          |
|--------|-----------------|
| `\n`   | Newline         |
| `\t`   | Tab             |
| `\r`   | Carriage return |
| `\0`   | Null byte       |
| `\\`   | Backslash       |
| `\"`   | Double quote    |
| `\'`   | Single quote    |

### Bool

```
let flag = true
let done = false
```

Booleans print as `true` or `false`, not `0` or `1`. They can be used in conditions and logic operations.

### Re-assigning Variables

You can reassign a variable using `let` again, as long as the type stays the same:

```
let x = 10
let x = 99   // valid — x is now 99
```

Changing the type is not allowed:

```
let x = 10
let x = "text"   // ERROR: x was previously declared as int
```

### Copying Variables

```
let y = x          // copy via let (type is inferred from x)
copy dst src       // explicit copy — any type
```

Maximum variables per program: **200**.

---

## Output

### print

Prints a value followed by a newline.

```
print "some text"
print variable_name
```

### println

Prints a value **without** a trailing newline. Useful for building prompts or inline output.

```
println "Enter your name: "
input name
println "Hello, "
print name
```

---

## Input

Reads a value from stdin into a variable.

```
input x
```

- If `x` is an **integer**, reads a number.
- If `x` is a **string**, reads an entire line.
- If `x` is not yet declared, it is automatically declared as a **string**.

```
let score = 0
input score        // reads integer from stdin

let username = ""
input username     // reads string line from stdin
```

---

## Arithmetic

Math operations only work on integer variables. The result is always stored back into the destination variable.

```
add x 10      // x = x + 10
sub x 5       // x = x - 5
mul x 3       // x = x * 3
div x 2       // x = x / 2
mod x 7       // x = x % 7  (remainder)
```

The second operand can also be another variable:

```
let a = 10
let b = 3
add a b       // a = 13
mod a b       // a = 1
```

### Increment / Decrement

```
inc x         // x = x + 1
dec x         // x = x - 1
```

### Negate

```
neg x         // x = -x
```

A few things to keep in mind:

- Division is **integer division** — result is truncated. `7 / 2 = 3`, not `3.5`.
- Dividing or taking modulo by zero causes a runtime error.
- Arithmetic cannot be performed on string or bool variables.

---

## Logic & Comparison

### not

Flips a bool or integer between `0` and `1`.

```
let flag = true
not flag        // flag = false

let x = 1
not x           // x = 0
```

### neg

Negates an integer.

```
let x = 5
neg x           // x = -5
```

### cmp

Compares two integer variables and stores the result as a bool.

```
cmp result a == b    // result = (a == b)
cmp result a != b
cmp result a <  b
cmp result a >  b
cmp result a <= b
cmp result a >= b
```

### and / or

Logical AND and OR between any two variables (int, bool, or string — truthy rules apply).

```
and result flag1 flag2    // result = flag1 && flag2
or  result flag1 flag2    // result = flag1 || flag2
```

**Truthy rules:**

| Type   | Falsy when      |
|--------|-----------------|
| int    | value is `0`    |
| bool   | value is `false`|
| string | empty string    |

---

## String Operations

### concat

Appends a string literal or variable to another string variable.

```
let s = "Hello"
concat s ", world"    // s = "Hello, world"

let t = "!"
concat s t            // s = "Hello, world!"
```

### len

Stores the length of a string into an integer variable.

```
let n = 0
len n s       // n = length of s
```

If the destination variable is not yet declared, it is automatically declared as an integer.

### to_str

Converts an integer to its string representation.

```
let n = 42
let s = ""
to_str s n    // s = "42"
```

### to_int

Parses a string as an integer.

```
let s = "100"
let n = 0
to_int n s    // n = 100
```

If the string is not a valid integer, the result is `0`.

---

## Conditionals

### if ... then ... end

```
if x == 10 then
    print "x is ten"
end
```

### if ... then ... else ... end

```
if x == 0 then
    print "zero"
else
    print "not zero"
end
```

### Supported condition forms

**Compare integer to literal:**

```
if x == 10 then ...
if x != 0  then ...
if x <  5  then ...
if x >  5  then ...
if x <= 10 then ...
if x >= 10 then ...
```

**Truthy test (int, bool, or string):**

```
if flag then
    print "flag is set"
end
```

Every `if` block must be closed with `end`.

---

## Loops

### loop N times

Runs the block exactly N times. N must be a positive integer literal.

```
loop 5 times
    print "hello"
end
```

The compiler manages the counter internally — you don't need a variable for it.

### loop while

Runs the block as long as the condition holds. The condition is checked **before** entering the body.

**Compare integer to literal:**

```
let i = 0
loop while i < 10
    print i
    inc i
end
```

**Truthy test:**

```
let running = true
loop while running
    // ...
    let running = false
end
```

All six comparison operators work: `==`, `!=`, `<`, `>`, `<=`, `>=`.

### Nested loops

Loops can be nested inside each other:

```
loop 3 times
    loop 3 times
        println "* "
    end
    print ""
end
```

### Iteration Limit

The VM has a hard limit of **10,000,000 backward jumps** per execution. If a loop exceeds this:

```
nsa-run: runtime error: infinite loop detected (limit 10M iterations)
```

This prevents programs from hanging NusaOS. If you genuinely need more iterations, recompile the VM with `NSA_NO_LOOP_LIMIT` defined or raise `MAX_BACK_JUMPS` in `nsa_vm.cpp`.

---

## Full Program Examples

### Factorial of 5

```
// factorial.nsa
let n = 5
let result = 1

loop while n != 0
    mul result n
    dec n
end

print "5! ="
print result
```

Output: `120`

---

### FizzBuzz (1–20)

Now possible without tricks because NSA v2 has `mod`:

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

### Countdown with input

```
// countdown.nsa
println "Start from: "
let n = 0
input n

loop while n != 0
    print n
    dec n
end

print "Done!"
```

---

### String builder

```
// greet.nsa
let greeting = "Hello, "
println "Your name: "
let name = ""
input name
concat greeting name
concat greeting "!"
print greeting
```

---

### Power of 2

```
// power.nsa
let base = 2
let exp  = 8
let result = 1

loop while exp != 0
    mul result base
    dec exp
end

print "2^8 ="
print result
```

Output: `256`

---

### Bool logic demo

```
// logic.nsa
let a = 5
let b = 10
let c = 5

cmp eq1 a == c    // true  — a equals c
cmp lt1 a <  b    // true  — a less than b
and both eq1 lt1  // true  — both are true

print eq1
print lt1
print both
```

---

## .nbin File Format

```
Offset   Size   Description
------   ----   -----------
0        6      Magic: 7F 4E 53 41 02 00  (\x7fNSA\x02\x00)
6        1      Variable count (sym_count, uint8)
7        2      Bytecode size in bytes (little-endian uint16)
9        N      Bytecode
```

The magic bytes differ from NusaLang v1 (`\x7fNUSA\x01`) intentionally — old `.nbin` files will be rejected with a clear error rather than silently misbehaving.

---

## Error Reference

### Compile-time errors

```
program.nsa:5: error: undeclared variable 'x'
program.nsa:3: error: 'score' was previously declared as int, cannot redeclare as string
program.nsa:8: error: unclosed 'if' block (missing 'end')
program.nsa:12: error: division/modulo by zero
program.nsa:1: error: 'let' is a reserved keyword
```

### Runtime errors

```
nsa run: runtime error at offset 42: division/modulo by zero
nsa run: runtime error at offset 17: ARITH_IMM: var 3 is not an integer
nsa run: runtime error at offset 0: infinite loop detected (limit 10M iterations)
nsa run: runtime error at offset 9: unknown opcode 0xAB
nsa run: 'file.nbin': not a valid .nbin file (wrong magic or version)
nsa run: 'file.nbin': truncated bytecode
```

Runtime errors include the **bytecode offset** where the fault occurred, making it easier to correlate with compiled output.

---

## What's New in v2 vs v1

| Feature              | v1 (NusaLang) | v2 (NSA)        |
|----------------------|---------------|-----------------|
| CLI                  | `nusa-build` + `nusa-run` | `nsa build` / `nsa run` |
| Bool type            | ✗             | ✓ `true`/`false`|
| Operators            | `==` `!=`     | All 6: `==` `!=` `<` `>` `<=` `>=` |
| Modulo               | ✗             | ✓ `mod`         |
| inc / dec            | ✗             | ✓               |
| neg / not            | ✗             | ✓               |
| String concat        | ✗             | ✓ `concat`      |
| String length        | ✗             | ✓ `len`         |
| Type conversion      | ✗             | ✓ `to_str` `to_int` |
| Comparison to bool   | ✗             | ✓ `cmp`         |
| Logical and/or       | ✗             | ✓               |
| User input           | ✗             | ✓ `input`       |
| println (no newline) | ✗             | ✓               |
| Copy between vars    | ✗             | ✓ `copy`        |
| Nested loops         | ✗             | ✓               |
| Loop limit           | 1,000,000     | 10,000,000      |
| Runtime error offset | ✗             | ✓               |

---

*NSA is part of the NusaOS project — a hobby operating system written from scratch in C++.*