#ifndef IO_OUTPUT_H
#define IO_OUTPUT_H

#include <stdint.h>

#define IO_DEFAULT 0x07
#define IO_GREEN   0x02
#define IO_RED     0x04

#define IO_COLS 80
#define IO_ROWS 25
#define IO_COORD(x, y) ((y) * IO_COLS + (x))

void io_attr(uint8_t new_attr);
void io_cursor(size_t new_cursor);
uint16_t io_putchar(uint8_t c);
uint16_t io_putstr(char* s);
uint16_t io_putint(uint32_t n, uint8_t radix, int8_t pad, uint8_t pad_char);
void io_clear();
void print(char* fmt, ...);
void println(char* fmt, ...);

#endif
