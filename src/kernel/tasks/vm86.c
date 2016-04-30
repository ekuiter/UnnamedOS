/**
 * @file
 * @addtogroup vm86
 * @{
 * Virtual 8086 Mode
 * 
 * The VM86 mode runs 16-bit code and calls BIOS functions. A VM86 monitor is
 * needed to emulate sensitive instructions.
 * @see vm86_asm.S
 * @see http://wiki.osdev.org/Virtual_8086_Mode
 * @see http://www.lowlevel.eu/wiki/Virtual_8086_Mode
 * @see http://osdev.berlios.de/v86.html
 * @see http://www.rcollins.org/articles/pvi1/pvi1.html
 * @see http://lowlevel.eu/wiki/Speicherbereiche
 * @see http://www.brokenthorn.com/Resources/OSDevVid2.html
 * @see http://ref.x86asm.net/coder32.html
 */

#include <common.h>
#include <tasks/vm86.h>
#include <tasks/schedule.h>
#include <interrupts/syscall.h>
#include <interrupts/isr.h>
#include <boot/multiboot.h>
#include <mem/gdt.h>
#include <mem/vmm.h>
#include <string.h>
#include <syscall.h>

/** Where the assembly code will be located. The code will be memcpy'd to this
 * location. We set this to the beginning of conventional memory. This way only
 * one VM86 task can be run at a time! */
#define CODE_ADDRESS ((void*) 0x500)
/** The real mode IVT (= Interrupt Vector Table) lies at the start of memory.
 * One of the few occasions to dereference a null pointer ^^ */
#define IVT_ADDRESS ((void*) 0)
#define OPERAND_SIZE 0x66 ///< opcode for overriding operand size
#define OPCODE_PUSHF 0x9C ///< PUSHF triggers a GPF inside VM86 mode
#define OPCODE_POPF  0x9D ///< POPF triggers a GPF inside VM86 mode
#define OPCODE_INT_3 0xCC ///< INT $3 triggers a GPF inside VM86 mode
#define OPCODE_INT   0xCD ///< INT triggers a GPF inside VM86 mode
#define OPCODE_IRET  0xCF ///< IRET triggers a GPF inside VM86 mode
#define OPCODE_CLI   0xFA ///< CLI triggers a GPF inside VM86 mode
#define OPCODE_STI   0xFB ///< STI triggers a GPF inside VM86 mode

/**
 * Emulates an IN instruction.
 * @param opcode  the opcode of the instruction to emulate
 * @param in_func the function to call
 * @param operand the input port
 * @param inc     the instruction length in bytes
 * @param type    in_func's return type
 */
#define CASE_IN(opcode, in_func, operand, inc, type) \
    case (opcode): \
        *(type*) &cpu->r.eax = (in_func)(operand); \
        vm86_increment_eip(cpu, (inc)); \
        break;

/**
 * Emulates an OUT instruction.
 * @param opcode   the opcode of the instruction to emulate
 * @param out_func the function to call
 * @param operand  the output port
 * @param inc      the instruction length in bytes
 */
#define CASE_OUT(opcode, out_func, operand, inc) \
    case (opcode): \
        (out_func)((operand), cpu->r.eax); \
        vm86_increment_eip(cpu, (inc)); \
        break;

/// A real mode far pointer.
typedef struct {
    uint16_t offset, ///< offset inside the segment
            segment; ///< blocks of 16 byte
} __attribute__((packed)) vm86_farptr_t;

static vm86_farptr_t* ivt = IVT_ADDRESS; ///< IVT needed to do BIOS calls
// symbols defined in vm86_asm.S, only the addresses matter
extern const void
        vm86_call_bios_start, ///< start of the BIOS call code
        vm86_call_bios_end,   ///< end of the BIOS call code
        vm86_interrupt_hook;  ///< location of INT in the BIOS call code

/**
 * Translate an address into a far pointer.
 * @param addr the address
 * @return the far pointer
 */
