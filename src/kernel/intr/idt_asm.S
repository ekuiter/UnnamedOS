.global idt_isr
idt_isr:
    pusha
    call idt_handle_interrupt
    mov $0x20, %al
    out %al, $0x20
    popa
    iret