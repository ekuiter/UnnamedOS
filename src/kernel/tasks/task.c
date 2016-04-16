/*
 * Task - controls multitasking
 *
 * http://www.lowlevel.eu/wiki/Teil_6_-_Multitasking
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

// Right now we manage this as an array until we have a better way (e.g. vector).
#define MAX_TASKS 1024
static task_t* tasks[MAX_TASKS];

static task_t* task_get(task_pid_t pid) {
    if (!tasks[pid]) {
        println("%4aTask %d does not exist%a", pid);
        return 0;
    }
    return tasks[pid];
}

task_pid_t task_add(task_t* task) {
    task_pid_t pid;
    // pid 0 is an error value
    for (pid = 1; tasks[pid] && pid < MAX_TASKS; pid++);
    if (pid == MAX_TASKS)
        panic("Maximum task number reached");
    tasks[pid] = task;
    return pid;
}

static void task_remove(task_pid_t pid) {
    tasks[pid] = 0;
}

static task_pid_t task_create_detailed(void* entry_point,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, elf_t* elf, size_t code_segment, size_t data_segment) {
    uint8_t old_interrupts = isr_enable_interrupts(0);
    logln("TASK", "Creating task with %dKB kernel and %dKB user stack",
            kernel_stack_len, user_stack_len);
    // Here we allocate a whole page (4KB) which is more than we need.
    task_t* task = vmm_alloc(sizeof(task_t), VMM_KERNEL); // TODO: use a proper heap (malloc)
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
    task_pid_t pid = task_add(task); // tell the scheduler about this task
    isr_enable_interrupts(old_interrupts);
    return pid;
}

task_pid_t task_create_kernel(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            0, 0, GDT_RING0_CODE_SEG, GDT_RING0_DATA_SEG);
}

task_pid_t task_create_user(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len, size_t user_stack_len, void* elf) {
    return task_create_detailed(entry_point, page_directory, kernel_stack_len,
            user_stack_len, elf, GDT_RING3_CODE_SEG, GDT_RING3_DATA_SEG);
}

void task_stop(task_pid_t pid) {
    task_get(pid)->state = TASK_STOPPED;
}

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

task_pid_t task_get_next_task(task_pid_t pid) {
    for (pid++; pid < MAX_TASKS && !tasks[pid]; pid++);
    if (pid == MAX_TASKS) {
        for (pid = 1; pid < MAX_TASKS && !tasks[pid]; pid++);
        if (pid == MAX_TASKS)
            return 0;
    }
    return pid;
}

static task_pid_t task_get_next_task_with_state(task_pid_t pid, task_state_t state) {
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

task_pid_t task_get_next_running_task(task_pid_t pid) {
    return task_get_next_task_with_state(pid, TASK_RUNNING);
}

task_pid_t task_get_next_stopped_task(task_pid_t pid) {
    return task_get_next_task_with_state(pid, TASK_STOPPED);
}

task_state_t task_get_ticks(task_pid_t pid) {
    return task_get(pid)->ticks;
}

uint32_t task_set_ticks(task_pid_t pid, uint32_t ticks) {
    task_t* task = task_get(pid);
    uint32_t old_ticks = task->ticks;
    task->ticks = ticks;
    return old_ticks;
}

cpu_state_t* task_get_cpu(task_pid_t pid) {
    return task_get(pid)->cpu;
}

void task_set_cpu(task_pid_t pid, cpu_state_t* cpu) {
    task_get(pid)->cpu = cpu;
}

page_directory_t* task_get_page_directory(task_pid_t pid) {
    return task_get(pid)->page_directory;
}

uint8_t task_get_vm86(task_pid_t pid) {
    return task_get(pid)->vm86;
}

void* task_get_elf(task_pid_t pid) {
    return task_get(pid)->elf;
}

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
