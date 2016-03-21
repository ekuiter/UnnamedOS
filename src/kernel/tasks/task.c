/*
 * Task - controls multitasking
 *
 * http://www.lowlevel.eu/wiki/Teil_6_-_Multitasking
 */

#include <common.h>
#include <tasks/task.h>
#include <tasks/schedule.h>
#include <mem/gdt.h>

void task_create_detailed(task_t* task, void* entry_point, task_stack_t* user_stack,
        size_t user_stack_len, task_stack_t* kernel_stack, size_t kernel_stack_len,
        size_t code_segment, size_t data_segment) {
    // We want to run a task in userspace, so we prepare a CPU state to pop off
    // when a timer interrupt occurs. The CPU state lies at the top of the task's
    // userspace stack:
    task->user_stack = user_stack, task->kernel_stack = kernel_stack;
    task->user_stack_len = user_stack_len, task->kernel_stack_len = kernel_stack_len;
    cpu_state_t* cpu = task->cpu =
            (cpu_state_t*) (kernel_stack + kernel_stack_len - 1 - sizeof(cpu_state_t));
    // make an initial CPU state, setting the registers popped off in isr_asm.S
    // (ESP is set to the pointer value of cpu itself, so the TOS of the user stack.)
    cpu->gs  = cpu->fs  = cpu->es  = cpu->ds  = gdt_get_selector(data_segment);
    // we don't need to set ESP because it is discarded by popa (and set to cpu)
    cpu->edi = cpu->esi = cpu->ebp = cpu->ebx = cpu->edx = cpu->ecx = cpu->eax = 0;
    // we also ignore intr and error, those are always set when entering the kernel
    cpu->eip = task->entry_point = (uintptr_t) entry_point;
    cpu->cs  = gdt_get_selector(code_segment);
    cpu->eflags.dword = 0;         // first reset EFLAGS, then
    cpu->eflags.bits._if = 1;      // enable interrupts, otherwise
    cpu->eflags.bits.reserved = 1; // we can't exit the task once entered!
    // If we want to go to userspace, the following registers are popped by iret.
    // Note that if we stay in the kernel, these values are ignored.
    cpu->user_esp = (uint32_t) user_stack + user_stack_len - 1;
    cpu->user_ss = gdt_get_selector(data_segment);
    schedule_add_task(task); // tell the scheduler to run this task when appropriate
}

void task_create_kernel(task_t* task, void* entry_point,
        task_stack_t* kernel_stack, size_t kernel_stack_len) {
    task_create_detailed(task, entry_point, 0, 0,
            kernel_stack, kernel_stack_len, GDT_RING0_CODE_SEG, GDT_RING0_DATA_SEG);
}

void task_create_user(task_t* task, void* entry_point, task_stack_t* kernel_stack,
        size_t kernel_stack_len, task_stack_t* user_stack, size_t user_stack_len) {
    task_create_detailed(task, entry_point, user_stack, user_stack_len,
            kernel_stack, kernel_stack_len, GDT_RING3_CODE_SEG, GDT_RING3_DATA_SEG);
}
