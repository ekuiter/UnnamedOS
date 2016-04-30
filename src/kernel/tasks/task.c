/**
 * @file
 * @addtogroup task
 * @{
 * Task
 * 
 * This controls multitasking. Each tasks has two associated stacks, a kernel
 * stack for interrupt handling and a user stack that serves as user call stack.
 * Also each task has its own page directory and therefore virtual address space.
 * @see http://www.lowlevel.eu/wiki/Teil_6_-_Multitasking
 */

#include <common.h>
#include <tasks/task.h>
#include <tasks/schedule.h>
#include <tasks/elf.h>
#include <interrupts/isr.h>
#include <mem/gdt.h>
#include <mem/mmu.h>
#include <mem/vmm.h>
#include <boot/multiboot.h>
#include <string.h>

#define MAX_TASKS 1024 ///< maximum number of tasks
/** Array of tasks. Fixed-size array for now, may change in the future. */
static task_t* tasks[MAX_TASKS];

/**
 * Returns the internal task structure associated with the given PID.
 * @param pid the task's PID
 * @return the task structure
 */
static task_t* task_get(task_pid_t pid) {
    if (!tasks[pid]) {
        println("%4aTask %d does not exist%a", pid);
        return 0;
    }
    return tasks[pid];
}

/**
 * Adds a new task to the task list and associates a PID.
 * @param task the task structure
 * @return the task's PID
 */
task_pid_t task_add(task_t* task) {
    task_pid_t pid;
    // pid 0 is an error value
    for (pid = 1; tasks[pid] && pid < MAX_TASKS; pid++);
    if (pid == MAX_TASKS)
        panic("Maximum task number reached");
    tasks[pid] = task;
    return pid;
}

/**
 * Removes a task from the task list.
 * @param pid the task's PID
 */
static void task_remove(task_pid_t pid) {
    tasks[pid] = 0;
}

/**
 * Creates a task.
 * @param entry_point      the virtual address where to start execution
 * @param page_directory   a page directory for the task, if 0 a new one is created
 * @param kernel_stack_len number of bytes to allocate for the kernel stack
 * @param user_stack_len   number of bytes to allocate for the user stack
 * @param elf              an ELF file, if 0 this is not an ELF task
 * @param code_segment     a code segment in the GDT
 * @param data_segment     a data segment in the GDT
 * @return the task's PID
 */
static task_pid_t task_create_detailed(void* entry_point,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, elf_t* elf, size_t code_segment, size_t data_segment) {
    uint8_t old_interrupts = isr_enable_interrupts(0);
    logln("TASK", "Creating task with %dKB kernel and %dKB user stack",
            kernel_stack_len, user_stack_len);
    // Here we allocate a whole page (4KB) which is more than we need.
    // TODO: use a proper heap (malloc)
    task_t* task = vmm_alloc(sizeof(task_t), VMM_KERNEL);
    task->page_directory = page_directory ? page_directory :
        vmm_create_page_directory();
    vmm_modify_page_directory(task->page_directory);
    task->state = TASK_RUNNING;
    task->vm86 = 0;
    task->elf = elf;
    task->kernel_stack = vmm_alloc(kernel_stack_len, VMM_KERNEL);
    task->user_stack   = vmm_alloc(user_stack_len, VMM_USER | VMM_WRITABLE);
    task->kernel_stack_len = kernel_stack_len;
    task->user_stack_len   = user_stack_len;
    /// Prepares a CPU state to pop off when a timer interrupt occurs.
    /// The CPU state lies at the top of the task's kernel stack.
    cpu_state_t* cpu = task->cpu = (cpu_state_t*)
            (task->kernel_stack + kernel_stack_len - 1 - sizeof(cpu_state_t));
    /// Makes an initial CPU state, setting the registers popped off in isr_asm.S.
    cpu->gs = cpu->fs = cpu->es = cpu->ds = gdt_get_selector(data_segment);
    // We don't need to set ESP because it is discarded by popa. Instead it is
    // set to the pointer value of cpu itself, so the TOS of the kernel stack.
    cpu->r.edi = cpu->r.esi = cpu->r.ebp = cpu->r.ebx =
            cpu->r.edx = cpu->r.ecx = cpu->r.eax = 0;
    // we also ignore intr and error, those are always set when entering the kernel
    cpu->eip = (uintptr_t) entry_point;
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
    task_pid_t pid = task_add(task); /// Tells the scheduler to run this task.
    isr_enable_interrupts(old_interrupts);
    return pid;
}

/**
 * Creates a kernel task.
 * @param entry_point      the virtual address where to start execution
 * @param page_directory   a page directory for the task, if 0 a new one is created
 * @param kernel_stack_len number of bytes to allocate for the kernel stack
 * @return the task's PID
 */
task_pid_t task_create_kernel(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            0, 0, GDT_RING0_CODE_SEG, GDT_RING0_DATA_SEG);
}

