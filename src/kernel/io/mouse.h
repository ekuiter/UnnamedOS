#ifndef IO_MOUSE_H
#define IO_MOUSE_H

#include <io/ps2.h>

void mouse_init(ps2_port_t port);
void mouse_handler(uint8_t data);

#endif
