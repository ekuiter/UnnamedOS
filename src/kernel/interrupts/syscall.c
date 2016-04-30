/**
 * @file
 * @addtogroup syscall
 * @{
 * Syscall Interface
 * 
 * This defines the syscall interface used in user-space to access kernel
 * functions. Syscalls may have up to 5 arguments and a return value.
 * They are called by firing a 0x30 interrupt with a syscall ID placed in EAX.
 * Parameters are placed in general purpose registers, the return value in EAX.
 */

#include <common.h>
#include <interrupts/syscall.h>
#include <interrupts/isr.h>
#include <tasks/schedule.h>
#include <syscall.h>

/**
 * Exits the current task.
 * @param return_value whether the task returned successfully (0) or not
 * @param ecx ignored
 * @param edx ignored
 * @param esi ignored
 * @param edi ignored
 * @param cpu the CPU state pointer so we can switch to the next task
 * @todo process the return value and return it to the calling task
 */
static void syscall_exit(uint32_t return_value, uint32_t ecx,
        uint32_t edx, uint32_t esi, uint32_t edi, cpu_state_t** cpu) {
    task_pid_t current_task = schedule_get_current_task();
    task_pid_t next_task = schedule_get_next_task();
    if (current_task == next_task) {
        println("%4aThe last task cannot exit%a");
    }
    /// We tell the scheduler to not switch to this task again, so we can
    /// properly free it later in schedule_finalize_tasks(). We can't do that
    /// here because we are operating on this task's kernel stack which we
    /// cannot free while it is still in use.
    task_stop(current_task);
    *cpu = schedule_switch_task(next_task);
}

/**
 * Returns the current task's PID.
 * @return current task's PID
 */
static uint32_t syscall_getpid() {
    return schedule_get_current_task();
}

/// Initializes the syscall interface.
void syscall_init() {
    isr_register_syscall(SYSCALL_EXIT,       syscall_exit);
    isr_register_syscall(SYSCALL_GETPID,     syscall_getpid);
    isr_register_syscall(SYSCALL_IO_PUTCHAR, io_putchar);
    isr_register_syscall(SYSCALL_IO_ATTR,    io_attr);
}

/// @}