/**
 * Creates a user task.
 * @param entry_point      the virtual address where to start execution
 * @param page_directory   a page directory for the task, if 0 a new one is created
 * @param kernel_stack_len number of bytes to allocate for the kernel stack
 * @param user_stack_len   number of bytes to allocate for the user stack
 * @param elf              an ELF file, if 0 this is not an ELF task
 * @return the task's PID
 */
task_pid_t task_create_user(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len, size_t user_stack_len, void* elf) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            user_stack_len, elf, GDT_RING3_CODE_SEG, GDT_RING3_DATA_SEG);
}

/**
 * Stops a task. This does not remove the task from the task list.
 * @param pid the task's PID
 */
void task_stop(task_pid_t pid) {
    task_get(pid)->state = TASK_STOPPED;
}

/**
 * Destroys a task. This requires the task to have been stopped before.
 * @param pid the task's PID
 */
void task_destroy(task_pid_t pid) {
    uint8_t old_interrupts = isr_enable_interrupts(0);
    if (task_get(pid)->state == TASK_RUNNING) {
        println("%4aYou may not destroy a running task%a");
        return;
    }
    logln("TASK", "Destroying task %d", pid);
    task_t* task = task_get(pid);
    vmm_modify_page_directory(task->page_directory);
    vmm_free(task->kernel_stack, task->kernel_stack_len);
    vmm_free(task->user_stack, task->user_stack_len);
    vmm_modified_page_directory();
    vmm_destroy_page_directory(task->page_directory);
    vmm_free(task, sizeof(task_t));
    task_remove(pid);
    isr_enable_interrupts(old_interrupts);
}

/**
 * Returns the next task from the task list.
 * @param pid the current task's PID
 * @return the next task's PID
 */
task_pid_t task_get_next_task(task_pid_t pid) {
    for (pid++; pid < MAX_TASKS && !tasks[pid]; pid++);
    if (pid == MAX_TASKS) {
        for (pid = 1; pid < MAX_TASKS && !tasks[pid]; pid++);
        if (pid == MAX_TASKS)
            return 0;
    }
    return pid;
}

/**
 * Returns the next task from the task list with a specified state.
 * @param pid   the current task's PID
 * @param state the next task's desired state
 * @return the next task's PID
 */
task_pid_t task_get_next_task_with_state(task_pid_t pid, task_state_t state) {
    uint32_t pids = 0;
    do {
        pid = task_get_next_task(pid);
        if (!pid)
            return 0;
        pids++;
    } while (pids <= MAX_TASKS && task_get(pid)->state != state);
    if (pids > MAX_TASKS)
        return 0;
    return pid;
}

/**
 * Returns a task's number of remaining ticks.
 * @param pid the task's PID
 * @return the task's number of remaining ticks
 */
task_state_t task_get_ticks(task_pid_t pid) {
    return task_get(pid)->ticks;
}

/**
 * Sets a task's number of remaining ticks.
 * @param pid   the task's PID
 * @param ticks the number of remaining ticks
 * @return the old number of remaining ticks
 */
uint32_t task_set_ticks(task_pid_t pid, uint32_t ticks) {
    task_t* task = task_get(pid);
    uint32_t old_ticks = task->ticks;
    task->ticks = ticks;
    return old_ticks;
}

/**
 * Returns a task's CPU state.
 * @param pid the task's PID
 * @return the task's CPU state
 */
cpu_state_t* task_get_cpu(task_pid_t pid) {
    return task_get(pid)->cpu;
}

/**
 * Sets a task's CPU state.
 * @param pid the task's PID
 * @param cpu the task's CPU state
 */
void task_set_cpu(task_pid_t pid, cpu_state_t* cpu) {
    task_get(pid)->cpu = cpu;
}

/**
 * Returns a task's page directory.
 * @param pid the task's PID
 * @return the task's page directory
 */
page_directory_t* task_get_page_directory(task_pid_t pid) {
    return task_get(pid)->page_directory;
}

/**
 * Returns whether a task is a VM86 task
 * @param pid the task's PID
 * @return whether the task is a VM86 task
 */
uint8_t task_get_vm86(task_pid_t pid) {
    return task_get(pid)->vm86;
}

/**
 * Returns a task's ELF file.
 * @param pid the task's PID
 * @return the task's ELF file or 0
 */
void* task_get_elf(task_pid_t pid) {
    return task_get(pid)->elf;
}

/**
 * Dumps the task list. The dump is logged.
 */
void task_dump() {
    task_pid_t initial_pid = task_get_next_task(0), pid = initial_pid;
    logln("TASK", "Task list:");
    if (!initial_pid) {
        logln("TASK", "There are no tasks.");
        return;
    }
    do {
        logln("TASK", "%s task with pid %d%s",
                task_get(pid)->state ? "Running" : "Stopped", pid,
                task_get(pid)->vm86 ? " (VM86)" : "");
    } while ((pid = task_get_next_task(pid)) && pid != initial_pid);
}

/// @}
