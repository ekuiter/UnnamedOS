#ifndef COMMON_H
#define COMMON_H

#include <hardware/io/output.h>
#include <hardware/io/port.h>
#include <stdint.h>

#define panic(...) { \
    print("%4a"); \
    print(__VA_ARGS__); \
    print(" at %s:%d", __FILE__, __LINE__); \
    halt(); \
}

#define bochs_break() { \
    outw(0x8A00, 0x8A00); \
    outw(0x8A00, 0x8AE0); \
}

extern void halt();
extern uint16_t bochs_log(uint8_t c);

#endif
