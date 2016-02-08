.global idt_isr
idt_isr:
    pusha
    call idt_handle_interrupt
    popa
    iret