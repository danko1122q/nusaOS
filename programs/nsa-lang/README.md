# NSA Programming Language

**NSA** (Nusa Shared Assembly/Script) is the native, high-level programming language for **nusaOS**. It is designed to be lightweight, fast, and easy to use, featuring its own bytecode compiler and virtual machine runtime.

## Features
- **Native to nusaOS:** Built specifically to run on the nusaOS kernel.
- **Bytecode Execution:** Compiles `.nsa` source code into optimized `.nbin` binaries.
- **Rich Syntax:** Supports functions, arrays, file I/O, floating-point math, and system calls.
- **No Dependencies:** Everything is packed into a single standalone binary.

## Getting Started

### Compiling a Program
```bash
nsa build hello.nsa
```
### Running a Program
```
nsa run hello.nbin
```

### License
- **This project is licensed under the GNU General Public License v3.0 (GPL-3.0).
- **You must retain the original copyright notice.

- **Any modifications or derivative works must also be open-sourced under the same license.

- **Private or closed-source redistribution is not allowed.

Copyright (c) 2026 Dava (Danko).