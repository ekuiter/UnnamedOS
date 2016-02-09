/*
 * I/O ports
 *
 * http://wiki.osdev.org/I/O_ports
 * http://wiki.osdev.org/Inline_Assembly
 * http://wiki.osdev.org/Inline_Assembly/Examples
 */

#ifndef IO_PORT_H
#define IO_PORT_H

#include <stdint.h>

inline void outb(uint16_t port, uint8_t val) {
    // syntax: outb <val, here %0>, <port, here %1>
    // "a" (val) store val to EAX for passing to outb
    // "Nd" (port) tries to pass port as a constant value ("N"), if it fails, EDX is used as a fallback ("d")
    asm volatile("outb %0, %1" : : "a" (val), "Nd" (port));
}

inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a" (val), "Nd" (port));
}

inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a" (val), "Nd" (port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t result;
    // syntax: inb <port, here %1>, <register, here %0>
    // "=a" (result) makes sure inb's result is saved to EAX ("a") and then to the result variable
    // "Nd" (port) see above
    asm volatile("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

inline uint32_t inl(uint16_t port) {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

inline void io_wait() {
    // We do a "dummy access" to an IO port to let slow devices (like some PICs) catch up.
    // 0x80 is responsible for POST debug messages on startup, we can treat it as having no effect.
    asm volatile("outb %al, $0x80");
}

#endif
