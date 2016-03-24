#ifndef MEM_PMM_H
#define MEM_PMM_H

#include <stdint.h>

typedef enum { // information about who uses a page, needs to fit in TYPE_BITS
    PMM_UNUSED, PMM_BIOS, PMM_KERNEL, PMM_USER
} pmm_page_type_t;

void pmm_init();
void pmm_use(void* ptr, size_t len, pmm_page_type_t type, char* tag);
void* pmm_alloc(size_t len);
void pmm_free(void* ptr, size_t len);
pmm_page_type_t pmm_check(void* ptr);
void pmm_dump(void* ptr, size_t len);


#endif
