#ifndef INTR_PIT_H
#define INTR_PIT_H

#include <stdint.h>

void pit_init(uint32_t new_freq);
void pit_irq();
void pit_time();
void pit_sleep(uint32_t ms);
void speaker_on(uint32_t freq);
void speaker_off();
void speaker_play(uint32_t freq, uint32_t ms);
void speaker_test();

#endif
