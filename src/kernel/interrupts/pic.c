/**
 * @file
 * @addtogroup pic
 * @{
 * Programmable Interrupt Controller
 * 
 * The PIC manages IRQs (hardware interrupts). It maps actual IRQs to interrupt
 * vectors. After booting, the IRQs need to be remapped to avoid conflicts.
 * @see http://wiki.osdev.org/8259_PIC
 * @see http://www.lowlevel.eu/wiki/PIC
 */

#include <common.h>
#include <interrupts/pic.h>

#define PIC1_CMD  0x20 ///< the master PIC's command port
#define PIC1_DATA 0x21 ///< the master PIC's data port
#define PIC2_CMD  0xA0 ///< the slave PIC's command port
#define PIC2_DATA 0xA1 ///< the slave PIC's data port
#define PIC_EOI   0x20 ///< "end of interrupt" signals that an IRQ has been handled
#define INT_IRQ0  0x20 ///< where we want to map the master PIC's IRQs
#define INT_IRQ8  0x28 ///< where we want to map the slave PIC's IRQs

/// Initializes the PIC.
void pic_init() {
    print("PIC init ... ");
    /// Before PIC initialization the interrupt vectors look like this:
    /// - 00-1F: Exceptions
    /// - 08-0F: IRQ0-7  (PIC1) - conflicts with exceptions!
    /// - 70-77: IRQ8-15 (PIC2)
    ///
    /// We "bend" the mapping "IRQ -> interrupt vector" to prevent those
    /// conflicts by re-initializing the master and slave PICs and setting
    /// the interrupt vectors 0x20 and 0x28 respectively.
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
    /// So after initialization we have this:
    /// - 00-1F: Exceptions
    /// - 20-27: IRQ0-7  (PIC1) - no more conflicts, all IRQs 0-15 are in a row
    /// - 28-2F: IRQ8-15 (PIC2)
    /// - 30-FF: freely usable (syscalls)
    println("%2aok%a. IRQ0=INT%02x, IRQ8=INT%02x.", irq0, irq8);
}

/**
 * Sends an "end of interrupt" signal. This tells the master PIC that we're ready
 * to process new interrupts. No new interrupts are fired until an EOI is sent.
 * @param intr the interrupt vector that has been handled
 */
void pic_send_eoi(uint8_t intr) {
    /// For IRQ8-15 (issued by slave), we send an EOI to the slave PIC as well.
    if (intr - INT_IRQ0 >= 0x08)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/// @}
