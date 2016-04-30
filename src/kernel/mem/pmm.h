/**
 * @file
 * @addtogroup pmm
 * @{
 */

#ifndef MEM_PMM_H
#define MEM_PMM_H

#include <stdint.h>

/// information on who uses a page frame (needs to fit in TYPE_BITS)
typedef enum {
    PMM_UNUSED, PMM_RESERVED, PMM_KERNEL, PMM_USER
} pmm_flags_t;

void pmm_init();
uint32_t pmm_get_page(void* ptr, uint32_t offset);
void* pmm_get_address(uint32_t page, uint32_t offset);
void pmm_use(void* ptr, size_t len, pmm_flags_t flags, char* tag);
void* pmm_alloc(size_t len, pmm_flags_t flags);
void pmm_free(void* ptr, size_t len);
pmm_flags_t pmm_check(void* ptr);
void pmm_dump(void* ptr, size_t len);
uint32_t pmm_get_highest_kernel_page();
#endif

/// @}
