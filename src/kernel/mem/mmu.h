/**
 * @file
 * @addtogroup mmu
 * @{
 */

#ifndef MEM_MMU_H
#define MEM_MMU_H

#include <stdint.h>
#include <mem/vmm.h>

void mmu_load_page_directory(page_directory_t* page_directory);
void mmu_enable_paging(page_directory_t* page_directory);
uint8_t mmu_get_paging();
void mmu_flush_tlb(void* vaddr);
void mmu_init();

#endif

/// @}
