#ifndef IO_MOUSE_H
#define IO_MOUSE_H

#include <io/ps2.h>

typedef struct {
    uint16_t x, y; // intern mouse coordinates
    uint8_t screen_x, screen_y; // mapped to screen coordinates
    uint8_t left : 1, right : 1, middle : 1; // whether mouse keys are pressed
} mouse_event_t;

typedef void (*mouse_handler_t)(mouse_event_t event);

void mouse_init(ps2_port_t _port);
void mouse_register_handler(mouse_handler_t _handler);
void mouse_handle_data(uint8_t data);

#endif
