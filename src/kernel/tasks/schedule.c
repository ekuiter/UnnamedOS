/*
 * Scheduler - switches tasks using a simple round robin strategy
 * 
 * http://wiki.osdev.org/Scheduling_Algorithms
 */

#include <common.h>
#include <tasks/schedule.h>
#include <tasks/tss.h>
#include <hardware/io/keyboard.h>

static task_t* tasks = 0; // linked list of scheduled tasks, start with none
static task_t* current_task = 0; // points to the currently running task
static uint32_t ticks_per_time_slice = 1; // 1 tick = frequency of the PIT

cpu_state_t* schedule(cpu_state_t* cpu) {   
    task_t* next_task;
    // if we just started scheduling or if we reached the linked list's end,
    if (!current_task || !current_task->next_task)
        next_task = tasks; // start over with the first task
    else
        next_task = current_task->next_task; // or continue in the list
    
    if (!next_task)
        return cpu; // if there are no tasks, we are most likely still in main
        
    if (current_task) {
        // If the current task's time slice is not over yet, do not switch tasks.
        if (current_task->ticks-- > 1)
            return cpu;
        current_task->cpu = cpu; // save the current ESP and CPU state
    }
    
    if (keyboard_get_event().flags.shift) { // don't clutter the log
        logln("SCHEDULER", "Task switch from");
        isr_dump_cpu(cpu);
        logln("SCHEDULER", "to");
        isr_dump_cpu(next_task->cpu);
    }
    
    // If we want to switch to userspace, we need to tell the TSS which kernel
    // stack to load when an interrupt occurs. At the point we do the iret,
    // the CPU state on the kernel stack (next_task->cpu) is fully popped, so
    // when doing the iret, ESP will be next_task->cpu + sizeof(cpu_state_t)
    // or next_task->cpu + 1. We will use that as ESP when we are interrupted
    // the next time. Note that this has no effect when we stay in the kernel.
    tss_set_stack((uint32_t) (next_task->cpu + 1));
    next_task->ticks = ticks_per_time_slice; // refill the task's time slice
    current_task = next_task;
    return next_task->cpu; // restore the CPU state and ESP saved earlier
}

task_t* schedule_get_current_task() {
    return current_task;
}

void schedule_add_task(task_t* task) {
    // TODO: prevent duplicate entries
    task->next_task = tasks;
    tasks = task;
}

void schedule_remove_task(task_t* task) {
    if (!tasks) // if there are no tasks, we are done
        return;
    if (task == tasks) { // the task to remove is at the beginning
        tasks = tasks->next_task;
        return;
    }
    task_t* cur = tasks;
    while (cur->next_task && cur->next_task != task);
    if (!cur->next_task) // the task to remove is not in the list
        return;
    // otherwise, the task is pointed to by cur->next_task, so remove it:
    cur->next_task = cur->next_task->next_task;
}
