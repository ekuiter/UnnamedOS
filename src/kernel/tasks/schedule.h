#ifndef TASKS_SCHEDULE_H
#define TASKS_SCHEDULE_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <tasks/task.h>

cpu_state_t* schedule(cpu_state_t* cpu);
cpu_state_t* schedule_switch_task(cpu_state_t* cpu, task_t* next_task);
task_t* schedule_get_current_task();
task_t* schedule_get_next_task();
uint8_t schedule_task_running(task_t* task);
void schedule_add_task(task_t* task);
void schedule_remove_task(task_t* task);
void schedule_finalize_task(task_t* task);
void schedule_finalize_tasks();
void schedule_dump();

#endif
