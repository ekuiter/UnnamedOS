#ifndef IO_KEYBOARD_H
#define IO_KEYBOARD_H

#include <io/ps2.h>

void keyboard_init(ps2_port_t port);
void keyboard_handler(uint8_t data);

#endif
