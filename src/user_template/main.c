void main() {
    asm volatile("int $0x30" : : "a" (0x12345678));
    while (1);
}