static vm86_farptr_t vm86_get_farptr(void* addr) {
    if ((uintptr_t) addr >= MULTIBOOT_LOWER_MEMORY) {
        println("%4aAddress %08x too large for VM86 mode%a", addr);
        vm86_farptr_t farptr = {.offset = 0, .segment = 0};
        return farptr;
    }
    /// There are many ways to translate an address into a far pointer. We do it
    /// in such a way that our code is likely to only use one segment (which is
    /// easier than handling multiple segments). The equation used is:
    /// ```
    /// paddr = segment * 16 + offset
    /// ```
    uint16_t offset = (uintptr_t) addr & 0xFFFF; // lower word is an offset
    vm86_farptr_t farptr = { 
        .offset = offset,
        // everything else belongs to the segment (bit shifting divides by 64KiB)
        .segment = ((uintptr_t) addr - offset) >> 4
    };
    return farptr;
}

/**
 * Writes a far pointer into another location.
 * @param segment where to write the segment
 * @param offset  where to write the offset
 * @param farptr  the far pointer to write
 */
static void vm86_write_farptr(uint16_t* segment, uint16_t* offset,
        vm86_farptr_t farptr) {
    *segment = farptr.segment;
    *offset  = farptr.offset;
}

/**
 * Translate a far pointer into an address. 
 * @param farptr the far pointer
 * @return the address
 */
static void* vm86_get_address(vm86_farptr_t farptr) {
    // segment * 16 + offset, bit shifting is a little faster
    return (void*) ((farptr.segment << 4) + farptr.offset);
}

/**
 * Creates a VM86 task.
 * @see task_create_detailed
 * @param code_start       start of the 16-bit code
 * @param code_end         end of the 16-bit code
 * @param page_directory   a page directory for the task, if 0 a new one is created
 * @param kernel_stack_len number of bytes to allocate for the kernel stack
 * @param user_stack_len   number of bytes to allocate for the user stack
 * @param registers        parameters to pass to the 16-bit code
 * @return the task's PID
 */
task_pid_t vm86_create_task(void* code_start, void* code_end,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, isr_registers_t* registers) {
    uint8_t old_interrupts = isr_enable_interrupts(0);
    logln("VM86", "Creating VM86 task with %dKB kernel and %dKB user stack",
            kernel_stack_len, user_stack_len);
    task_t* task = vmm_alloc(sizeof(task_t), VMM_KERNEL);
    task->page_directory = page_directory ? page_directory :
        vmm_create_page_directory();
    vmm_modify_page_directory(task->page_directory);
    /// Identity maps the first MiB so our VM86 task can operate inside it.
    /// Because this is not inside the user domain (1GiB and onwards),
    /// bypasses domain checking.
    vmm_enable_domain_check(0);
    vmm_map_range(0, 0, MULTIBOOT_LOWER_MEMORY, VMM_USER | VMM_WRITABLE);
    vmm_enable_domain_check(1);
    uint32_t code_length = (uintptr_t) code_end - (uintptr_t) code_start + 1;
    /// Copies the 16-bit code into conventional memory. We could map the code
    /// as virtual memory, but copying is easier ;) Note that this code might
    /// not reference any labels or so, because it needs to be position
    /// independent. For calling the BIOS this is enough, if we started writing
    /// real 16-bit programs, we would need to improve this.
    memcpy(CODE_ADDRESS, code_start, code_length);
    task->state = TASK_RUNNING;
    task->vm86 = 1;
    task->kernel_stack = vmm_alloc(kernel_stack_len, VMM_KERNEL);
    /// The user stack is located after the code (we assume that's free memory).
    task->user_stack   = (task_stack_t*) ((uintptr_t) CODE_ADDRESS + code_length);
    task->kernel_stack_len = kernel_stack_len;
    task->user_stack_len   = user_stack_len;
    cpu_state_t* cpu = task->cpu = (cpu_state_t*)
            (task->kernel_stack + kernel_stack_len - 1 - sizeof(cpu_state_t));
    // the segment registers are overwritten with the vm86_* values when ireting
    cpu->gs = cpu->fs = cpu->es = cpu->ds = gdt_get_selector(GDT_RING3_DATA_SEG);
    cpu->r = *registers; // here we pass parameters to the code
    vm86_farptr_t entry_point_farptr = vm86_get_farptr(CODE_ADDRESS);
    cpu->eip = entry_point_farptr.offset; // we need to use real mode addressing
    cpu->cs  = entry_point_farptr.segment; // in the form of CS:IP to run the code
    cpu->eflags.dword         = 0;
    cpu->eflags.bits._if      = 1;
    cpu->eflags.bits.reserved = 1;
    cpu->eflags.bits.vm       = 1; // enter virtual 8086 mode
    vm86_farptr_t user_stack_farptr = vm86_get_farptr((void*)
        ((uint32_t) task->user_stack + user_stack_len - 1));
    cpu->user_esp = user_stack_farptr.offset; // the stack pointer is given
    cpu->user_ss  = user_stack_farptr.segment; // in the form SS:SP in real mode
    // this is the same segment where code and stack are located:
    cpu->vm86_es = cpu->vm86_ds = cpu->vm86_fs = cpu->vm86_gs =
            entry_point_farptr.segment;
    vmm_modified_page_directory();
    task_pid_t pid = task_add(task);
    isr_enable_interrupts(old_interrupts);
    return pid;
}

