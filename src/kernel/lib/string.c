#include <common.h>
#include <string.h>

// compares two strings and returns 0 if they are equal
uint32_t strcmp(char* s1, char* s2) {        
    while (*s1 != 0 && *s1 == *s2)
        s1++, s2++;
    return *s1 - *s2;
}

// copies num bytes from src to dst
void* memcpy(void* dst, const void* src, size_t num) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    while (num--)
        *d++ = *s++;
    return dst;
}

// sets num bytes to val starting from ptr
void* memset(void* ptr, int val, size_t num) {
    uint8_t* p = ptr;
    while (num--)
        *p++ = (uint8_t) val;
    return ptr;
}
