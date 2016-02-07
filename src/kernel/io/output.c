#include <io/output.h>

/*
 * Output to Screen
 *
 * http://wiki.osdev.org/Printing_to_Screen
 * http://wiki.osdev.org/Text_UI
 */

#define IO_COLS 80
#define IO_ROWS 25
#define IO_CHARS (IO_COLS * IO_ROWS)
#define IO_MEM ((uint8_t*) 0xb8000)

static uint8_t* video = IO_MEM;
static uint8_t attr = 0x07;
static size_t cursor = 0;

static void io_setchar(uint8_t c, size_t pos) {
    video[(pos % IO_CHARS) * 2] = c;
}

static void io_setattr(uint8_t attr, size_t pos) {
    video[(pos % IO_CHARS) * 2 + 1] = attr;
}

void io_attr(uint8_t new_attr) {
    attr = new_attr;
}

void io_cursor(size_t new_cursor) {
    cursor = new_cursor;
}

uint16_t io_putchar(uint8_t c) {
    if (c == '\n')
        cursor = cursor + IO_COLS - (cursor % IO_COLS);
    else {
        io_setchar(c, cursor);
        io_setattr(attr, cursor);
        cursor = (cursor + 1) % IO_CHARS;
    }
    return 1;
}

uint16_t io_putstr(char* s) {
    uint16_t count = 0;
    while (*s != '\0')
        count += io_putchar(*s++);
    return count;
}

uint16_t io_putint(uint32_t n, uint8_t radix, int8_t pad, uint8_t pad_char) {
    if (radix < 2 || radix > 36)
        return io_putstr("radix invalid");
    uint8_t buf_len = sizeof(n) * 8 + 1; // max. string length is "bits of n + \0 character"
    char buf[buf_len], digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* cur = &buf[buf_len - 1]; // cur points to the last element
    *cur-- = '\0';
    for (; n != 0; n /= radix) { // cycle digits backwards
        *cur-- = digits[n % radix]; // choose a digit to display
        pad--;
    }
    uint16_t count = 0;
    for (; pad > 0; pad--)
        count += io_putchar(pad_char); // put padding characters
    return count + io_putstr(cur + 1);
}

void io_clear() {
    io_cursor(0);
    for (int i = 0; i < IO_CHARS; i++)
        io_putchar('\0');
}
