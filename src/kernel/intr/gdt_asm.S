.global gdt_flush
gdt_flush:

// We need to load the CS register which can only be done by doing a far jump.
// 0x08 then is loaded into CS which has the 16-bit format
// fedcba9876543210
// a            bc
// a being the index of the segment in the GDT, b whether we're in GDT (0) or IDT (1) and c the ring of the segment.
// So 0x08 means 0000000000001000, a is 1, b is 0 and c is 0; so 0x08 describes the 2nd entry of the GDT (our code segment which is in ring 0).

gdt_flush_code_segment:
        ljmp $0x0008, $gdt_flush_data_segment
        // We jump to the next label below with the side effect that CS is set to 0x08.
        // The jump address ist calculated roughly as follows: gdt[index($0x08)].base + $gdt_flush_data_segment.
        // index($0x08) is 1 (as explained above), so we look for the base of the 2nd GDT entry.
        // This base is 0 (see gdt_init in gdt.c), so the jump address is 0 + $gdt_flush_data_segment = $gdt_flush_data_segment,
        // which means we just jump to gdt_flush_data_segment normally (with flushing CS as side effect).

gdt_flush_data_segment:
        mov $0x0010, %ax // To flush the data (DS), stack (SS), extra (ES) and general purpose (FS/GS) segment registers, we can
        mov %ax, %ds     // just load our segment selector directly with mov. We use 0x10 because it means 0000000000010000,
        mov %ax, %es     // a is 2, b is 0 and c is 0; so 0x10 describes the 3rd entry of the GDT (our data segment which is in ring 0).
        mov %ax, %fs
        mov %ax, %gs
        mov %ax, %ss
        ret              // All the segment registers are set up now, we can return to the calling C code.
