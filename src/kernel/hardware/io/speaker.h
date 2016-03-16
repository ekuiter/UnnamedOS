#ifndef HARDWARE_IO_SPEAKER_H
#define HARDWARE_IO_SPEAKER_H

#include <stdint.h>

void speaker_on(uint32_t freq);
void speaker_off();
void speaker_play(uint32_t freq, uint32_t ms);
void speaker_test();

#endif
