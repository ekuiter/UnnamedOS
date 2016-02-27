/*
 * Programmable Interval Timer (IRQ0) - used for multitasking and system clock
 *
 * http://lowlevel.eu/wiki/PIT
 * http://wiki.osdev.org/PIT
 * http://stackoverflow.com/questions/7083482/how-to-prevent-gcc-from-optimizing-out-a-busy-wait-loop
 * http://stackoverflow.com/questions/2219829/how-to-prevent-gcc-optimizing-some-statements-in-c
 * http://wiki.osdev.org/PC_Speaker
 * http://www.joachim-neu.de/res/tutorials/lautsprecher.pdf
 */

#include <common.h>
#include <intr/pit.h>

#define PIT_CHANNEL(c)   (0x40 + (c))
#define PIT_INIT         0x43
#define PIT_SPEAKER      0x61
#define PIT_FREQ         1193182
#define MODE_RATE        0x02
#define MODE_SQUARE_WAVE 0x03
#define SPEAKER_ON       0x01
#define SPEAKER_CHANNEL2 0x02
#define MS_TO_TICKS(ms, freq) (freq * ms / 1000) // freq = ticks/1s = ticks/1000ms, so ticks = freq*1000ms

typedef struct {
    uint8_t fmt  : 1; // counter format (binary/bcd)
    uint8_t mode : 3; // usually 2 (rate generator)
    uint8_t byte : 2; // which byte of the counter register to write
    uint8_t chan : 2; // channel to be changed
} __attribute__((packed)) pit_init_t;

static uint32_t freq = 0; // assume undefined frequency in the beginning (actually, this is 18.2065Hz)
static uint32_t seconds = 0, minutes = 0, hours = 0, ticks = 0;

static uint8_t pit_init_channel(uint8_t channel, uint8_t mode, uint32_t freq) {
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

void pit_init(uint32_t new_freq) {
    print("PIT init ... ");
    if (pit_init_channel(0, MODE_RATE, new_freq)) {
        freq = new_freq;
        println("%2aok%a. Frequency=%dHz.", freq);
    } else
        println("%4afail%a. Frequency must be > 18Hz and < 0.59MHz.");
}

void pit_irq() {
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
}

void pit_time() {
    print("%02d:%02d:%02d", hours, minutes, seconds);
}

// we disable GCC's optimization here to prevent the idling loop from being optimized away
__attribute__((optimize("O0"))) void pit_sleep(uint32_t ms) {
    uint32_t wait_until = ticks + MS_TO_TICKS(ms, freq);
    // We do not use < here because wait_until might have overflowed, e.g. wait_until=0xFF, ticks=0xFF00 and in that
    while (ticks != wait_until); // case wait_until < ticks would not wait and terminate immediately.
}

void speaker_on(uint32_t freq) {
    if (!pit_init_channel(2, MODE_SQUARE_WAVE, freq)) {
        println("%4aSpeaker frequency must be > 18Hz and < 0.59MHz%a");
        return;
    }
    uint8_t state = inb(PIT_SPEAKER);
    outb(PIT_SPEAKER, state | SPEAKER_ON | SPEAKER_CHANNEL2);
}

void speaker_off() {
    uint8_t state = inb(PIT_SPEAKER);
    outb(PIT_SPEAKER, state & ~(SPEAKER_ON | SPEAKER_CHANNEL2));
}

void speaker_play(uint32_t freq, uint32_t ms) {
    print("speaker on %d Hz, waiting %d ms, ", freq, ms);
    speaker_on(freq);
    pit_sleep(ms);
    speaker_off();
    println("speaker off");
}

void speaker_test() {
    while (1) {
        speaker_play(523, 200);
        speaker_play(587, 200);
        speaker_play(659, 200);
        speaker_play(698, 200);
        speaker_play(784, 200);
        speaker_play(880, 200);
        speaker_play(988, 200);
        speaker_play(1047, 200);
        speaker_play(988, 200);
        speaker_play(880, 200);
        speaker_play(784, 200);
        speaker_play(698, 200);
        speaker_play(659, 200);
        speaker_play(587, 200);
    }
}