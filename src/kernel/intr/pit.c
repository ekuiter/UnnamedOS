/*
 * Programmable Interval Timer (IRQ0) - used for multitasking and system clock
 *
 * http://lowlevel.eu/wiki/PIT
 * http://wiki.osdev.org/PIT
 * http://stackoverflow.com/questions/7083482/how-to-prevent-gcc-from-optimizing-out-a-busy-wait-loop
 * http://stackoverflow.com/questions/2219829/how-to-prevent-gcc-optimizing-some-statements-in-c
 */

#include <common.h>
#include <intr/pit.h>

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_INIT     0x43
#define PIT_FREQ     1193182
#define MS_TO_TICKS(ms, freq) (freq * ms / 1000) // freq = ticks/1s = ticks/1000ms, so ticks = freq*1000ms

typedef struct {
    uint8_t fmt  : 1; // counter format (binary/bcd)
    uint8_t mode : 3; // usually 2 (rate generator)
    uint8_t byte : 2; // which byte of the counter register to write
    uint8_t chan : 2; // channel to be changed
} __attribute__((packed)) pit_init_t;

static uint32_t freq = 0; // assume undefined frequency in the beginning (actually, this is 18.2065Hz)
static uint32_t seconds = 0, ticks = 0;

void pit_init(uint32_t new_freq) {
    print("PIT init ... ");
    if (new_freq < 19 || new_freq > PIT_FREQ / 2) {
        println("%4afail%a. Frequency must be > 19Hz and < 0.59MHz.");
        return;
    }
    freq = new_freq;
    // With every PIT_FREQ tick the counter is decremented, when the counter reaches 0, an IRQ0 is fired.
    // So our frequency is calculated as freq = PIT_FREQ / counter. We want to tell the PIT the counter
    uint16_t counter = PIT_FREQ / freq; // it needs to decrement, so we rearrange the equation.
    // (If freq < 19, counter > 65535 (uint16_t max) would generate an overflow, so 19 Hz is the lowest freq possible.
    //  If freq > 596591, counter <= 1, but the values 1 and 0 are not allowed: 1 because only the transition from 2 to 1
    //  fires an IRQ0 and that's not possible if we set counter to 1 initially. 0 because we can't divide through 0.)
    // our PIT is going to be a binary rate generator, we want to write to channel 0 (the freely usable one)
    pit_init_t init = {.fmt = 0, .mode = 2, .byte = 3, .chan = 0}; // and we will write the counter's LSB and MSB
    outb(PIT_INIT, *(uint8_t*) &init);
    outb(PIT_CHANNEL0, counter & 0xFF);
    outb(PIT_CHANNEL0, counter >> 8);
    println("%2aok%a. Frequency=%dHz.", freq);
}

void pit_irq() {
    ticks++;
    if (ticks % freq == 0) // every "freq" ticks a second is gone (because 1Hz = 1/s)
        seconds++;
}

uint32_t pit_seconds() {
    return seconds;
}

// we disable GCC's optimization here to prevent the idling loop from being optimized away
__attribute__((optimize("O0"))) void pit_sleep(uint32_t ms) {
    uint32_t wait_until = ticks + MS_TO_TICKS(ms, freq);
    while (ticks != wait_until);
}
