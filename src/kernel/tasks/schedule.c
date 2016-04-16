/*
 * Scheduler - switches tasks using a simple round robin strategy
 * 
 * http://wiki.osdev.org/Scheduling_Algorithms
 */

#include <common.h>
#include <tasks/schedule.h>
#include <tasks/elf.h>
#include <tasks/tss.h>
#include <mem/mmu.h>
#include <mem/vmm.h>

static task_pid_t current_task = 0; // the currently running task pid
static uint32_t ticks_per_time_slice = 1; // 1 tick = frequency of the PIT

cpu_state_t* schedule(cpu_state_t* cpu) {
    task_pid_t next_task = schedule_get_next_task();
    if (!next_task)
        return cpu; // if there are no tasks, we are most likely still in main
    if (current_task) {
        // If the current task's time slice is not over yet, do not switch tasks.
        if (task_set_ticks(current_task, task_get_ticks(current_task) - 1) > 1)
            return cpu;
        task_set_cpu(current_task, cpu); // save the current ESP and CPU state
    }
    if (current_task == next_task) // no switch is needed
        return cpu;
    return schedule_switch_task(cpu, next_task); // do a task and context switch
}

cpu_state_t* schedule_switch_task(cpu_state_t* cpu, task_pid_t next_task) {
    if (current_task)
        logln("SCHEDULE", "Task switch from task %d to task %d",
            current_task, next_task);
    else
        logln("SCHEDULE", "Initial task switch to task %d", next_task);
    // If we want to switch to userspace, we need to tell the TSS which kernel
    // stack to load when an interrupt occurs. At the point we do the iret,
    // the CPU state on the kernel stack (next_task->cpu) is fully popped, so
    // when doing the iret, ESP will be next_task->cpu + sizeof(cpu_state_t)
    // or next_task->cpu + 1. We will use that as ESP when we are interrupted
    // the next time. Note that this has no effect when we stay in the kernel.
    tss_set_stack((uint32_t) (task_get_cpu(next_task) + 1));
    task_set_ticks(next_task, ticks_per_time_slice); // refill the task's time slice
    io_set_logging(0);
    vmm_load_page_directory(task_get_page_directory(next_task));
    io_set_logging(1);
    current_task = next_task;
    return task_get_cpu(next_task); // restore the CPU state and ESP saved earlier
}

task_pid_t schedule_get_current_task() {
    return current_task;
}

task_pid_t schedule_get_next_task() {
    return task_get_next_running_task(current_task);
}

void schedule_finalize_tasks() {
    task_pid_t pid = 0;
    while ((pid = task_get_next_stopped_task(pid)))
        task_get_elf(pid) ? elf_destroy_task(pid) : task_destroy(pid);
}
