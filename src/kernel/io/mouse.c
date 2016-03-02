/*
 * PS/2 Mouse - fires IRQ12
 *
 * http://wiki.osdev.org/PS/2_Mouse
 * http://wiki.osdev.org/Mouse_Input
 * http://lowlevel.eu/wiki/PS/2-Maus
 */

#include <common.h>
#include <io/mouse.h>

void mouse_init(ps2_port_t port) {
}

void mouse_handle_data(uint8_t data) {
    print("%5a%02x%a ", data);
}