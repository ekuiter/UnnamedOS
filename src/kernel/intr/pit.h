#ifndef INTR_PIT_H
#define INTR_PIT_H

#include <stdint.h>
#include <intr/isr.h>

typedef struct {
    uint32_t wait_until;
    enum {
        PS2_WAITING_UNTIL_OVERFLOW,
        PS2_WAITING_UNTIL_TIMEOUT,
        PS2_TIMED_OUT
    } state;
} pit_timeout_t;

void pit_init(uint32_t new_freq);
void pit_time();
void pit_sleep(uint32_t ms);
pit_timeout_t pit_make_timeout(uint32_t ms);
int8_t pit_timed_out(pit_timeout_t* timeout);
void speaker_on(uint32_t freq);
void speaker_off();
void speaker_play(uint32_t freq, uint32_t ms);
void speaker_test();

#endif
