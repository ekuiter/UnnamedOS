/*
 * PS/2 Keyboard - fires IRQ1
 *
 * http://wiki.osdev.org/Keyboard_Controller
 * http://www.lowlevel.eu/wiki/Keyboard_Controller
 * http://www.win.tue.nl/~aeb/linux/kbd/scancodes-10.html
 * http://www.computer-engineering.org/ps2keyboard/scancodes2.html
 * https://upload.wikimedia.org/wikipedia/commons/5/51/KB_United_States-NoAltGr.svg
 */

#include <common.h>
#include <string.h>
#include <hardware/io/keyboard.h>

// keyboard commands
#define SET_LEDS             0xED
#define SCANCODE_SET         0xF0
        
#define SCANCODE_LEN            8 // maximal length of a scancode in set 2
#define BREAK_CODE           0xF0 // sent when a key is released
#define EXTENDED_CODE        0xE0 // sent by more uncommon keys
#define KEYCODE_NUMBER        256 // keycodes are uint8_t, so we can have 256 keycodes

// The pause and print screen keys are treated specially, pause starts with 0xE1.
// Print screen consists of two extended scancodes, also its break code differs.
#define IS_PAUSE_MK(buf, len) ((len) >= 1 && (buf)[0] == 0xE1)
#define IS_PR_SC_MK(buf, len) ((len) >= 2 && (buf)[0] == 0xE0 && (buf)[1] == 0x12)
#define IS_PR_BR_MK(buf, len) ((len) >= 3 && (buf)[0] == 0xE0 && (buf)[1] == 0xF0 && (buf)[2] == 0x7C)

// we number keys by a more or less reasonable system (following the directions
#define KEYCODE(row, col) (((row) << 5) | (col)) // given on osdev.org, see above)

// If the received scancode matches the given scancode or if the passed key
// string matches the given name, return a suitable keycode.
#define MAKE_KEYCODE(name, row, col, scancode...) { \
    uint8_t compare_buf[SCANCODE_LEN] = { scancode }; \
    if ((len != 0 && scancode_compare(buf, compare_buf, len)) || \
        (len == 0 && strcmp((name), (char*) buf) == 0)) \
        return KEYCODE((row), (col)); \
}

typedef enum { // Can be used to control multiple LEDs at once. Notice that the
    SCROLL = 0b1, NUM = 0b10, CAPS = 0b100, ALL = SCROLL | NUM | CAPS
} keyboard_leds_t; // num LED can affect sent scancodes on certain laptops.

static enum { // state machine for scancode receiving
    START, BREAK_RECEIVED, EXTENDED_RECEIVED, SPECIAL_RECEIVED
} keyboard_state = START;

