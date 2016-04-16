#ifndef TASKS_SCHEDULE_H
#define TASKS_SCHEDULE_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <tasks/task.h>

cpu_state_t* schedule(cpu_state_t* cpu);
cpu_state_t* schedule_switch_task(cpu_state_t* cpu, task_pid_t next_task);
task_pid_t schedule_get_current_task();
task_pid_t schedule_get_next_task();
void schedule_finalize_tasks();

#endif
