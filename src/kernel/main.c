#include <stdint.h>
#include <intr/gdt.h>

void main() {
    gdt_init();
    *(uint8_t*)0xb8000 = 'H';
    *(uint8_t*)0xb8001 = 0x07;
    *(uint8_t*)0xb8002 = 'e';
    *(uint8_t*)0xb8003 = 0x07;
    *(uint8_t*)0xb8004 = 'l';
    *(uint8_t*)0xb8005 = 0x07;
    *(uint8_t*)0xb8006 = 'l';
    *(uint8_t*)0xb8007 = 0x07;
    *(uint8_t*)0xb8008 = 'o';
    *(uint8_t*)0xb8009 = 0x07;
    while(1);
}