/**
 * @file
 * @addtogroup vm86
 * @{
 */

#ifndef TASKS_VM86_H
#define TASKS_VM86_H

#include <stdint.h>
#include <tasks/task.h>

task_pid_t vm86_create_task(void* code_start, void* code_end,
        page_directory_t* page_directory, size_t kernel_stack_len,
        size_t user_stack_len, isr_registers_t* registers);
void vm86_call_bios(uint8_t interrupt, isr_registers_t* registers);
void vm86_init();

#endif

/// @}
