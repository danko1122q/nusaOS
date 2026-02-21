nusaOS Build Guide (i686)
=========================

Before anything else, you need a cross-compiler that targets i686-pc-nusaos.
Everything else depends on it.

Build flow::

    build toolchain
        |
        v
    generate CMakeToolchain.txt
        |
        v
    cmake configure
        |
        v
    make
        |
        v
    make install
        |
        v
    make img
        |
        v
    make qemu


Toolchain
---------

Run this once. Only redo it if you modify the GCC or Binutils patches::

    cd toolchain
    ARCH=i686 ./build-toolchain.sh

This takes 10-30 minutes and produces the compiler at
toolchain/tools/i686/bin/i686-pc-nusaos-gcc.

Then generate the CMake toolchain file::

    ARCH=i686 ./build-toolchain.sh make-toolchain-file

This reads CMakeToolchain.txt.in and writes build/i686/CMakeToolchain.txt.


Configure
---------

Create the build directory and run CMake::

    cd ..
    mkdir -p build/i686
    cd build/i686
    cmake ../.. -DCMAKE_TOOLCHAIN_FILE=./CMakeToolchain.txt

If CMake complains about a stale cache from a previous run::

    rm -rf CMakeCache.txt CMakeFiles/


Build
-----

Compile the kernel and userland::

    make -j$(nproc)

Install files into the sysroot::

    make install

Pack the sysroot into a bootable disk image::

    make img

Boot the image in QEMU::

    make qemu


Clean build from scratch
------------------------

If you want to throw everything away and start over::

    cd ~/github/os-project/nusaOS
    rm -rf build/i686 && mkdir -p build/i686
    cd toolchain && ARCH=i686 ./build-toolchain.sh make-toolchain-file && cd ..
    cd build/i686
    cmake ../.. -DCMAKE_TOOLCHAIN_FILE=./CMakeToolchain.txt
    make -j$(nproc)
    make install
    make img
    make qemu


Troubleshooting
---------------

Compiler not found
  Check that toolchain/tools/i686/bin/ contains i686-pc-nusaos-gcc.
  If the folder is empty the toolchain build failed midway, run it again.

System unknown to CMake
  You are missing toolchain/CMake/Platform/nusaOS.cmake.
  Copy duckOS.cmake from the same folder and rename it to nusaOS.cmake.

QEMU black screen
  Make sure make install and make img both finished cleanly before running
  make qemu. A missing file in sysroot will cause a silent boot failure.