/**
 * Calls a BIOS interrupt.
 * @param interrupt the interrupt vector to call
 * @param registers the parameters for the call
 */
void vm86_call_bios(uint8_t interrupt, isr_registers_t* registers) {
    uint8_t* opcode  = (uint8_t*) &vm86_interrupt_hook; // the interrupt opcode
    uint8_t* operand = opcode + 1; // the actual interrupt vector
    if (*opcode != OPCODE_INT) {
        println("%4aVM86 BIOS handler corrupted%a");
        return;
    }
    *operand = interrupt;
    vm86_create_task((void*) &vm86_call_bios_start, (void*) &vm86_call_bios_end,
            0, _4KB, _4KB, registers);
}

/**
 * Emulates a PUSH instruction.
 * @param cpu   the CPU state to modify
 * @param value the value to push
 */
static void vm86_push(cpu_state_t* cpu, uint16_t value) {
    vm86_farptr_t sssp = {.offset = cpu->user_esp, .segment = cpu->user_ss};
    uint16_t* new_esp = (uint16_t*) vm86_get_address(sssp) - 1;
    *new_esp = value;
    vm86_write_farptr((uint16_t*) &cpu->user_ss, (uint16_t*) &cpu->user_esp,
            vm86_get_farptr(new_esp));
}

/**
 * Emulates a POP instruction.
 * @param cpu the CPU state to modify
 * @return the popped value
 */
static uint16_t vm86_pop(cpu_state_t* cpu) {
    vm86_farptr_t sssp = {.offset = cpu->user_esp, .segment = cpu->user_ss};
    uint16_t* esp = vm86_get_address(sssp);
    uint16_t value = *esp;
    vm86_write_farptr((uint16_t*) &cpu->user_ss, (uint16_t*) &cpu->user_esp,
            vm86_get_farptr(esp + 1));
    return value;
}

/**
 * Emulates the completion of an instruction.
 * @param cpu the CPU state to modify
 * @param inc the instruction length in bytes
 */
static void vm86_increment_eip(cpu_state_t* cpu, size_t inc) {
    vm86_farptr_t csip = {.offset = cpu->eip, .segment = cpu->cs};
    vm86_write_farptr(&cpu->cs, (uint16_t*) &cpu->eip,
            vm86_get_farptr((uint8_t*) vm86_get_address(csip) + inc));
}

/**
 * Monitors a VM86 task by emulating sensitive instructions.
 * @param cpu the task's CPU state
 * @return whether the monitor handled the GPF
 */
