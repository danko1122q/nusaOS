#pragma once

#include <kernel/syscall/syscall_numbers.h>

#define RB_POWER_OFF 0
#define RB_AUTOBOOT  1

#ifdef __cplusplus
extern "C" {
#endif

int reboot(int cmd);

#ifdef __cplusplus
}
#endif