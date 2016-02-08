#ifndef INTR_IDT_H
#define INTR_IDT_H

void idt_init();
void idt_interrupts(uint8_t enable);

#endif
