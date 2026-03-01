#!/bin/bash
set -e

SCRIPTDIR=$(dirname "$BASH_SOURCE")
source "$SCRIPTDIR/nusaos.sh"

detach_and_unmount() {
  msg "Cleaning up..."
  sync
  if [ ! -z "$MOUNTED" ]; then
    umount -l mnt/ || true
    if [ -d mnt ]; then rmdir mnt || true; fi
  fi

  if [ ! -z "$dev" ]; then
    if [ "$SYSTEM" = "Darwin" ]; then
      hdiutil detach "$dev" || true
    else
      sync
      losetup -d "${dev}" || true
    fi
  fi
}

ON_FAIL=detach_and_unmount

if [ "$(id -u)" != 0 ]; then
  exec sudo -E -- "$0" "$@" || exit 1
else
  : "${SUDO_UID:=0}" "${SUDO_GID:=0}"
fi

SYSTEM="$(uname -s)"
DU_COMMAND="du"
IMAGE_NAME="nusaOS.img"
IMAGE_EXTRASIZE="50000"

if [ "$SYSTEM" = "Darwin" ]; then
    export PATH="/usr/local/opt/e2fsprogs/bin:/usr/local/opt/e2fsprogs/sbin:/opt/homebrew/opt/e2fsprogs/bin:/opt/homebrew/opt/e2fsprogs/sbin:$PATH"
    DU_COMMAND="gdu"
fi

if [ $# -eq 0 ]; then
  USER_SIZE=0
  if [ -d "${SOURCE_DIR}/user" ]; then
    USER_SIZE=$("$DU_COMMAND" -sk "${SOURCE_DIR}/user" | cut -f1)
  fi
  IMAGE_SIZE=$(($("$DU_COMMAND" -sk root | cut -f1) + IMAGE_EXTRASIZE + USER_SIZE))
  IMAGE_SIZE=$(expr '(' '(' "$IMAGE_SIZE" + 1023 ')' / 1024 ')' '*' 1024)

  if [ -f "$IMAGE_NAME" ]; then
    USE_EXISTING=1
    msg "Using existing image..."
  else
    msg "Creating image ($IMAGE_SIZE K)..."
    qemu-img create -q -f raw "$IMAGE_NAME" "$IMAGE_SIZE"k || exit 1
    chown "$SUDO_UID":"$SUDO_GID" "$IMAGE_NAME"
  fi

  if [ "$SYSTEM" = "Darwin" ]; then
    msg "Attaching image..."
    dev_arr=($(hdiutil attach -nomount "$IMAGE_NAME"))
    dev=${dev_arr[0]}
    part="s1"
    if [ -z "$dev" ]; then exit 1; fi
  else
    msg "Making loopback device..."
    dev=$(losetup --find --partscan --show "$IMAGE_NAME")
    part="p1"
    if [ -z "$dev" ]; then exit 1; fi
  fi
else
  dev="$1"
  part="1"
fi

if [ ! "$USE_EXISTING" ]; then
  if [ "$SYSTEM" = "Darwin" ]; then
    diskutil partitionDisk $(basename "$dev") 1 MBR fuse-ext2 nusaOS 100% || exit 1
  else
    parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || exit 1
  fi
  yes | mke2fs -q -I 128 -b 1024 "${dev}${part}" || exit 1
fi

mkdir -p mnt/
if [ "$SYSTEM" = "Darwin" ]; then
  fuse-ext2 "${dev}${part}" mnt -o rw+,allow_other,uid="$SUDO_UID",gid="$SUDO_GID" || exit 1
  MOUNTED="1"
else
  mount "${dev}${part}" mnt/ || exit 1
  MOUNTED="1"
fi

if [ ! "$USE_EXISTING" ]; then
  if [ "$SYSTEM" != "Darwin" ]; then
    GRUB_COMMAND="grub-install"
    if ! type "$GRUB_COMMAND" &> /dev/null; then
        GRUB_COMMAND="grub2-install"
    fi
    "$GRUB_COMMAND" --boot-directory=mnt/boot \
                    --target=i386-pc \
                    --locales="" \
                    --no-floppy \
                    --modules="ext2 part_msdos" \
                    "${dev}" --force || exit 1
    
    if [[ -d "mnt/boot/grub2" ]]; then
        cp "${SOURCE_DIR}/scripts/grub.cfg" mnt/boot/grub2/grub.cfg || exit 1
    elif [[ -d "mnt/boot/grub" ]]; then
        cp "${SOURCE_DIR}/scripts/grub.cfg" mnt/boot/grub/grub.cfg || exit 1
    fi
  fi
fi

bash "${SOURCE_DIR}/scripts/base-system.sh" mnt/ || exit 1

sync

if [ $# -eq 0 ]; then
  detach_and_unmount
  success "Done! Saved to $IMAGE_NAME."
else
  success "Done!"
fi