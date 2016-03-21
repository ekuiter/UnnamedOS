int test = 0x12345678;

void main() {
    asm volatile("int $0x30" : : "a" (test));
    while (1);
}
