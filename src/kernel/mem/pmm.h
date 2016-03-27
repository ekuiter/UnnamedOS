#ifndef MEM_PMM_H
#define MEM_PMM_H

#include <stdint.h>

typedef enum { // information about who uses a page, needs to fit in TYPE_BITS
    PMM_UNUSED, PMM_RESERVED, PMM_KERNEL, PMM_USER
} pmm_flags_t;

void pmm_init();
uint32_t pmm_get_page(void* ptr, uint32_t offset);
void* pmm_get_address(uint32_t page, uint32_t offset);
pmm_flags_t pmm_check(void* ptr);
void pmm_dump(void* ptr, size_t len);
uint32_t pmm_get_allocations();

// marks the given page frame(s) as used
void pmm_use(void* ptr, size_t len, pmm_flags_t flags, char* tag);

// allocates page frame(s)
void* pmm_alloc(size_t len, pmm_flags_t flags);

// frees the given page frame(s)
void pmm_free(void* ptr, size_t len);

#endif
