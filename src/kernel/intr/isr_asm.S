.global isr_common
isr_common:
// At this place, the CPU has already pushed EFLAGS, CS and EIP to the stack and, if we were in userspace
// before getting interrupted, SS and ESP. In this case, SS and ESP have been set using the TSS.
// Additionally, in the ISR stubs we pushed an error code and the interrupt vector. Now we save the remaining registers:
    pusha    // save all general purpose registers
    push %ds // and the segment registers (except SS) onto the stack
    push %es
    push %fs
    push %gs
    push %esp // push ESP again so we can freely switch stacks by returning the new ESP from isr_handle_interrupt
// Put the interrupt handling code in a defined state where we can access all kernel data, this is only necessary
// when coming from userspace because DS, ES, FS and GS might hold a RING3 selector.
// SS does not need to be altered because (a) we were in RING0 before and SS has already the right selector
// or (b) we were in RING3 before and SS was set to RING0 from the TSS.
    mov $0x0010, %ax // 0x0010 is the RING0 data segment (see gdt_asm.S)
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    call isr_handle_interrupt // jump to C
    mov %eax, %esp // here a stack switch (possibly) happens if we want to switch tasks, we need to ensure that
                   // the new stack pops off the right registers below!
    pop %gs // pop everything neatly from the stack
    pop %fs
    pop %es
    pop %ds
    popa // this pops everything pusha pushed (except ESP!)
    add $8, %esp // pop interrupt and error code
    iret // return from interrupt, takes care of popping EFLAGS, CS and so on

// These ISR stubs push an error code (if necessary) and the interrupt vector before jumping into the common ISR handler:
.macro isr_intr nr
.global isr_intr_\nr
isr_intr_\nr:
    push $0 // no error code
    push $\nr // interrupt vector
    jmp isr_common
.endm

.macro isr_intr_with_error nr
.global isr_intr_\nr
isr_intr_\nr:
    // error code is already on the stack
    push $\nr // interrupt vector
    jmp isr_common
.endm

isr_intr 0x00 // 00-1F: Exceptions
isr_intr 0x01
isr_intr 0x02
isr_intr 0x03
isr_intr 0x04
isr_intr 0x05
isr_intr 0x06
isr_intr 0x07
isr_intr_with_error 0x08
isr_intr 0x09
isr_intr_with_error 0x0A
isr_intr_with_error 0x0B
isr_intr_with_error 0x0C
isr_intr_with_error 0x0D
isr_intr_with_error 0x0E
isr_intr 0x0F
isr_intr 0x10
isr_intr_with_error 0x11
isr_intr 0x12
isr_intr 0x13
isr_intr 0x14
isr_intr 0x15
isr_intr 0x16
isr_intr 0x17
isr_intr 0x18
isr_intr 0x19
isr_intr 0x1A
isr_intr 0x1B
isr_intr 0x1C
isr_intr 0x1D
isr_intr 0x1E
isr_intr 0x1F
isr_intr 0x20 // 20-27: IRQ0-7 (PIC1) (see pic.c)
isr_intr 0x21
isr_intr 0x22
isr_intr 0x23
isr_intr 0x24
isr_intr 0x25
isr_intr 0x26
isr_intr 0x27
isr_intr 0x28 // 28-2F: IRQ8-15 (PIC2)
isr_intr 0x29
isr_intr 0x2A
isr_intr 0x2B
isr_intr 0x2C
isr_intr 0x2D
isr_intr 0x2E
isr_intr 0x2F
isr_intr 0x30 // 30: syscall
