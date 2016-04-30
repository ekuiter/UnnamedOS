/**
 * @file
 * @addtogroup schedule
 * @{
 * Scheduler
 * 
 * The scheduler switches tasks using a simple round robin strategy.
 * @see http://wiki.osdev.org/Scheduling_Algorithms
 */

#include <common.h>
#include <tasks/schedule.h>
#include <tasks/elf.h>
#include <tasks/tss.h>
#include <mem/mmu.h>
#include <mem/vmm.h>

static task_pid_t current_task = 0; ///< the currently running task pid
static uint32_t ticks_per_time_slice = 1; ///< 1 tick = frequency of the PIT

/**
 * Returns the next task to run.
 * @param cpu the current task's CPU state
 * @return the next task's CPU state
 */
cpu_state_t* schedule(cpu_state_t* cpu) {
    task_pid_t next_task = schedule_get_next_task();
    if (!next_task)
        return cpu; /// Does nothing if there are no tasks yet.
    if (current_task) {
        /// Does not switch tasks if the current task's time slice is not over yet.
        if (task_set_ticks(current_task, task_get_ticks(current_task) - 1) > 1)
            return cpu;
        task_set_cpu(current_task, cpu); // save the current ESP / CPU state
    }
    if (current_task == next_task) // no switch is needed
        return cpu;
    /// Otherwise switches to the next task.
    return schedule_switch_task(next_task);
}

/**
 * Switches to a given task.
 * @param next_task the new task's PID
 * @return the new task's CPU state
 */
cpu_state_t* schedule_switch_task(task_pid_t next_task) {
    if (current_task)
        logln("SCHEDULE", "Task switch from task %d to task %d",
            current_task, next_task);
    else
        logln("SCHEDULE", "Initial task switch to task %d", next_task);
    /// If we want to switch to userspace, tells the TSS which kernel stack to
    /// load when an interrupt occurs.
    // When the iret is done, the CPU state on the kernel stack is fully popped,
    // so when doing the iret, ESP will be next_task->cpu + sizeof(cpu_state_t)
    // or next_task->cpu + 1. We will use that as ESP when we are interrupted
    // the next time. Note that this has no effect when we stay in the kernel.
    tss_set_stack((uint32_t) (task_get_cpu(next_task) + 1));
    /// Refills the new task's time slice.
    task_set_ticks(next_task, ticks_per_time_slice);
    /// Switches to the new virtual address space.
    io_set_logging(0);
    vmm_load_page_directory(task_get_page_directory(next_task));
    io_set_logging(1);
    current_task = next_task;
    return task_get_cpu(next_task); // restore the CPU state / ESP saved earlier
}

/**
 * Returns the current task's PID.
 * @return the current task's PID
 */
task_pid_t schedule_get_current_task() {
    return current_task;
}

/**
 * Returns the next running task's PID.
 * @return the next running task's PID
 */
task_pid_t schedule_get_next_task() {
    return task_get_next_task_with_state(current_task, TASK_RUNNING);
}

/// Destroys tasks marked for removal.
void schedule_finalize_tasks() {
    task_pid_t pid = 0;
    while ((pid = task_get_next_task_with_state(pid, TASK_STOPPED)))
        task_get_elf(pid) ? elf_destroy_task(pid) : task_destroy(pid);
}

/// @}
