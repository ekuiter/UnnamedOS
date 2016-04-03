#ifndef IO_H
#define IO_H

#include <stdint.h>

#define IO_DEFAULT 0x07
#define IO_GREEN   0x02
#define IO_RED     0x04

typedef uint16_t (*putchar_func_t)(uint8_t c);
typedef uint8_t (*attr_func_t)(uint8_t c);

void io_set_stubs(putchar_func_t _io_putchar_stub, attr_func_t _io_attr_stub);
uint16_t io_putstr(char* s, putchar_func_t putchar_func);
uint16_t io_putint(uint32_t n, uint8_t radix, int8_t pad, uint8_t pad_char,
        putchar_func_t putchar_func);
void io_clear(putchar_func_t putchar_func);
uint16_t vprint(char* fmt, uint32_t* arg, putchar_func_t putchar_func);
uint16_t print(char* fmt, ...);
uint16_t println(char* fmt, ...);
uint16_t fprint(putchar_func_t putchar_func, char* fmt, ...);
uint16_t fprintln(putchar_func_t putchar_func, char* fmt, ...);

#endif
