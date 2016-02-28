#ifndef IO_PS2_H
#define IO_PS2_H

#include <intr/isr.h>

// PS/2 device ports
typedef enum {
    PS2_ANY_PORT, // don't care about the port used
    PS2_PORT_1,   // port 1 is usually the keyboard
    PS2_PORT_2    // port 2, if supported, is usually the mouse
} ps2_port_t;

typedef enum { PS2_TIMEOUT_ERR = 1, PS2_INVALID_PORT_ERR } ps2_error_t;

typedef union {
    struct {
        uint8_t port1_intr   : 1; // whether port 1 fires IRQ1
        uint8_t port2_intr   : 1; // whether port 2 fires IRQ12
        uint8_t system       : 1; // set if system passed POST tests
        uint8_t              : 1;
        uint8_t port1_clock  : 1; // whether port 1 clock is disabled
        uint8_t port2_clock  : 1; // whether port 2 clock is disabled
        uint8_t port1_transl : 1; // whether port 1 translates scancode sets
        uint8_t              : 1;
    } __attribute__((packed)) bits;
    uint8_t byte;
} ps2_config_t;

ps2_config_t ps2_read_config();
void ps2_write_config(ps2_config_t config);
void ps2_flush();
uint8_t ps2_write_device(ps2_port_t port, uint8_t command);
uint8_t ps2_read_device(ps2_port_t port, ps2_error_t* err);
void ps2_init();
void ps2_reboot();

#endif
