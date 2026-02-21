#!/bin/bash
# Gunakan /bin/bash karena /bin/sh (dash) tidak mendukung $BASH_SOURCE atau source.

set -e

# Ambil direktori script dengan cara yang lebih aman
SCRIPTDIR=$(dirname "$(readlink -f "$0")")

# Gunakan titik (.) sebagai pengganti source agar lebih kompatibel
. "$SCRIPTDIR/nusaos.sh"

# Pastikan variabel KERNEL_NAME tersedia
if [ -z "$KERNEL_NAME" ]; then
    KERNEL_NAME="nusak32"
fi

# Check for KVM Support
if [ -z "$USE_KVM" ]; then
  USE_KVM="0"
  if [ -e /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
        HOST_ARCH="$(uname -m)"
        # Perbaikan logika pengecekan arsitektur
        if [ "$ARCH" = "$HOST_ARCH" ] || { [ "$ARCH" = "i686" ] && [ "$HOST_ARCH" = "x86_64" ]; }; then
            USE_KVM="1"
        else
            warn "Host architecture ($HOST_ARCH) does not match guest architecture ($ARCH) - not using kvm."
        fi
  fi
fi

if [ -z "$NUSAOS_IMAGE" ]; then
  NUSAOS_IMAGE="nusaOS.img"
fi

# Determine what acceleration we should use
if [ -z "$NUSAOS_QEMU_ACCEL" ]; then
  if command -v wslpath >/dev/null; then
    NUSAOS_QEMU_ACCEL="-accel whpx,kernel-irqchip=off -accel tcg"
  else
    if [ "$USE_KVM" -ne "0" ]; then
      NUSAOS_QEMU_ACCEL="-enable-kvm"
    else
      NUSAOS_QEMU_ACCEL=""
    fi
  fi
fi

case $ARCH in
  i686)
    QEMU_SYSTEM="i386"
    NUSAOS_QEMU_MEM="512M"
    # Tambahkan format=raw secara eksplisit agar QEMU tidak menebak
    NUSAOS_QEMU_DRIVE="-drive file=$NUSAOS_IMAGE,format=raw,index=0,media=disk"
    NUSAOS_QEMU_DEVICES="-device ac97"
    NUSAOS_QEMU_SERIAL="-serial stdio"
    ;;
  aarch64)
    QEMU_SYSTEM="aarch64"
    NUSAOS_QEMU_MACHINE="-machine raspi3b"
    NUSAOS_QEMU_MEM="1G"
    NUSAOS_QEMU_DRIVE=""
    NUSAOS_QEMU_DEVICES=""
    NUSAOS_QEMU_SERIAL="-serial null -serial stdio"
    ;;
  *)
    fail "Unsupported architecture $ARCH."
    ;;
esac

# Find qemu binary
if command -v wslpath >/dev/null; then
  if [ -z "$NUSAOS_WIN_QEMU_INSTALL_DIR" ]; then
    NUSAOS_WIN_QEMU_INSTALL_DIR="C:\\Program Files\\qemu"
  fi
  NUSAOS_QEMU="$(wslpath "${NUSAOS_WIN_QEMU_INSTALL_DIR}")/qemu-system-$QEMU_SYSTEM.exe"
  NUSAOS_IMAGE_PATH="$(wslpath -w "$NUSAOS_IMAGE")"
else
  NUSAOS_QEMU="qemu-system-$QEMU_SYSTEM"
fi

# Deteksi display
NUSAOS_QEMU_DISPLAY=""
if "$NUSAOS_QEMU" --display help | grep -iq sdl; then
  [ "$ARCH" != "aarch64" ] && NUSAOS_QEMU_DISPLAY="-display sdl"
elif "$NUSAOS_QEMU" --display help | grep -iq cocoa; then
  NUSAOS_QEMU_DISPLAY="-display cocoa"
fi

# Gabungkan argumen kernel tambahan
# Tambahkan default init jika tidak ada
if [ -z "$NUSAOS_KERNEL_ARGS" ]; then
  NUSAOS_KERNEL_ARGS="init=/bin/dsh $@"
fi

# Menjalankan QEMU tanpa variabel string yang berantakan (menggunakan array lebih aman di bash)
"$NUSAOS_QEMU" \
  -s \
  -kernel "kernel/${KERNEL_NAME}" \
  -m "$NUSAOS_QEMU_MEM" \
  $NUSAOS_QEMU_SERIAL \
  $NUSAOS_QEMU_DEVICES \
  $NUSAOS_QEMU_DRIVE \
  $NUSAOS_QEMU_MACHINE \
  $NUSAOS_QEMU_DISPLAY \
  $NUSAOS_QEMU_ACCEL \
  -append "$NUSAOS_KERNEL_ARGS"