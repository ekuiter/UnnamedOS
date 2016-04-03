/*
 * Scheduler - switches tasks using a simple round robin strategy
 * 
 * http://wiki.osdev.org/Scheduling_Algorithms
 */

#include <common.h>
#include <tasks/schedule.h>
#include <tasks/tss.h>
#include <hardware/io/keyboard.h>
#include <mem/mmu.h>
#include <mem/vmm.h>

static task_t* scheduled_tasks = 0; // linked list of tasks, start with none
static task_t* remove_tasks = 0; // linked list of tasks to be removed
static task_t* current_task = 0; // points to the currently running task
static uint32_t ticks_per_time_slice = 1; // 1 tick = frequency of the PIT

cpu_state_t* schedule(cpu_state_t* cpu) {   
    task_t* next_task = schedule_get_next_task();
    if (!next_task)
        return cpu; // if there are no tasks, we are most likely still in main
    if (current_task) {
        // If the current task's time slice is not over yet, do not switch tasks.
        if (current_task->ticks-- > 1)
            return cpu;
        current_task->cpu = cpu; // save the current ESP and CPU state
    }
    if (current_task == next_task) // no switch is needed
        return cpu;
    return schedule_switch_task(cpu, next_task); // do a task and context switch
}

cpu_state_t* schedule_switch_task(cpu_state_t* cpu, task_t* next_task) {
    if (current_task)
        logln("SCHEDULER", "Task switch from task %d to task %d",
            current_task->pid, next_task->pid);
    else
        logln("SCHEDULER", "Initial task switch to task %d", next_task->pid);
    // If we want to switch to userspace, we need to tell the TSS which kernel
    // stack to load when an interrupt occurs. At the point we do the iret,
    // the CPU state on the kernel stack (next_task->cpu) is fully popped, so
    // when doing the iret, ESP will be next_task->cpu + sizeof(cpu_state_t)
    // or next_task->cpu + 1. We will use that as ESP when we are interrupted
    // the next time. Note that this has no effect when we stay in the kernel.
    tss_set_stack((uint32_t) (next_task->cpu + 1));
    next_task->ticks = ticks_per_time_slice; // refill the task's time slice
    io_set_logging(0);
    vmm_load_page_directory(next_task->page_directory);
    io_set_logging(1);
    current_task = next_task;
    return next_task->cpu; // restore the CPU state and ESP saved earlier
}

task_t* schedule_get_current_task() {
    return current_task;
}

task_t* schedule_get_next_task() {
    return task_get_next_task(&scheduled_tasks, current_task, 1);
}

uint8_t schedule_task_running(task_t* task) {
    return current_task == task || task_contains_task(&scheduled_tasks, task);
}

void schedule_add_task(task_t* task) {
    task_add(&scheduled_tasks, task);
}

void schedule_remove_task(task_t* task) {
    task_remove(&scheduled_tasks, task);
    task_add(&remove_tasks, task); // schedule this task for final removal
}

void schedule_finalize_task(task_t* task) {
    task_remove(&remove_tasks, task);
}

void schedule_finalize_tasks() {
    task_t* cur = 0;
    task_t* next = task_get_next_task(&remove_tasks, cur, 0);
    while ((cur = next)) {
        next = task_get_next_task(&remove_tasks, cur, 0);
        if (!schedule_task_running(cur))
            task_destroy(cur);
    }
}

void schedule_dump() {
    logln("SCHEDULER", "Scheduled and removed tasks:");
    task_dump(&scheduled_tasks);
    task_dump(&remove_tasks);
}