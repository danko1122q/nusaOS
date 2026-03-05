# NusaLang — Language Documentation

NusaLang is a programming language that runs natively on NusaOS. You write code in a `.nusa` file, compile it to a `.nbin` bytecode binary, and execute it with the NusaLang runtime. It's minimal by design, but fully functional.

---

## Getting Started

```sh
nusa-build program.nusa program.nbin   # compile
nusa-run program.nbin                  # run
```

Both binaries are included in NusaOS. Nothing extra to install.

---

## File Structure

A `.nusa` file is plain text — one statement per line. No semicolons, no curly braces, no mandatory indentation (though indenting your blocks is strongly recommended for readability).

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

All variables must be declared with `let` before use. There are two types: **integer** and **string**.

### Integer

```
let x = 42
let year = 2024
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
| `\\`   | Backslash       |
| `\"`   | Double quote    |

### Re-assigning Variables

You can reassign a variable using `let` again, as long as the type stays the same:

```
let x = 10
let x = 99   // valid — x is now 99
```

Changing the type is not allowed:

```
let x = 10
let x = "text"   // ERROR: x was already declared as int
```

Maximum variables per program: **200**.

---

## Print

Prints a value to the screen.

```
print "some text"
print variable_name
```

Every `print` automatically adds a newline at the end. You can only print one value per statement — there is no way to print multiple things on the same line.

Example:

```
let os = "NusaOS"
let version = 1

print "OS Name:"
print os
print "Version:"
print version
```

Output:
```
OS Name:
NusaOS
Version:
1
```

---

## Arithmetic

Math operations only work on integer variables. The syntax is always `operation variable value` — the result is stored back into the same variable.

```
add x 10      // x = x + 10
sub x 5       // x = x - 5
mul x 3       // x = x * 3
div x 2       // x = x / 2
```

The second operand can also be another variable:

```
let a = 10
let b = 3
add a b       // a = a + b  =>  a = 13
```

A few things to keep in mind:

- Division is **integer division** — the result is truncated. `7 / 2 = 3`, not `3.5`.
- Dividing by zero causes a runtime error and the program stops immediately.
- Arithmetic cannot be performed on string variables.

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

Supported operators:

| Operator | Meaning       |
|----------|---------------|
| `==`     | Equal to      |
| `!=`     | Not equal to  |

The comparison value must be an **integer literal**. You cannot compare two variables directly, and you cannot compare strings.

```
if age == 17 then
  print "seventeen"
end

if status != 0 then
  print "active"
end
```

Every `if` block must be closed with `end`.

---

## Loops

NusaLang has two kinds of loops.

### loop N times

Runs the block exactly N times. N must be a positive integer literal.

```
loop 5 times
  print "hello"
end
```

The compiler handles the counter internally — you don't need to manage it yourself.

### loop while

Runs the block as long as the condition holds.

```
let i = 10
loop while i != 0
  print i
  sub i 1
end
```

The condition is checked **before** entering the loop body. If the condition is already false when the loop is reached, the body will not execute at all.

Available operators:

| Operator | Meaning              |
|----------|----------------------|
| `!= N`   | While not equal to N |
| `== N`   | While equal to N     |

Example of a loop that never runs:

```
let x = 5
loop while x == 0   // x is 5, not 0 — skipped immediately
  print "this will never print"
end
```

### Iteration Limit

The VM has a hard limit of **1,000,000 backward jumps** per execution. If a loop exceeds this, the program halts with:

```
nusa-run: runtime error: infinite loop detected
```

This is intentional — it prevents programs from freezing NusaOS. If your program genuinely needs more than a million iterations, the limit can be raised by modifying the VM source and recompiling.

---

## Full Program Examples

### Factorial of 5

```
// factorial.nusa
let n = 5
let result = 1

loop while n != 0
  mul result n
  sub n 1
end

print "5! ="
print result
```

Output: `120`

---

### Countdown

```
// countdown.nusa
let seconds = 10

loop while seconds != 0
  print seconds
  sub seconds 1
end

print "Done!"
```

---

### Simple FizzBuzz (1–15)

NusaLang doesn't have a modulo operator, so this uses a manual counter approach:

```
// fizzbuzz.nusa
let i = 1
let fizz = 3
let buzz = 5

loop while i != 16
  print i
  sub fizz 1
  sub buzz 1
  if fizz == 0 then
    print "Fizz"
    let fizz = 3
  end
  if buzz == 0 then
    print "Buzz"
    let buzz = 5
  end
  add i 1
end
```

---

### Power of 2 (2^8)

```
// power.nusa
let base = 2
let exp = 8
let result = 1

loop while exp != 0
  mul result base
  sub exp 1
end

print "2^8 ="
print result
```

Output: `256`

---

## Current Limitations

This is what the language doesn't support yet — useful to know if you're looking to contribute or extend it:

- No functions or procedures
- No arrays
- No user input — output only
- No modulo or power operators
- No greater-than / less-than comparisons (`>`, `<`, `>=`, `<=`)
- No string concatenation
- No nested loops (a loop inside a loop is not yet supported)
- Cannot print multiple values on a single line
- Maximum bytecode size per program: **65,535 bytes**

---

## .nbin File Format

For anyone building tooling around NusaLang, here's the binary layout of a `.nbin` file:

```
Offset   Size   Description
------   ----   -----------
0        6      Magic bytes: 7F 4E 55 53 41 03  (\x7fNUSA\x03)
6        1      Variable count (sym_count)
7        2      Bytecode size in bytes (little-endian uint16)
9        N      Bytecode
```

---

## Error Reference

### Compile-time errors

Printed by `nusa-build` in this format:

```
filename.nusa:12: error: undeclared variable 'x'
filename.nusa:7: error: 'name' was previously declared as string
filename.nusa:3: error: unclosed 'if' (missing 'end')
```

### Runtime errors

Printed by `nusa-run`:

```
nusa-run: runtime error: division by zero
nusa-run: runtime error: infinite loop detected
nusa-run: runtime error: unknown opcode 0xXX at offset N
nusa-run: 'file.nbin': not a valid .nbin file
nusa-run: 'file.nbin': truncated bytecode
```

---

*NusaLang is part of the NusaOS project — a hobby operating system written from scratch.*