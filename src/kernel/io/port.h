#ifndef IO_PORT_H
#define IO_PORT_H

#include <stdint.h>

/*
 * I/O ports
 *
 * http://wiki.osdev.org/I/O_ports
 * http://wiki.osdev.org/Inline_Assembly
 * http://wiki.osdev.org/Inline_Assembly/Examples
 */

inline void outb(uint16_t port, uint8_t val) {
    // syntax: outb <val, here %0>, <port, here %1>
    // "a" (val) store val to EAX for passing to outb
    // "Nd" (port) tries to pass port as a constant value ("N"), if it fails, EDX is used as a fallback ("d")
    asm volatile("outb %0, %1" : : "a" (val), "Nd" (port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t result;
    // syntax: inb <port, here %1>, <register, here %0>
    // "=a" (result) makes sure inb's result is saved to EAX ("a") and then to the result variable
    // "Nd" (port) see above
    asm volatile("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

#endif
