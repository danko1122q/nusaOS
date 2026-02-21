#include <sys/reboot.h>
#include <sys/syscall.h>

int reboot(int cmd) {
    return syscall2(SYS_REBOOT, cmd);
}