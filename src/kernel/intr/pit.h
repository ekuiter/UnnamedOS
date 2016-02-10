#ifndef INTR_PIT_H
#define INTR_PIT_H

#include <stdint.h>

void pit_init(uint32_t new_freq);
void pit_irq();
uint32_t pit_seconds();
void pit_sleep(uint32_t ms);

#endif
