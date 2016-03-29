/*
 * Task - controls multitasking
 *
 * http://www.lowlevel.eu/wiki/Teil_6_-_Multitasking
 */

#include <common.h>
#include <tasks/task.h>
#include <tasks/schedule.h>
#include <mem/gdt.h>
#include <mem/mmu.h>
#include <mem/vmm.h>

static task_t* task_create_detailed(void* entry_point,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, size_t code_segment, size_t data_segment) {    
    logln("TASK", "Creating task with %dKB kernel and %dKB user stack",
            kernel_stack_len, user_stack_len);
    // Here we allocate a whole page (4KB) which is more than we need.
    task_t* task = vmm_alloc(sizeof(task_t), VMM_KERNEL); // TODO: use a proper heap (malloc)    
    task->page_directory   = page_directory ? page_directory :
        vmm_create_page_directory();
    page_directory_t* old_directory = vmm_load_page_directory(task->page_directory);
    task->kernel_stack     = vmm_alloc(kernel_stack_len, VMM_KERNEL);
    task->user_stack       = vmm_alloc(user_stack_len, VMM_USER | VMM_WRITABLE);
    task->kernel_stack_len = kernel_stack_len;
    task->user_stack_len   = user_stack_len;
    // We want to run a task in userspace, so we prepare a CPU state to pop off
    // when a timer interrupt occurs. The CPU state lies at the top of the task's
    // userspace stack:
    cpu_state_t* cpu = task->cpu = (cpu_state_t*)
            (task->kernel_stack + kernel_stack_len - 1 - sizeof(cpu_state_t));
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
    cpu->user_esp = (uint32_t) task->user_stack + user_stack_len - 1;
    cpu->user_ss = gdt_get_selector(data_segment);
    vmm_load_page_directory(old_directory);
    schedule_add_task(task); // tell the scheduler to run this task when appropriate
    return task;
}

task_t* task_create_kernel(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            0, GDT_RING0_CODE_SEG, GDT_RING0_DATA_SEG);
}

task_t* task_create_user(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len, size_t user_stack_len) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            user_stack_len, GDT_RING3_CODE_SEG, GDT_RING3_DATA_SEG);
}

void task_destroy(task_t* task) {
    if (schedule_get_current_task() == task) {
        println("%4aYou may not destroy a running task%a");
        return;
    }
    page_directory_t* old_directory = vmm_load_page_directory(task->page_directory);
    vmm_free(task->kernel_stack, task->kernel_stack_len);
    vmm_free(task->user_stack, task->user_stack_len);
    vmm_load_page_directory(old_directory);
    vmm_destroy_page_directory(task->page_directory);
    schedule_remove_task(task);
    vmm_free(task, sizeof(task_t));
}