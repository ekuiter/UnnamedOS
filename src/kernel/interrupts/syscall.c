/*
 * Syscall - defines the syscall interface used in user-space
 */

#include <common.h>
#include <interrupts/syscall.h>
#include <interrupts/isr.h>
#include <tasks/schedule.h>
#include <syscall.h>

static uint32_t syscall_exit(uint32_t return_value, uint32_t ecx,
        uint32_t edx, uint32_t esi, uint32_t edi, cpu_state_t** cpu) {
    task_t* current_task = schedule_get_current_task();
    task_t* next_task = schedule_get_next_task();
    if (current_task == next_task) {
        println("%4aThe last task cannot exit%a");
        return 0;
    }
    // For now we just tell the scheduler to not switch to this task again,
    // so we can properly free it later. We can't do that here because we are
    // operating on this task's kernel stack which we cannot free while it
    schedule_remove_task(current_task); // is still in use.
    *cpu = schedule_switch_task(*cpu, next_task);
    return 0;
}

static uint32_t syscall_getpid() {
    return schedule_get_current_task()->pid;
}

void syscall_init() {
    isr_register_syscall(SYSCALL_EXIT,       syscall_exit);
    isr_register_syscall(SYSCALL_GETPID,     syscall_getpid);
    isr_register_syscall(SYSCALL_IO_PUTCHAR, io_putchar);
    isr_register_syscall(SYSCALL_IO_ATTR,    io_attr);
}
