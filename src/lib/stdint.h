#ifndef STDINT_H
#define STDINT_H

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef signed long long int64_t;
typedef signed int int32_t;
typedef signed short int16_t;
typedef signed char int8_t;

typedef uint32_t uintptr_t;
typedef uint32_t size_t;

typedef union word {
    unsigned short w;
    unsigned char b[2];
} word_t;

typedef union dword {
    unsigned int dw;
    word_t b[2];
} dword_t;

#endif
