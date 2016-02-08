#include <intr/isr.h>
#include <intr/pic.h>
#include <io/output.h>

/*
 * Interrupt Service Routine - handles IRQs, exceptions and syscalls
 *
 * http://www.lowlevel.eu/wiki/Teil_5_-_Interrupts
 * http://www.lowlevel.eu/wiki/ISR
 * http://wiki.osdev.org/Exceptions
 */

#define IS_EXCEPTION(intr) (intr <= 0x1F)
#define IS_IRQ(intr)       (intr >= 0x20 && intr <= 0x2F)
#define IS_SYSCALL(intr)   (intr == 0x30)
#define INT_IRQ0  0x20
#define INT_TIMER INT_IRQ0

// registers pushed in idt_isr (see isr_asm.S)
typedef struct {
    uint16_t gs, : 16, fs, : 16, es, : 16, ds, : 16;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax, intr, error, eip;
    uint16_t cs, : 16;
    uint32_t eflags, user_esp, user_ss;
} __attribute__((packed)) cpu_state_t;

void isr_interrupts(uint8_t enable) {
    if (enable) {
        io_putstr("Enabling interrupts ... ");
        asm volatile("sti");
    } else {
        io_putstr("Disabling interrupts ... ");
        asm volatile("cli");
    }
    io_attr(IO_GREEN);
    io_putstr("ok");
    io_attr(IO_DEFAULT);
    io_putstr(".\n");
}

#define panic(s) _panic(s, __FILE__, __LINE__)
#define assert(exp) ((exp) ? (void) 0 : panic("Assert failed"))

extern void halt();

void _panic(char* msg, char* file, uint32_t line) {
    io_attr(IO_RED);
    io_putstr(msg);
    io_putstr(" at ");
    io_putstr(file);
    io_putchar(':');
    io_putint(line, 10, 0, 0);
    halt();
}

// ESP points to the CPU state, returns a (possibly new) ESP to allow switching tasks
uint32_t isr_handle_interrupt(uint32_t esp) {
    cpu_state_t* cpu = (cpu_state_t*) esp;
    if (IS_EXCEPTION(cpu->intr)) {
        switch (cpu->intr) {
            default:
                io_attr(IO_RED);
                io_putstr("EX");
                io_putint(cpu->intr, 16, 2, '0');
                io_putstr(" (EIP=");
                io_putint(cpu->eip, 16, 8, '0');
                panic(")");
        }
    } else if (IS_IRQ(cpu->intr)) {
        switch (cpu->intr) {
            case INT_TIMER: break;
            default:
                io_attr(IO_GREEN);
                io_putstr("IRQ");
                io_putint(cpu->intr - INT_IRQ0, 16, 2, '0');
                io_attr(IO_DEFAULT);
        }
        pic_send_eoi(cpu->intr);
    } else if (IS_SYSCALL(cpu->intr)) {
        switch (cpu->eax) {
            case 0: break;
            default:
                io_attr(IO_RED);
                io_putstr("SYS");
                io_putint(cpu->eax, 16, 8, '0');
                io_attr(IO_DEFAULT);
        }
    }
    return esp;
}