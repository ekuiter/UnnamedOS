/**
 * @file
 * @addtogroup task
 * @{
 */

#ifndef TASKS_TASK_H
#define TASKS_TASK_H

#include <stdint.h>
#include <interrupts/isr.h>
#include <mem/vmm.h>

#define _4KB 0x1000 ///< 4KB are 4096 bytes, often used for stacks

typedef uint8_t task_stack_t; ///< stacks are measured in bytes
typedef uint32_t task_pid_t;  ///< unique process ID

/// state of a task
typedef enum {
    TASK_STOPPED, TASK_RUNNING
} task_state_t;

/// internal representation of a task
typedef struct {
    task_state_t state;          ///< whether the task is running or stopped
    page_directory_t* page_directory; ///< this task's virtual memory map
    task_stack_t* kernel_stack;  ///< stack for handling interrupts
    task_stack_t* user_stack;    ///< stack for the actual task's code
    size_t kernel_stack_len,     ///< kernel stack length
            user_stack_len;      ///< user stack length
    cpu_state_t* cpu; ///< saved CPU state when entering/leaving interrupts
    uint32_t ticks;   ///< how many ticks the task may run per time slice
    uint8_t vm86;     ///< whether this task is running in Virtual 8086 mode
    void* elf;        ///< if this is an ELF task, this points to the ELF file
} task_t;

task_pid_t task_add(task_t* task);
task_pid_t task_create_kernel(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len);
task_pid_t task_create_user(void* entry_point, page_directory_t* page_directory,
        size_t kernel_stack_len, size_t user_stack_len, void* elf);
void task_stop(task_pid_t pid);
void task_destroy(task_pid_t pid);
task_pid_t task_get_next_task(task_pid_t pid);
task_pid_t task_get_next_task_with_state(task_pid_t pid, task_state_t state);
task_state_t task_get_ticks(task_pid_t pid);
uint32_t task_set_ticks(task_pid_t pid, uint32_t ticks);
cpu_state_t* task_get_cpu(task_pid_t pid);
void task_set_cpu(task_pid_t pid, cpu_state_t* cpu);
page_directory_t* task_get_page_directory(task_pid_t pid);
uint8_t task_get_vm86(task_pid_t pid);
void* task_get_elf(task_pid_t pid);
void task_dump();

#endif

/// @}
