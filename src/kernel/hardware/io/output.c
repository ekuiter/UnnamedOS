/*
 * Output to Screen
 *
 * http://wiki.osdev.org/Printing_to_Screen
 * http://wiki.osdev.org/Text_UI
 * https://www.eskimo.com/~scs/cclass/int/sx11b.html
 * https://www.eskimo.com/~scs/cclass/int/sx11c.html
 */

#include <common.h>
#include <hardware/io/output.h>
#include <mem/vmm.h>

#define IO_CHARS    (IO_COLS * IO_ROWS)
#define IO_MEM      ((uint8_t*) 0xB8000)
#define IO_LEN      4000
#define TAG_LENGTH  8

static uint8_t* video = IO_MEM;
static uint8_t attr = IO_DEFAULT;
static size_t cursor = 0;
static uint8_t logging_enabled = 1;

void io_use_video_memory() {
    // map the video memory somewhere into the virtual address space
    video = vmm_use_physical_memory(video, IO_LEN, VMM_KERNEL);
}

static void io_setchar(uint8_t c, size_t pos) {
    video[(pos % IO_CHARS) * 2] = c;
}

static void io_setattr(uint8_t attr, size_t pos) {
    video[(pos % IO_CHARS) * 2 + 1] = attr;
}

uint8_t io_attr(uint8_t new_attr) {
    uint8_t old_attr = attr;
    attr = new_attr;
    return old_attr;
}

size_t io_cursor(int32_t new_cursor) {
    size_t old_cursor = cursor;
    if (new_cursor >= 0)
        cursor = new_cursor;
    return old_cursor;
}

uint16_t io_putchar(uint8_t c) {
    if (c == '\n')
        cursor = cursor + IO_COLS - (cursor % IO_COLS);
    else {
        io_setchar(c, cursor);
        io_setattr(attr, cursor);
        cursor = (cursor + 1) % IO_CHARS;
    }
    return 1;
}

void io_clear(putchar_func_t putchar_func) {
    io_cursor(0);
    for (int i = 0; i < IO_CHARS; i++)
        putchar_func('\0');
}

uint16_t log(char* tag, char* fmt, ...) {
    if (!logging_enabled)
        return 0;
    if (!tag)
        return vprint(fmt, (uint32_t*) &fmt, bochs_log);
    uint16_t count = vprint("[%s", (uint32_t*) (&tag - 1), bochs_log);
    while (count <= TAG_LENGTH)
        count += bochs_log(' ');
    return count + vprint("] ", (uint32_t*) (&tag - 1), bochs_log) +
            vprint(fmt, (uint32_t*) &fmt, bochs_log);
}

uint16_t logln(char* tag, char* fmt, ...) {
    if (!logging_enabled)
        return 0;
    if (!tag)
        return vprint(fmt, (uint32_t*) &fmt, bochs_log) + bochs_log('\n');
    uint16_t count = vprint("[%s", (uint32_t*) (&tag - 1), bochs_log);
    while (count <= TAG_LENGTH)
        count += bochs_log(' ');
    return count + vprint("] ", (uint32_t*) (&tag - 1), bochs_log) +
            vprint(fmt, (uint32_t*) &fmt, bochs_log) + bochs_log('\n');
}

void io_set_logging(uint8_t _logging_enabled) {
    logging_enabled = _logging_enabled;
}