#ifndef STRING_H
#define STRING_H

uint32_t strcmp(char* s1, char* s2);
size_t strlen(const char* str);
void* memcpy(void* dst, const void* src, size_t num);
void* memset(void* ptr, int val, size_t num);

#endif
