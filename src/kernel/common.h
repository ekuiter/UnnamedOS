#ifndef COMMON_H
#define COMMON_H

#include <io/output.h>
#include <io/port.h>
#include <stdint.h>

#define panic(s) _panic(s, __FILE__, __LINE__)
#define assert(exp) ((exp) ? (void) 0 : panic("Assert failed"))

extern void halt();
void _panic(char* msg, char* file, uint32_t line);

#endif
