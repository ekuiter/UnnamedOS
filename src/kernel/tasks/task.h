#ifndef TASKS_TASK_H
#define TASKS_TASK_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <mem/vmm.h>

typedef uint8_t task_stack_t;

typedef struct task {
    uint32_t pid; // process id
    task_stack_t* user_stack; // stack for the actual task's code
    task_stack_t* kernel_stack; // stack for handling interrupts
    size_t user_stack_len, kernel_stack_len;
    page_directory_t* page_directory; // this task's virtual memory map
    cpu_state_t* cpu; // saved CPU state when entering/leaving interrupts
    uintptr_t entry_point; // where to start execution (e.g. function pointer)
    uint32_t ticks; // how many ticks the task may run per time slice
    struct task* next_task; // pointer to the next task in the linked list
} task_t;

task_t* task_create_kernel(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len);
task_t* task_create_user(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len, size_t user_stack_len);
void task_destroy(task_t* task);
void task_add(task_t** tasks, task_t* task);
void task_remove(task_t** tasks, task_t* task);
task_t* task_get_next_task(task_t** tasks, task_t* task, uint8_t cyclic);
uint8_t task_contains_task(task_t** tasks, task_t* task);
void task_dump(task_t** tasks);

#endif
