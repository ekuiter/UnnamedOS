#ifndef COMMON_H
#define COMMON_H

#include <io/output.h>
#include <io/port.h>
#include <stdint.h>

#define panic(...) { \
    print("%4a"); \
    print(__VA_ARGS__); \
    print(" at %s:%d", __FILE__, __LINE__); \
    halt(); \
}
#define assert(exp) ((exp) ? (void) 0 : panic("Assert failed"))

extern void halt();

#endif
