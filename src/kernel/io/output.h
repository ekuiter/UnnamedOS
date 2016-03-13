#ifndef IO_OUTPUT_H
#define IO_OUTPUT_H

#include <stdint.h>

#define IO_DEFAULT 0x07
#define IO_GREEN   0x02
#define IO_RED     0x04

#define IO_COLS 80
#define IO_ROWS 25
#define IO_COORD(x, y) ((y) * IO_COLS + (x))

typedef uint16_t (*putchar_func_t)(uint8_t c);

void io_attr(uint8_t new_attr);
void io_cursor(size_t new_cursor);
uint16_t io_putchar(uint8_t c);
uint16_t io_putstr(char* s, putchar_func_t putchar_func);
uint16_t io_putint(uint32_t n, uint8_t radix, int8_t pad, uint8_t pad_char,
        putchar_func_t putchar_func);
void io_clear(putchar_func_t putchar_func);
void print(char* fmt, ...);
void println(char* fmt, ...);
void fprint(putchar_func_t putchar_func, char* fmt, ...);
void fprintln(putchar_func_t putchar_func, char* fmt, ...);

#endif
