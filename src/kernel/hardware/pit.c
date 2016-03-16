/*
 * Programmable Interval Timer (IRQ0) - used for multitasking and system clock
 *
 * http://lowlevel.eu/wiki/PIT
 * http://wiki.osdev.org/PIT
 * http://stackoverflow.com/questions/7083482/how-to-prevent-gcc-from-optimizing-out-a-busy-wait-loop
 * http://stackoverflow.com/questions/2219829/how-to-prevent-gcc-optimizing-some-statements-in-c
 */

#include <common.h>
#include <hardware/pit.h>

#define PIT_CHANNEL(c)   (0x40 + (c))
#define PIT_INIT         0x43
#define PIT_FREQ         1193182
#define MODE_RATE        0x02
#define MS_TO_TICKS(ms, freq) (freq * ms / 1000) // freq = ticks/1s = ticks/1000ms, so ticks = freq*1000ms

typedef struct {
    uint8_t fmt  : 1; // counter format (binary/bcd)
    uint8_t mode : 3; // usually 2 (rate generator)
    uint8_t byte : 2; // which byte of the counter register to write
    uint8_t chan : 2; // channel to be changed
} __attribute__((packed)) pit_init_t;

static uint32_t freq = 0; // assume undefined frequency in the beginning (actually, this is 18.2065Hz)
static uint32_t seconds = 0, minutes = 0, hours = 0, ticks = 0;

uint8_t pit_init_channel(uint8_t channel, uint8_t mode, uint32_t freq) {
    if (freq < 19 || freq > PIT_FREQ / 2)
        return 0;
    // With every PIT_FREQ tick the counter is decremented, when the counter reaches 0, on channel 0 an IRQ0 is fired,
    // on channel 2 the speaker is turned on. So our frequency is calculated as freq = PIT_FREQ / counter. We want to
    uint16_t counter = PIT_FREQ / freq; // tell the PIT the counter it needs to decrement, so we rearrange the equation.
    // (If freq < 19, counter > 65535 (uint16_t max) would generate an overflow, so 19 Hz is the lowest freq possible.
    //  If freq > 596591, counter <= 1, but the values 1 and 0 are not allowed: 1 because only the transition from 2 to 1
    //  fires an IRQ0 and that's not possible if we set counter to 1 initially. 0 because we can't divide through 0.)
    // Our PIT is going to be a binary rate generator, we want to write to channel 0 (the freely usable one) or we init
    // channel 2 (which controls the speaker) as a binary square wave generator. Then we will write the counter's LSB
    pit_init_t init = {.fmt = 0, .mode = mode, .byte = 3, .chan = channel}; // first and next its MSB.
    outb(PIT_INIT, *(uint8_t*) &init);
    outb(PIT_CHANNEL(channel), counter & 0xFF);
    outb(PIT_CHANNEL(channel), counter >> 8);
    return 1;
}

static cpu_state_t* pit_handle_interrupt(cpu_state_t* cpu) {
    ticks++;
    if (ticks % freq == 0) { // every "freq" ticks a second is gone (because 1Hz = 1/s)
        seconds++;
        if (seconds % 60 == 0) {
            seconds = 0;
            minutes++;
            if (minutes % 60 == 0) {
                minutes = 0;
                hours++;
            }
        }
    }
    return cpu;
}

void pit_init(uint32_t new_freq) {
    print("PIT init ... ");
    isr_register_handler(ISR_IRQ(0), pit_handle_interrupt);
    if (pit_init_channel(0, MODE_RATE, new_freq)) {
        freq = new_freq;
        println("%2aok%a. Frequency=%dHz.", freq);
    } else
        println("%4afail%a. Frequency must be > 18Hz and < 0.59MHz.");
}

void pit_time() {
    print("%02d:%02d:%02d", hours, minutes, seconds);
}

// we disable GCC's optimization here to prevent the idling loop from being optimized away
__attribute__((optimize("O0"))) void pit_sleep(uint32_t ms) {
    uint32_t wait_until = ticks + MS_TO_TICKS(ms, freq); // If wait_until has overflowed, we
    while (wait_until < ticks); // wait until ticks overflows, then it approaches
    while (ticks < wait_until); // wait_until from the "bottom" as expected.
    // (I used != before but when using timeouts and not an idling loop, the
    // wait_until moment can easily be missed when doing "heavier" tasks like I/O
    // polling. > and < are more robust in that regard.)
    // Alternative implementation with timeouts from below:
    // pit_timeout_t timeout = pit_make_timeout(ms); // This implementation works,
    // while (!pit_timed_out(&timeout)); // but is slower and not as straightforward.
}

pit_timeout_t pit_make_timeout(uint32_t ms) { // timeout constructor
    pit_timeout_t timeout = { // avoid ticks + INT_MAX, this overflows to ticks + 0,
        .wait_until = ticks + MS_TO_TICKS(ms, freq), // waiting NOT AT ALL.
        .state = PS2_WAITING_UNTIL_OVERFLOW // We start by waiting until ticks overflows.
    };
    return timeout;
}

int8_t pit_timed_out(pit_timeout_t* timeout) {   
    switch (timeout->state) { // simple state machine mimicking pit_sleep's
        case PS2_WAITING_UNTIL_OVERFLOW: // behaviour in a more "asynchronous" style
            if (!(timeout->wait_until < ticks)) // like pit_sleep's 1st while
                timeout->state = PS2_WAITING_UNTIL_TIMEOUT;
            return 0;
        case PS2_WAITING_UNTIL_TIMEOUT:
            if (!(ticks < timeout->wait_until)) { // like pit_sleep's 2nd while
                timeout->state = PS2_TIMED_OUT;
                return 1;
            }
            return 0;
        case PS2_TIMED_OUT:
            return 1;
        default:
            return -1;
    }
}
