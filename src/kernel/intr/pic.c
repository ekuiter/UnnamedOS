/*
 * Programmable Interrupt Controller - manages IRQs (hardware interrupts)
 *
 * http://wiki.osdev.org/8259_PIC
 * http://www.lowlevel.eu/wiki/PIC
 */

#include <common.h>
#include <intr/pic.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20
#define INT_IRQ0  0x20
#define INT_IRQ8  0x28

void pic_init() {
    print("PIC init ... ");
    // interrupt vectors before:
    // 00-1F: Exceptions
    // 08-0F: IRQ0-7  (PIC1) - conflicts with exceptions!
    // 70-77: IRQ8-15 (PIC2)
    // We will now "bend" the mapping "IRQ -> interrupt vector" to prevent those conflicts
    // by re-initializing the master and slave PICs and setting the following interrupt vectors:
    uint8_t irq0 = INT_IRQ0, irq8 = INT_IRQ8;
    // PIC 1 (master) (we use io_wait in case of slow PICs)
    outb(PIC1_CMD,  0x11); io_wait(); // ICW1: Initialize PIC1 + send ICW4
    outb(PIC1_DATA, irq0); io_wait(); // ICW2: IRQ0 interrupt vector
    outb(PIC1_DATA, 0x04); io_wait(); // ICW3: Slave on IRQ2 (bitmask!)
    outb(PIC1_DATA, 0x01); io_wait(); // ICW4: 8086 flag
    outb(PIC1_DATA, 0x00); io_wait(); // demask and enable all IRQs
    // PIC 2 (slave)
    outb(PIC2_CMD,  0x11); io_wait(); // ICW1: Initialize PIC2 + send ICW4
    outb(PIC2_DATA, irq8); io_wait(); // ICW2: IRQ8 interrupt vector
    outb(PIC2_DATA, 0x02); io_wait(); // ICW3: Slave on IRQ2
    outb(PIC2_DATA, 0x01); io_wait(); // ICW4: 8086 flag
    outb(PIC2_DATA, 0x00); io_wait(); // demask and enable all IRQs
    // interrupt vectors after:
    // 00-1F: Exceptions
    // 20-27: IRQ0-7  (PIC1) - no more conflicts, all IRQs 0-15 are in a row
    // 28-2F: IRQ8-15 (PIC2)
    // 30-FF: freely usable (syscalls)
    println("%2aok%a. IRQ0=INT%02x, IRQ8=INT%02x.", irq0, irq8);
}

// EOI = "end of interrupt", a signal that we are ready to process new interrupts
void pic_send_eoi(uint8_t intr) {
    if (intr - INT_IRQ0 >= 0x08) // for IRQ8-15 (issued by slave), we need to send EOIs to the master and slave PIC
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
