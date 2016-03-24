#ifndef TASKS_SCHEDULE_H
#define TASKS_SCHEDULE_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <tasks/task.h>

cpu_state_t* schedule(cpu_state_t* cpu);
task_t* schedule_get_current_task();
void schedule_add_task(task_t* task);
void schedule_remove_task(task_t* task);

#endif
