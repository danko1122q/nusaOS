# nusaOS Build Guide (i686)

Before anything else, you need a cross-compiler that targets `i686-pc-nusaos`. Everything else depends on it.

---

## Prerequisites

Before building, ensure your host system has all the necessary tools and libraries installed. Run these commands on your terminal:

```bash
sudo apt update
sudo apt install build-essential bison flex libgmp-dev libmpc-dev libmpfr-dev texinfo nasm cmake qemu-system-x86
```

---

## Build Flow

```
install prerequisites (once)
         │
         ▼
   build toolchain
         │
         ▼
generate CMakeToolchain.txt
         │
         ▼
   cmake configure
         │
         ▼
        make
         │
         ▼
   make install
         │
         ▼
     make img
         │
         ▼
    make qemu
```

---

## Toolchain

Run this once. Only redo it if you modify the GCC or Binutils patches:

```bash
cd toolchain
ARCH=i686 ./build-toolchain.sh
```

This takes **10–30 minutes** and produces the compiler at:

```
toolchain/tools/i686/bin/i686-pc-nusaos-gcc
```

Then generate the CMake toolchain file:

```bash
ARCH=i686 ./build-toolchain.sh make-toolchain-file
```

This reads `CMakeToolchain.txt.in` and writes `build/i686/CMakeToolchain.txt`.

---

## Configure

Create the build directory and run CMake:

```bash
cd ..
mkdir -p build/i686
cd build/i686
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=./CMakeToolchain.txt
```

If CMake complains about a stale cache from a previous run:

```bash
rm -rf CMakeCache.txt CMakeFiles/
```

---

## Build

Compile the kernel and userland:

```bash
make -j$(nproc)
```

Install files into the sysroot:

```bash
make install
```

Pack the sysroot into a bootable disk image:

```bash
make img
```

Boot the image in QEMU:

```bash
make qemu
```

---

## Clean Build from Scratch

If you want to throw everything away and start over:

```bash
# Adjust path if necessary
cd /workspaces/nusaOS
rm -rf build/i686 && mkdir -p build/i686
cd toolchain && ARCH=i686 ./build-toolchain.sh make-toolchain-file && cd ..
cd build/i686
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=./CMakeToolchain.txt
make -j$(nproc)
make install
make img
make qemu
```

---

## Troubleshooting

### Package missing
- If `apt install` fails, run `sudo apt update` first.
- Ensure `nasm` is installed, otherwise CMake will fail with: `No CMAKE_ASM_NASM_COMPILER could be found`.

### Compiler not found
- Check that `toolchain/tools/i686/bin/` contains `i686-pc-nusaos-gcc`.
- If the folder is empty, the toolchain build failed midway — run it again.

### System unknown to CMake
- You are missing `toolchain/CMake/Platform/nusaOS.cmake`.
- Copy an existing platform file and rename it to `nusaOS.cmake`.

### QEMU black screen
- Make sure `make install` and `make img` both finished cleanly before running `make qemu`.
- A missing file in sysroot will cause a silent boot failure.
