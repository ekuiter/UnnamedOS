#define SHOULD_DEFINE_SYSCALLS

#define SYSCALL_0(id, name, return_type) \
    return_type name() { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id)); \
        return (return_type) retval; \
    }

#define SYSCALL_1(id, name, return_type, type1) \
    return_type name(type1 ebx) { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id), "b" (ebx)); \
        return (return_type) retval; \
    }

#define SYSCALL_2(id, name, return_type, type1, type2) \
    return_type name(type1 ebx, type2 ecx) { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id), "b" (ebx), \
            "c" (ecx)); \
        return (return_type) retval; \
    }

#define SYSCALL_3(id, name, return_type, type1, type2, type3) \
    return_type name(type1 ebx, type2 ecx, type3 edx) { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id), "b" (ebx), \
            "c" (ecx), "d" (edx)); \
        return (return_type) retval; \
    }

#define SYSCALL_4(id, name, return_type, type1, type2, type3, type4) \
    return_type name(type1 ebx, type2 ecx, type3 edx, type4 esi) { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id), "b" (ebx), \
            "c" (ecx), "d" (edx), "s" (esi)); \
        return (return_type) retval; \
    }

#define SYSCALL_5(id, name, return_type, type1, type2, type3, type4, type5) \
    return_type name(type1 ebx, type2 ecx, type3 edx, type4 esi, type5 edi) { \
        return_type retval; \
        asm volatile("int $0x30" : "=a" (retval) : "a" (id), "b" (ebx), \
            "c" (ecx), "d" (edx), "s" (esi), "d" (edi)); \
        return (return_type) retval; \
    }

#include <syscall.h>
