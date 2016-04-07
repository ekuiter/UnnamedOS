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
#include <boot/multiboot.h>
#include <string.h>

static uint32_t pid = 0; // process id counter

uint32_t task_increment_pid() {
    return ++pid;
}

static task_t* task_create_detailed(void* entry_point,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, size_t code_segment, size_t data_segment) {
    uint32_t pid = task_increment_pid();
    logln("TASK", "Creating task %d with %dKB kernel and %dKB user stack",
            pid, kernel_stack_len, user_stack_len);
    // Here we allocate a whole page (4KB) which is more than we need.
    task_t* task = vmm_alloc(sizeof(task_t), VMM_KERNEL); // TODO: use a proper heap (malloc)    
    task->page_directory = page_directory ? page_directory :
        vmm_create_page_directory();
    vmm_modify_page_directory(task->page_directory);
    task->pid = pid;
    task->vm86 = 0;
    task->kernel_stack = vmm_alloc(kernel_stack_len, VMM_KERNEL);
    task->user_stack   = vmm_alloc(user_stack_len, VMM_USER | VMM_WRITABLE);
    task->kernel_stack_len = kernel_stack_len;
    task->user_stack_len   = user_stack_len;
    // We want to run a task in userspace, so we prepare a CPU state to pop off
    // when a timer interrupt occurs. The CPU state lies at the top of the task's
    // userspace stack:
    cpu_state_t* cpu = task->cpu = (cpu_state_t*)
            (task->kernel_stack + kernel_stack_len - 1 - sizeof(cpu_state_t));
    // make an initial CPU state, setting the registers popped off in isr_asm.S
    // (ESP is set to the pointer value of cpu itself, so the TOS of the user stack.)
    cpu->gs = cpu->fs = cpu->es = cpu->ds = gdt_get_selector(data_segment);
    // we don't need to set ESP because it is discarded by popa (and set to cpu)
    cpu->r.edi = cpu->r.esi = cpu->r.ebp = cpu->r.ebx =
            cpu->r.edx = cpu->r.ecx = cpu->r.eax = 0;
    // we also ignore intr and error, those are always set when entering the kernel
    cpu->eip = task->entry_point = (uintptr_t) entry_point;
    cpu->cs  = gdt_get_selector(code_segment);
    cpu->eflags.dword         = 0; // first reset EFLAGS, then
    cpu->eflags.bits._if      = 1; // enable interrupts, otherwise
    cpu->eflags.bits.reserved = 1; // we can't exit the task once entered!
    // If we want to go to userspace, the following registers are popped by iret.
    // Note that if we stay in the kernel, these values are ignored.
    cpu->user_esp = (uint32_t) task->user_stack + user_stack_len - 1;
    cpu->user_ss  = gdt_get_selector(data_segment);
    // The VM86 values will be ignored so we don't need to set them.
    vmm_modified_page_directory();
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
    if (schedule_task_running(task)) {
        println("%4aYou may not destroy a running task%a");
        return;
    }
    logln("TASK", "Destroying task %d", task->pid);
    vmm_modify_page_directory(task->page_directory);
    vmm_free(task->kernel_stack, task->kernel_stack_len);
    vmm_free(task->user_stack, task->user_stack_len);
    vmm_modified_page_directory();
    schedule_finalize_task(task);
    vmm_destroy_page_directory(task->page_directory);
    vmm_free(task, sizeof(task_t));
}

void task_add(task_t** tasks, task_t* task) {
    // TODO: prevent duplicate entries
    task->next_task = *tasks;
    *tasks = task;
}

void task_remove(task_t** tasks, task_t* task) {
    if (!*tasks) // if there are no tasks, we are done
        return;
    if (task == *tasks) { // the task to remove is at the beginning
        *tasks = (*tasks)->next_task;
        return;
    }
    task_t* cur = *tasks;
    while (cur->next_task && cur->next_task != task)
        cur = cur->next_task;
    if (!cur->next_task) // the task to remove is not in the list
        return;
    // otherwise, the task is pointed to by cur->next_task, so remove it:
    cur->next_task = cur->next_task->next_task;
}

task_t* task_get_next_task(task_t** tasks, task_t* task, uint8_t cyclic) {
    if (!task)
        return *tasks;
    // If we reached the linked list's end,
    else if (!task->next_task) // start over with the first task
        return cyclic ? *tasks : 0; // (depending on the parameter cyclic)
    else
        return task->next_task; // or continue in the list
}

uint8_t task_contains_task(task_t** tasks, task_t* task) {
    task_t* cur = 0;
    while ((cur = task_get_next_task(tasks, cur, 0)))
        if (cur == task)
            return 1;
    return 0;
}

void task_dump(task_t** tasks) {
    task_t* cur = 0;
    if (task_get_next_task(tasks, cur, 0))
        logln("TASK", "Task list at %08x:", tasks);
    else
        logln("TASK", "Empty task list at %08x", tasks);
    while ((cur = task_get_next_task(tasks, cur, 0)))
        logln("TASK", "Task %d with %dKB kernel and %dKB user stack",
                cur->pid, cur->kernel_stack_len, cur->user_stack_len);
}