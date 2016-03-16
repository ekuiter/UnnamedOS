#include <common.h>
#include <string.h>

// compares two strings and returns 0 if they are equal
uint32_t strcmp(char* s1, char* s2) {        
    while (*s1 != 0 && *s1 == *s2)
        s1++, s2++;
    return *s1 - *s2;
}