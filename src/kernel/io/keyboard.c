/*
 * PS/2 Keyboard - fires IRQ1
 *
 * http://wiki.osdev.org/Keyboard_Controller
 * http://www.lowlevel.eu/wiki/Keyboard_Controller
 * http://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
 */

#include <common.h>
#include <io/keyboard.h>

#define SET_LEDS     0xED
#define SCANCODE_SET 0xF0
#define LED_BYTE(scroll, number, caps) (scroll * 0b1 + number * 0b10 + caps * 0b100)

static ps2_port_t port = 0;

static void keyboard_set_scancode_set(uint8_t scancode_set) {
    ps2_write_device(port, SCANCODE_SET);
    ps2_write_device(port, scancode_set);
    ps2_config_t config = ps2_read_config();
    config.bits.port1_transl = 0;
    ps2_write_config(config);
}

static uint8_t keyboard_get_scancode_set() {
    ps2_write_device(port, SCANCODE_SET);
    ps2_write_device(port, 0);
    ps2_error_t err;
    return ps2_read_device(port, &err);
}

void keyboard_init(ps2_port_t _port) {
    port = _port;
    ps2_write_device(port, SET_LEDS);
    ps2_write_device(port, LED_BYTE(0, 0, 1));
    keyboard_set_scancode_set(2);
}

void keyboard_handler(uint8_t data) {
    print("%3a%02x%a ", data);
}