#include <io.h>
#include <syscall.h>

#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

static putchar_func_t io_putchar_stub = sys_io_putchar;
static attr_func_t io_attr_stub = sys_io_attr;

void io_set_stubs(putchar_func_t _io_putchar_stub, attr_func_t _io_attr_stub) {
    io_putchar_stub = _io_putchar_stub;
    io_attr_stub = _io_attr_stub;
}

uint16_t io_putstr(char* s, putchar_func_t putchar_func) {
    uint16_t count = 0;
    while (*s != '\0')
        count += putchar_func(*s++);
    return count;
}

uint16_t io_putint(uint32_t n, uint8_t radix, int8_t pad, uint8_t pad_char,
        putchar_func_t putchar_func) {
    if (radix < 2 || radix > 36)
        return io_putstr("radix invalid", putchar_func);
    uint8_t buf_len = sizeof(n) * 8 + 1; // max. string length is "bits of n + \0 character"
    char buf[buf_len], digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* cur = &buf[buf_len - 1]; // cur points to the last element
    *cur-- = '\0';
    if (n == 0) {
        *cur-- = '0';
        pad--;
    } else {
        for (; n != 0; n /= radix) { // cycle digits backwards
            *cur-- = digits[n % radix]; // choose a digit to display
            pad--;
        }
    }
    uint16_t count = 0;
    for (; pad > 0; pad--)
        count += putchar_func(pad_char); // put padding characters
    return count + io_putstr(cur + 1, putchar_func);
}

// Variadic print function, takes format strings like "0x%08x" and any arguments. Arguments are "automagically"
// promoted (char, short to int, float to double) so that every argument following fmt is 4 bytes in size (apart
// from uint64_t and double which are not supported by this implementation, so we ignore them). That means we can
// interpret &fmt as a uint32_t pointer (arg) and increment it (++arg) to access all the additional arguments,
// doing essentially what the macros va_start, va_arg and va_end would do if we had stdarg.h.
uint16_t vprint(char* fmt, uint32_t* arg, putchar_func_t putchar_func) {
    uint16_t count = 0;
    for (char* cur = fmt; *cur != '\0'; cur++) { // iterate over format string
        if (*cur != '%') // output non-% characters normally
            count += putchar_func(*cur);
        else { // process format specifiers
            int8_t pad = 0; // default values for integer padding
            uint8_t pad_char = ' ';
            if (*++cur == '0') { // if %0..., pad with zeroes
                pad_char = '0';
                cur++;
            }
            for (; IS_DIGIT(*cur); cur++) // convert pad string into integer
                pad = pad * 10 + (*cur - '0'); // powers of ten, (c - '0') converts a single digit to int
            switch (*cur) { // %d outputs decimal, %x hexadecimal numbers and so on
                case 'd': // decimal
                    if (*++arg & 0x80000000) { // is negative
                        putchar_func('-');
                        pad--;
                        *arg = -*arg;
                    }
                    count += io_putint(*arg, 10, pad, pad_char, putchar_func);
                    break;
                case 'u': // decimal
                    count += io_putint(*++arg, 10, pad, pad_char, putchar_func);
                    break;
                case 'x': // hexadecimal
                    count += io_putint(*++arg, 16, pad, pad_char, putchar_func);
                    break;
                case 'b': // binary
                    count += io_putint(*++arg, 2, pad, pad_char, putchar_func);
                    break;
                case 'o': // octal
                    count += io_putint(*++arg, 8, pad, pad_char, putchar_func);
                    break;
                case 'c': // single character
                    count += putchar_func((uint8_t) *++arg);
                    break;
                case 's': // 0-terminated string
                    count += io_putstr((char*) *++arg, putchar_func);
                    break;
                case 'a': // attribute byte
                    io_attr_stub(pad == 0 ? IO_DEFAULT : pad);
                    break;
                case '%': // escaped %
                    count += putchar_func('%');
                    break;
            }
        }
    }
    return count;
}

uint16_t print(char* fmt, ...) {
    return vprint(fmt, (uint32_t*) &fmt, io_putchar_stub);
}

uint16_t println(char* fmt, ...) {
    return vprint(fmt, (uint32_t*) &fmt, io_putchar_stub) + io_putchar_stub('\n');
}

uint16_t fprint(putchar_func_t putchar_func, char* fmt, ...) {
    return vprint(fmt, (uint32_t*) &fmt, putchar_func);
}

uint16_t fprintln(putchar_func_t putchar_func, char* fmt, ...) {
    return vprint(fmt, (uint32_t*) &fmt, putchar_func) + putchar_func('\n');
}