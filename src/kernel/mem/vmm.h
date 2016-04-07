#ifndef MEM_VMM_H
#define MEM_VMM_H

#include <stdint.h>
#include <mem/pmm.h>

#define VMM_PAGEDIR ((page_directory_t*) 0xFFFFF000) // see vmm_init
#define VMM_PAGETAB(i) ((page_table_t*) (0xFFC00000 + i * PAGE_SIZE))

typedef enum {
    // whether we are working with kernel or user memory, this controls
    // permissions and in which domain memory is stored
    VMM_KERNEL = 0b0, VMM_USER = 0b1, VMM_WRITABLE = 0b100
} vmm_flags_t;

typedef struct {
    uint8_t  pr    :  1; // whether this page table is present in virtual memory
    uint8_t  rw    :  1; // whether these pages should be writable
    uint8_t  user  :  1; // whether user space may access these pages
    uint8_t  wt    :  1; // enables write-through (1) or write-back (0))
    uint8_t  cache :  1; // enables (0) or disables (1) caching for these pages
    uint8_t  ac    :  1; // whether some of these pages have been accessed
    uint8_t        :  1; // reserved
    uint8_t  sz    :  1; // page size (0=4KiB, 1=4MiB)
    uint8_t        :  4; // ignored / available
    uint32_t pt    : 20; // where this page table is located (4KiB aligned!)
} __attribute__((packed)) page_directory_entry_t;

typedef page_directory_entry_t page_directory_t;

typedef struct {
    uint8_t  pr    :  1; // see above for flag meanings
    uint8_t  rw    :  1;
    uint8_t  user  :  1;
    uint8_t  wt    :  1;
    uint8_t  cache :  1;
    uint8_t  ac    :  1;
    uint8_t  dirty :  1; // whether this page has been written to
    uint8_t        :  1; // reserved
    uint8_t  gl    :  1; // marks this page as global (the TLB will not flush it)
    uint8_t        :  3;
    uint32_t page  : 20; // where this page is located (4KiB aligned!)
} __attribute__((packed)) page_table_entry_t;

typedef page_table_entry_t page_table_t;

// In the following, page_directory might be VMM_PAGEDIR or a physical address.
// In general, given addresses should be page aligned (4 KiB)!
page_directory_t* vmm_create_page_directory();
void vmm_destroy_page_directory(page_directory_t* dir_phys);
page_directory_t* vmm_load_page_directory(page_directory_t* new_directory);
void vmm_modify_page_directory(page_directory_t* new_directory);
void vmm_modified_page_directory();
void* vmm_get_physical_address(void* _vaddr);
void vmm_dump();
void vmm_enable_domain_check(uint8_t enable);
void vmm_init();

// maps the given page frame into the page directory
uint8_t vmm_map(void* _vaddr, void* paddr, vmm_flags_t flags);

// unmaps the given page frame from the page directory
void vmm_unmap(void* _vaddr);

// maps the given page frame(s) into the page directory
void vmm_map_range(void* vaddr, void* paddr, size_t len, vmm_flags_t flags);

// unmaps the given page frame(s) from the page directory
void vmm_unmap_range(void* vaddr, size_t len);

// if necessary, maps the given page frame(s) somewhere into memory
void* vmm_map_physical_memory(void* paddr, size_t len, vmm_flags_t flags);

// if necessary, unmaps the given page frame(s)
void vmm_unmap_physical_memory(void* vaddr, size_t len);

// marks the given page frame(s) as used and maps them into the given memory
void vmm_use(void* vaddr, void* paddr, size_t len, vmm_flags_t flags);

// marks the given page frame(s) as used and maps them somewhere into memory
void* vmm_use_physical_memory(void* paddr, size_t len, vmm_flags_t flags);

// marks some page frame(s) as used and maps them into the given memory
void* vmm_use_virtual_memory(void* vaddr, size_t len, vmm_flags_t flags);

// marks some page frame(s) as used and maps them somewhere into memory
void* vmm_alloc(size_t len, vmm_flags_t flags);

// frees the given page frame(s) and unmaps them from the page directory
void vmm_free(void* ptr, size_t len);

#endif
