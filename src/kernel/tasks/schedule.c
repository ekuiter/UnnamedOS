/*
 * Scheduler - switches tasks using a simple round robin strategy
 * 
 * http://wiki.osdev.org/Scheduling_Algorithms
 */

#include <common.h>
#include <tasks/schedule.h>

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
    
    fprintln(bochs_log, "Task switch from");
    isr_dump_cpu(cpu);
    fprintln(bochs_log, "to");
    isr_dump_cpu(next_task->cpu);
    
    next_task->ticks = ticks_per_time_slice;
    current_task = next_task;
    return next_task->cpu; // restore the CPU state and ESP saved earlier
}

void schedule_add_task(task_t* task) {
    task->next_task = tasks;
    tasks = task;
}