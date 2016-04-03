#include <lib.h>

void lib_init(putchar_func_t io_putchar_stub, attr_func_t io_attr_stub) {
    io_set_stubs(io_putchar_stub, io_attr_stub);
}
