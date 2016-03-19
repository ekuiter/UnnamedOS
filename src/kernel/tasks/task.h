#ifndef TASKS_TASK_H
#define TASKS_TASK_H

#include <stdint.h>
#include <interrupts/isr.h>

typedef uint8_t task_stack_t;

typedef struct task {
    task_stack_t* user_stack; // stack for the actual task's code
    task_stack_t* kernel_stack; // stack for handling interrupts
    cpu_state_t* cpu; // saved CPU state when entering/leaving interrupts
    uintptr_t entry_point; // where to start execution (e.g. function pointer)
    uint32_t ticks; // how many ticks the task may run per time slice
    struct task* next_task; // pointer to the next task in the linked list
} task_t;

void task_create_detailed(task_t* task, uintptr_t entry_point, task_stack_t* user_stack,
        size_t user_stack_len, task_stack_t* kernel_stack, size_t kernel_stack_len,
        size_t code_segment, size_t data_segment);
void task_create(task_t* task, uintptr_t entry_point, task_stack_t* user_stack,
        size_t user_stack_len, task_stack_t* kernel_stack, size_t kernel_stack_len);

#endif
