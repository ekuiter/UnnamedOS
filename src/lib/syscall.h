#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#ifndef SHOULD_DEFINE_SYSCALLS
#define SYSCALL_0(id, name, return_type) \
    return_type name()
#define SYSCALL_1(id, name, return_type, type1) \
    return_type name(type1 ebx)
#define SYSCALL_2(id, name, return_type, type1, type2) \
    return_type name(type1 ebx, type2 ecx)
#define SYSCALL_3(id, name, return_type, type1, type2, type3) \
    return_type name(type1 ebx, type2 ecx, type3 edx)
#define SYSCALL_4(id, name, return_type, type1, type2, type3, type4) \
    return_type name(type1 ebx, type2 ecx, type3 edx, type4 esi)
#define SYSCALL_5(id, name, return_type, type1, type2, type3, type4, type5) \
    return_type name(type1 ebx, type2 ecx, type3 edx, type4 esi, type5 edi)
#endif

#define SYSCALL_NUMBER 32

enum {
    SYSCALL_EXIT, SYSCALL_GETPID, SYSCALL_IO_PUTCHAR, SYSCALL_IO_ATTR
} syscall_ids;

SYSCALL_1(SYSCALL_EXIT,       sys_exit,       uint32_t, uint32_t);
SYSCALL_0(SYSCALL_GETPID,     sys_getpid,     uint32_t);
SYSCALL_1(SYSCALL_IO_PUTCHAR, sys_io_putchar, uint16_t, uint8_t);
SYSCALL_1(SYSCALL_IO_ATTR,    sys_io_attr,    uint8_t,  uint8_t);

#undef SHOULD_DEFINE_SYSCALLS
#undef SYSCALL_0
#undef SYSCALL_1
#undef SYSCALL_2
#undef SYSCALL_3
#undef SYSCALL_4
#undef SYSCALL_5

#endif