static uint8_t vm86_monitor(cpu_state_t* cpu) {
    // if we are not in VM86 mode, do not handle this GPF
    if (!task_get_vm86(schedule_get_current_task()))
        return 0;
    // First we determine which instruction triggered the GPF. For that we need
    // to translate real mode (CS:IP) to protected mode (EIP) addresses.
    vm86_farptr_t csip = {.offset = cpu->eip, .segment = cpu->cs};
    uint8_t* eip = vm86_get_address(csip);
    uint16_t opcode = eip[0] == OPERAND_SIZE ? OPERAND_SIZE << 8 | eip[1] : eip[0];    
    // There are multiple instructions that fire an GPF and need to be emulated
    switch (opcode) { // in different ways.
        case OPCODE_PUSHF:
            vm86_push(cpu, cpu->eflags.dword); // push FLAGS onto user stack
            vm86_increment_eip(cpu, 1);
            break;
        case OPCODE_POPF:
            vm86_pop(cpu); // pop FLAGS, for simplicity we ignore the value
            vm86_increment_eip(cpu, 1); 
            break;
        case OPCODE_INT_3:
            println("BIOS call returned EAX=%08x, EBX=%08x, ECX=%08x, EDX=%08x",
                    cpu->r.eax, cpu->r.ebx, cpu->r.ecx, cpu->r.edx);
            sys_exit(0); // exit the VM86 task (with a breakpoint exception)
            break;
        case OPCODE_INT:
            logln("VM86", "Emulating INT %02x", eip[1]);
            // Here the VM86 code called a BIOS function, so we set up the USER
            // stack how a 8086 CPU would do it: push FLAGS, CS and IP. These
            // are the values used when the BIOS iret's, so CS:IP points to the
            // next instruction after the INT.
            csip = vm86_get_farptr(eip + 2); // = EIP + OPCODE_INT + OPERAND
            vm86_push(cpu, cpu->eflags.dword); // push FLAGS (16-bit!)
            vm86_push(cpu, csip.segment); // push CS (next instruction)
            vm86_push(cpu, csip.offset); // push IP
            // this is where we want to iret to: the actual BIOS code
            vm86_write_farptr(&cpu->cs, (uint16_t*) &cpu->eip, ivt[eip[1]]);
            break;
        case OPCODE_IRET:
            logln("VM86", "Emulating IRET");
            // This is executed when a BIOS call has finished. We reverse the
            // pushes we did above.
            cpu->eip = vm86_pop(cpu); // pop IP
            cpu->cs = vm86_pop(cpu); // pop CS
            vm86_pop(cpu); // pop FLAGS
            break;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
        // Different flavours of port mapped I/O. We could use a TSS I/O map
        CASE_IN (0xE4,   inb,  eip[1], 2, uint8_t);  // to manage this, for now
        CASE_IN (0xE5,   inw,  eip[1], 2, uint16_t); // we just allow any I/O
        CASE_IN (0x66E5, inl,  eip[2], 3, uint32_t); // operation.
        CASE_OUT(0xE6,   outb, eip[1], 2);
        CASE_OUT(0xE7,   outw, eip[1], 2);
        CASE_OUT(0x66E7, outl, eip[2], 3);
        CASE_IN (0xEC,   inb,  cpu->r.edx, 1, uint8_t);
        CASE_IN (0xED,   inw,  cpu->r.edx, 1, uint16_t);
        CASE_IN (0x66ED, inl,  cpu->r.edx, 2, uint32_t);
        CASE_OUT(0xEE,   outb, cpu->r.edx, 1);
        CASE_OUT(0xEF,   outw, cpu->r.edx, 1);
        CASE_OUT(0x66EF, outl, cpu->r.edx, 2);
#pragma GCC diagnostic pop
        case OPCODE_CLI: // we pretend to enable / disable interrupts,
        case OPCODE_STI: // should work most times
            vm86_increment_eip(cpu, 1); 
            break;
        default:
            panic("VM86 opcode %02x unhandled (CS:IP=%04x:%04x)",
                    opcode, cpu->cs, cpu->eip);
    }
    return 1;
}

/**
 * Handles general protection faults.
 * @param cpu the CPU state
 * @return a (possibly altered) CPU state
 */
static cpu_state_t* vm86_handle_gpf(cpu_state_t* cpu) {
    if (!vm86_monitor(cpu))
        panic("%4aEX%02x (EIP=%08x)", cpu->intr, cpu->eip);
    return cpu;
}

/// Initializes VM86 mode.
void vm86_init() {
    isr_register_handler(ISR_EXCEPTION(0x0D), vm86_handle_gpf);
}

/// @}
