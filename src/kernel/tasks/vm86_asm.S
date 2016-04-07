.global vm86_call_bios_start, vm86_interrupt_hook, vm86_call_bios_end

vm86_call_bios_start: // This is where our VM86 task's entry point is. We
.code16               // tell GAS that now we are in 16-bit/real mode.
        //...         // Here we could add instructions.
vm86_interrupt_hook:  // This label tells the kernel about where the operand
        int $0x00     // $0x00 is in memory, therefore it can be set dynamically.
        //...         // Here we could add instructions.
vm86_call_bios_end:   // This is the task's last byte of code (the kernel copies
        int $0x3      // the code up to this byte). The int $0x3 signals the
                      // kernel to terminate a VM86 task - it consists of only
                      // one byte, otherwise the label would be incorrect!
.code32               // Finally, we switch back to 32-bit/protected mode code.

// To make your own VM86 task, use the following structure:
// code_start:
// .code16
//         // your 16-bit code
// code_end:
//         int $0x3
// .code32
// Note that this code must be position independent (no jumps / addresses)!
// This is a huge limitation, but not needed for our purposes (calling the BIOS).