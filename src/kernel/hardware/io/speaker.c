/*
 * Speaker - this is purely for fun ;)
 *
 * http://wiki.osdev.org/PC_Speaker
 * http://www.joachim-neu.de/res/tutorials/lautsprecher.pdf
 */

#include <common.h>
#include <hardware/io/speaker.h>
#include <hardware/pit.h>

#define PIT_SPEAKER      0x61
#define MODE_SQUARE_WAVE 0x03
#define SPEAKER_ON       0x01
#define SPEAKER_CHANNEL2 0x02

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