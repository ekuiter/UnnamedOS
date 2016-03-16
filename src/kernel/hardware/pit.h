#ifndef HARDWARE_PIT_H
#define HARDWARE_PIT_H

#include <stdint.h>
#include <interrupts/isr.h>

typedef struct {
    uint32_t wait_until;
    enum {
        PS2_WAITING_UNTIL_OVERFLOW,
        PS2_WAITING_UNTIL_TIMEOUT,
        PS2_TIMED_OUT
    } state;
} pit_timeout_t;

uint8_t pit_init_channel(uint8_t channel, uint8_t mode, uint32_t freq);
void pit_init(uint32_t new_freq);
void pit_time();
void pit_sleep(uint32_t ms);
pit_timeout_t pit_make_timeout(uint32_t ms);
int8_t pit_timed_out(pit_timeout_t* timeout);

#endif