#if 0 // prevent Wunused-variable
static uint8_t qwerty_layout[][KEYCODE_NUMBER] = {
    { // no modifiers
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0, 0, 0, 0, '/', '*', '-', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\', 0, 0, 0, '7', '8', '9', '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\n', '4', '5', '6', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, '1', '2', '3', '\n', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, { // shift key / caps lock
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0, 0, 0, 0, '/', '*', '-', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|', 0, 0, 0, '7', '8', '9', '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '\n', '4', '5', '6', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, '1', '2', '3', '\n', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};
#endif

static uint8_t qwertz_layout[][KEYCODE_NUMBER] = {
    { // no modifiers
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '^', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0, 0, 0, 0, 0, 0, 0, '/', '*', '-', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '\t', 'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 0, '+', '#', 0, 0, 0, '7', '8', '9', '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, 0, '\n', '4', '5', '6', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, '<', 'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, 0, '1', '2', '3', '\n', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, { // shift key / caps lock
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, '!', '"', 0, '$', '%', '&', '/', '(', ')', '=', '?', 0, 0, 0, 0, 0, 0, '/', '*', '-', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I', 'O', 'P', 0, '*', '\'', 0, 0, 0, '7', '8', '9', '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, '\n', '4', '5', '6', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, '>', 'Y', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_', 0, 0, '1', '2', '3', '\n', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, ' ', 0, 0, 0, 0, 0, 0, 0, '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

static ps2_port_t port = 0; // where the keyboard is connected (almost always 1)
static uint8_t scancode_buf[SCANCODE_LEN]; // receives scancodes from keyboard
static size_t scancode_len = 0; // where to write the next received scancode byte
static keyboard_event_t event = {0}; // information on the currently pressed key
// a bitmap of all keys, indexed by keycodes (32 bytes long)
static uint8_t key_states[KEYCODE_NUMBER / 8];
// keyboard layout, currently set statically
static uint8_t (*current_layout)[KEYCODE_NUMBER] = qwertz_layout;
static keyboard_handler_t handler; // which function to call when a key event occurs

static uint8_t keyboard_scancode_set(uint8_t scancode_set) {
    if (scancode_set > 3)
        return 0; // there is no scancode set 4, ...
    ps2_write_device(port, SCANCODE_SET);
    ps2_write_device(port, scancode_set);
    if (scancode_set == 0) { // read scancode set rather than write
        ps2_error_t err;
        return ps2_read_device(port, &err);
    }
    ps2_config_t config = ps2_read_config();
    config.bits.port1_transl = 0;
    ps2_write_config(config);
    return scancode_set;
}

static uint8_t keyboard_leds(keyboard_leds_t leds, int8_t value) {
    static uint8_t led_state = 0;
    if (value < 0)
        led_state ^= leds; // toggle LEDs
    else if (value > 0)
        led_state |= leds; // enable LEDs
    else
        led_state &= ~leds; // disable LEDs
    ps2_write_device(port, SET_LEDS);
    ps2_write_device(port, led_state);
    return led_state;
}

void keyboard_init(ps2_port_t _port) {
    port = _port;
    keyboard_leds(ALL, 0);
    keyboard_scancode_set(2); // scancode set 2 is widely supported
}

static uint8_t scancode_compare(uint8_t* buf, uint8_t* compare_buf, size_t len) {
    for (int i = 0; i < len; i++)
        if (buf[i] != compare_buf[i])
            return 0;
    return 1;
}

static uint8_t keyboard_get_keycode_from_scancode(uint8_t* buf, size_t len) {
    MAKE_KEYCODE("POWER",     0,  0, 0xE0, 0x37); MAKE_KEYCODE("ESC",       1,  0, 0x76);
    MAKE_KEYCODE("SLEEP",     0,  1, 0xE0, 0x3F); MAKE_KEYCODE("F1",        1,  1, 0x05);
    MAKE_KEYCODE("WAKE",      0,  2, 0xE0, 0x5E); MAKE_KEYCODE("F2",        1,  2, 0x06);
    MAKE_KEYCODE("NEXT TRCK", 0,  3, 0xE0, 0x4D); MAKE_KEYCODE("F3",        1,  3, 0x04);
    MAKE_KEYCODE("PREV TRCK", 0,  4, 0xE0, 0x15); MAKE_KEYCODE("F4",        1,  4, 0x0C);
    MAKE_KEYCODE("STOP",      0,  5, 0xE0, 0x3B); MAKE_KEYCODE("F5",        1,  5, 0x03);
    MAKE_KEYCODE("PLAY",      0,  6, 0xE0, 0x34); MAKE_KEYCODE("F6",        1,  6, 0x0B);
    MAKE_KEYCODE("MUTE",      0,  7, 0xE0, 0x23); MAKE_KEYCODE("F7",        1,  7, 0x83);
    MAKE_KEYCODE("VOL UP",    0,  8, 0xE0, 0x32); MAKE_KEYCODE("F8",        1,  8, 0x0A);
    MAKE_KEYCODE("VOL DN",    0,  9, 0xE0, 0x21); MAKE_KEYCODE("F9",        1,  9, 0x01);
    MAKE_KEYCODE("MEDIA SEL", 0, 10, 0xE0, 0x50); MAKE_KEYCODE("F10",       1, 10, 0x09);
    MAKE_KEYCODE("MAIL",      0, 11, 0xE0, 0x48); MAKE_KEYCODE("F11",       1, 11, 0x78);
    MAKE_KEYCODE("CALC",      0, 12, 0xE0, 0x2B); MAKE_KEYCODE("F12",       1, 12, 0x07);
    MAKE_KEYCODE("COMPUTER",  0, 13, 0xE0, 0x40); MAKE_KEYCODE("PRNT SCRN", 1, 13, 0xE0, 0x12, 0xE0, 0x7C);
    MAKE_KEYCODE("WWW SRCH",  0, 14, 0xE0, 0x10); MAKE_KEYCODE("SCROLL",    1, 14, 0x7E);
    MAKE_KEYCODE("WWW HOME",  0, 15, 0xE0, 0x3A); MAKE_KEYCODE("PAUSE",     1, 15, 0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77);
    MAKE_KEYCODE("WWW BACK",  0, 16, 0xE0, 0x38); 
    MAKE_KEYCODE("WWW FORW",  0, 17, 0xE0, 0x30); 
    MAKE_KEYCODE("WWW STOP",  0, 18, 0xE0, 0x28); 
    MAKE_KEYCODE("WWW REFR",  0, 19, 0xE0, 0x20); 
    MAKE_KEYCODE("WWW FAVR",  0, 20, 0xE0, 0x18); 
   
    MAKE_KEYCODE("`",         2,  0, 0x0E);       MAKE_KEYCODE("TAB",       3,  0, 0x0D);
    MAKE_KEYCODE("1",         2,  1, 0x16);       MAKE_KEYCODE("Q",         3,  1, 0x15);
    MAKE_KEYCODE("2",         2,  2, 0x1E);       MAKE_KEYCODE("W",         3,  2, 0x1D);
    MAKE_KEYCODE("3",         2,  3, 0x26);       MAKE_KEYCODE("E",         3,  3, 0x24);
    MAKE_KEYCODE("4",         2,  4, 0x25);       MAKE_KEYCODE("R",         3,  4, 0x2D);
    MAKE_KEYCODE("5",         2,  5, 0x2E);       MAKE_KEYCODE("T",         3,  5, 0x2C);
    MAKE_KEYCODE("6",         2,  6, 0x36);       MAKE_KEYCODE("Y",         3,  6, 0x35);
    MAKE_KEYCODE("7",         2,  7, 0x3D);       MAKE_KEYCODE("U",         3,  7, 0x3C);
    MAKE_KEYCODE("8",         2,  8, 0x3E);       MAKE_KEYCODE("I",         3,  8, 0x43);
    MAKE_KEYCODE("9",         2,  9, 0x46);       MAKE_KEYCODE("O",         3,  9, 0x44);
    MAKE_KEYCODE("0",         2, 10, 0x45);       MAKE_KEYCODE("P",         3, 10, 0x4D);
    MAKE_KEYCODE("-",         2, 11, 0x4E);       MAKE_KEYCODE("[",         3, 11, 0x54);
    MAKE_KEYCODE("=",         2, 12, 0x55);       MAKE_KEYCODE("]",         3, 12, 0x5B);
    MAKE_KEYCODE("BKSP",      2, 13, 0x66);       MAKE_KEYCODE("\\",        3, 13, 0x5D);
    MAKE_KEYCODE("INSERT",    2, 14, 0xE0, 0x70); MAKE_KEYCODE("DELETE",    3, 14, 0xE0, 0x71);
    MAKE_KEYCODE("HOME",      2, 15, 0xE0, 0x6C); MAKE_KEYCODE("END",       3, 15, 0xE0, 0x69);
    MAKE_KEYCODE("PG UP",     2, 16, 0xE0, 0x7D); MAKE_KEYCODE("PG DN",     3, 16, 0xE0, 0x7A);
    MAKE_KEYCODE("NUM",       2, 17, 0x77);       MAKE_KEYCODE("KP 7",      3, 17, 0x6C);
    MAKE_KEYCODE("KP /",      2, 18, 0xE0, 0x4A); MAKE_KEYCODE("KP 8",      3, 18, 0x75);
    MAKE_KEYCODE("KP *",      2, 19, 0x7C);       MAKE_KEYCODE("KP 9",      3, 19, 0x7D);
    MAKE_KEYCODE("KP -",      2, 20, 0x7B);       MAKE_KEYCODE("KP +",      3, 20, 0x79);

    MAKE_KEYCODE("CAPS",      4,  0, 0x58);       MAKE_KEYCODE("L SHFT",    5,  0, 0x12);
    MAKE_KEYCODE("A",         4,  1, 0x1C);       MAKE_KEYCODE("<",         5,  1, 0x61);
    MAKE_KEYCODE("S",         4,  2, 0x1B);       MAKE_KEYCODE("Z",         5,  2, 0x1A);
    MAKE_KEYCODE("D",         4,  3, 0x23);       MAKE_KEYCODE("X",         5,  3, 0x22);
    MAKE_KEYCODE("F",         4,  4, 0x2B);       MAKE_KEYCODE("C",         5,  4, 0x21);
    MAKE_KEYCODE("G",         4,  5, 0x34);       MAKE_KEYCODE("V",         5,  5, 0x2A);
    MAKE_KEYCODE("H",         4,  6, 0x33);       MAKE_KEYCODE("B",         5,  6, 0x32);
    MAKE_KEYCODE("J",         4,  7, 0x3B);       MAKE_KEYCODE("N",         5,  7, 0x31);
    MAKE_KEYCODE("K",         4,  8, 0x42);       MAKE_KEYCODE("M",         5,  8, 0x3A);
    MAKE_KEYCODE("L",         4,  9, 0x4B);       MAKE_KEYCODE(",",         5,  9, 0x41);
    MAKE_KEYCODE(";",         4, 10, 0x4C);       MAKE_KEYCODE(".",         5, 10, 0x49);
    MAKE_KEYCODE("'",         4, 11, 0x52);       MAKE_KEYCODE("/",         5, 11, 0x4A);
    MAKE_KEYCODE("ENTER",     4, 12, 0x5A);       MAKE_KEYCODE("R SHFT",    5, 12, 0x59);
    MAKE_KEYCODE("KP 4",      4, 13, 0x6B);       MAKE_KEYCODE("U ARROW",   5, 13, 0xE0, 0x75);
    MAKE_KEYCODE("KP 5",      4, 14, 0x73);       MAKE_KEYCODE("KP 1",      5, 14, 0x69);
    MAKE_KEYCODE("KP 6",      4, 15, 0x74);       MAKE_KEYCODE("KP 2",      5, 15, 0x72);
                                                  MAKE_KEYCODE("KP 3",      5, 16, 0x7A);
                                                  MAKE_KEYCODE("KP EN",     5, 17, 0xE0, 0x5A);
                                                  
    MAKE_KEYCODE("L CTRL",    6,  0, 0x14);       MAKE_KEYCODE("R CTRL",    6,  7, 0xE0, 0x14);
    MAKE_KEYCODE("L GUI",     6,  1, 0xE0, 0x1F); MAKE_KEYCODE("L ARROW",   6,  8, 0xE0, 0x6B);
    MAKE_KEYCODE("L ALT",     6,  2, 0x11);       MAKE_KEYCODE("D ARROW",   6,  9, 0xE0, 0x72);
    MAKE_KEYCODE("SPACE",     6,  3, 0x29);       MAKE_KEYCODE("R ARROW",   6, 10, 0xE0, 0x74);
    MAKE_KEYCODE("R ALT",     6,  4, 0xE0, 0x11); MAKE_KEYCODE("KP 0",      6, 11, 0x70);
    MAKE_KEYCODE("R GUI",     6,  5, 0xE0, 0x27); MAKE_KEYCODE("KP .",      6, 12, 0x71);
    MAKE_KEYCODE("APPS",      6,  6, 0xE0, 0x2F);
    
    return KEY_UNKNOWN;
}

uint8_t keyboard_get_keycode(char* name) {
    return keyboard_get_keycode_from_scancode((uint8_t*) name, 0);
}

uint8_t keyboard_get_key_pressed(uint8_t keycode) {
    return (key_states[keycode / 8] >> (keycode % 8)) & 1; // bitmap getter
}

static void keyboard_set_key_pressed(uint8_t keycode, uint8_t value) {
    if (value) // bitmap setter
        key_states[keycode / 8] |= 1 << (keycode % 8);
    else
        key_states[keycode / 8] &= ~(1 << (keycode % 8));
}

static uint8_t keyboard_get_ascii_from_keycode(uint8_t (*layout)[KEYCODE_NUMBER],
        uint8_t keycode, keyboard_flags_t flags) {
    if (flags.shift || flags.caps_lock)
        return layout[1][keycode];
    return layout[0][keycode];
}

// called when a scancode has been received (makecode = key pressed)
static void keyboard_process_scancode(uint8_t is_makecode) {    
    event.keycode = keyboard_get_keycode_from_scancode(scancode_buf, scancode_len);
    event.pressed = is_makecode;
    // the pause key never sends a breakcode and we don't need lock breakcodes
    // (in bochs they are not even sent, maybe because of the LED toggling)
    if (event.keycode != KEY("PAUSE") && event.keycode != KEY("SCROLL") &&
        event.keycode != KEY("NUM")   && event.keycode != KEY("CAPS"))
        keyboard_set_key_pressed(event.keycode, event.pressed);
    event.ascii = keyboard_get_ascii_from_keycode(current_layout, event.keycode, event.flags);
    
    // handle modifier keys
    event.flags.shift = keyboard_get_key_pressed(KEY("L SHFT")) ||
                        keyboard_get_key_pressed(KEY("R SHFT"));
    event.flags.ctrl  = keyboard_get_key_pressed(KEY("L CTRL")) ||
                        keyboard_get_key_pressed(KEY("R CTRL"));
    event.flags.gui   = keyboard_get_key_pressed(KEY("L GUI"))  ||
                        keyboard_get_key_pressed(KEY("R GUI"));
    event.flags.alt   = keyboard_get_key_pressed(KEY("L ALT"))  ||
                        keyboard_get_key_pressed(KEY("R ALT"));
    if (event.pressed) {
        if (event.keycode == KEY("SCROLL"))
            keyboard_leds(SCROLL, event.flags.scroll_lock = !event.flags.scroll_lock);
        if (event.keycode == KEY("NUM"))
            keyboard_leds(NUM,    event.flags.num_lock    = !event.flags.num_lock);
        if (event.keycode == KEY("CAPS"))
            keyboard_leds(CAPS,   event.flags.caps_lock   = !event.flags.caps_lock);
    }
    
    if (handler) handler(event); // call our event handler if there is one
    
    keyboard_state = START; // reset the state machine so
    scancode_len = 0; // we can proceed with a fresh scancode
}

static uint8_t keyboard_process_special_scancode(uint8_t condition, uint8_t len,
        uint8_t is_breakcode) {
    if (condition) { // check whether a special scancode is being sent
        keyboard_state = SPECIAL_RECEIVED;
        if (scancode_len == len) // whether we fully received the scancode
            keyboard_process_scancode(is_breakcode);
        return 1;
    }
    return 0;
}

keyboard_event_t keyboard_get_event() {
    return event;
}

void keyboard_register_handler(keyboard_handler_t _handler) {
    handler = _handler;
}

// called by the PS/2 driver with a scancode byte
void keyboard_handle_data(uint8_t data) {   
    if (keyboard_state == BREAK_RECEIVED) // ignore the BREAK_CODE byte
        scancode_len--; // so we can easily convert breakcodes to keycodes
    scancode_buf[scancode_len++] = data; // save scancode byte
    
    // handle special keys (pause and print screen)
    if (keyboard_process_special_scancode(IS_PAUSE_MK(scancode_buf, scancode_len), 8, 1)) return;
    if (keyboard_process_special_scancode(IS_PR_SC_MK(scancode_buf, scancode_len), 4, 1)) return;
    if (keyboard_process_special_scancode(IS_PR_BR_MK(scancode_buf, scancode_len), 6, 0)) return;
    
    switch (keyboard_state) {
        case START: // we just received the first scancode byte
            if      (data == BREAK_CODE)    keyboard_state = BREAK_RECEIVED;
            else if (data == EXTENDED_CODE) keyboard_state = EXTENDED_RECEIVED;
            else keyboard_process_scancode(1); // received a one-byte scancode
            break;
        case BREAK_RECEIVED:
            keyboard_process_scancode(0); // received a breakcode
            break;
        case EXTENDED_RECEIVED: // received an extended ...
            if (data == BREAK_CODE)
                keyboard_state = BREAK_RECEIVED; // ... breakcode
            else keyboard_process_scancode(1); // ... scancode
            break;
        case SPECIAL_RECEIVED: // this will never be executed, prevent Wswitch
            break;
    }
}
