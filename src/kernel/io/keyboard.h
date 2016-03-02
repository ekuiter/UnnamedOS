#ifndef IO_KEYBOARD_H
#define IO_KEYBOARD_H

#include <io/ps2.h>


#define KEY(name)   keyboard_get_keycode(name)
#define KEY_UNKNOWN 0xFF // keycode for unknown keys

typedef struct {
    uint8_t shift : 1, ctrl : 1, gui : 1, alt : 1,
            scroll_lock : 1, num_lock : 1, caps_lock : 1;
} keyboard_flags_t; // which modifier keys are pressed / locked

typedef struct {
    uint8_t keycode; // which key was last pressed / released
    uint8_t pressed; // whether the key was pressed (1) or released (0)
    uint8_t ascii; // contains a corresponding ASCII character (if any), otherwise 0
    keyboard_flags_t flags;
} keyboard_event_t;

typedef void (*keyboard_handler_t)(keyboard_event_t event);

void keyboard_init(ps2_port_t port);
uint8_t keyboard_get_keycode(char* name);
void keyboard_register_handler(keyboard_handler_t _handler);
void keyboard_handle_data(uint8_t data);

#endif
