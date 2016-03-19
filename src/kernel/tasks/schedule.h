#ifndef TASKS_SCHEDULE_H
#define TASKS_SCHEDULE_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <tasks/task.h>

cpu_state_t* schedule(cpu_state_t* cpu);
void schedule_add_task(task_t* task);

#endif
