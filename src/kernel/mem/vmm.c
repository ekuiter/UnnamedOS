/*
 * Virtual Memory Manager - manages paging and memory contexts
 * 
 * http://wiki.osdev.org/Paging
 * http://www.lowlevel.eu/wiki/Paging
 * http://www.rohitab.com/discuss/topic/31139-tutorial-paging-memory-mapping-with-a-recursive-page-directory/
 */

#include <common.h>
#include <string.h>
#include <mem/vmm.h>
#include <mem/mmu.h>
#include <interrupts/isr.h>
#include <boot/multiboot.h>

#define ENTRIES 1024 // number of entries in page directories and tables
// The number of bytes per page directory / table "happens" to equal the size
#define PAGE_SIZE (ENTRIES * sizeof(page_directory_entry_t)) // of a page.
#define MEMORY_SIZE 0x100000000 // 4GB address space
#define PAGE_NUMBER (MEMORY_SIZE / PAGE_SIZE) // total number of pages

// we use two domains (kernel and user space)
typedef struct {
    void* start;
    void* end;
} vmm_domain_t;

typedef union {
    struct {
        uint16_t page_offset : 12; // location in the page
        uint16_t page        : 10; // index in the page table
        uint16_t page_table  : 10; // index in the page directory
    } __attribute__((packed)) bits;
    void* ptr;
} vmm_virtual_address_t;

static page_directory_t* page_directory = 0; // the current page directory
static page_directory_t* old_directory = 0; // for temporary modifications
static uint8_t old_interrupts = 0;
// We use 0-1GiB as kernel memory. This will be mapped into all processes.
// We exclude the first page table so we can use it freely for VM86.
static vmm_domain_t kernel_domain = {.start = (void*) 0x400000,
    .end = (void*) 0x3FFFFFFF};
// This memory is process-specific. Process images are loaded to 1GiB.
// The last page table is excluded (it contains the page directory and tables).
static vmm_domain_t user_domain = {.start = (void*) 0x40000000,
    .end = (void*) 0xFFFFFFFF - ENTRIES * PAGE_SIZE};
static uint8_t domain_check_enabled = 0;

static void vmm_destroy_page_table(uint16_t page_table) {
    // free the page table memory (using its physical address!)
    pmm_free(pmm_get_address(page_directory[page_table].pt, 0), PAGE_SIZE);
    // tell the page directory that this page table was deleted
    memset(page_directory + page_table, 0, sizeof(page_directory_entry_t));
}

page_directory_t* vmm_create_page_directory() {   
    page_directory_t* dir_phys = pmm_alloc(PAGE_SIZE, PMM_KERNEL);
    logln("VMM", "Creating page directory at %08x", dir_phys);
    page_directory_t* dir = vmm_map_physical_memory(dir_phys, PAGE_SIZE, VMM_KERNEL);
    memset(dir, 0, PAGE_SIZE);
    // for a detailed explanation on why we do this, see vmm_init
    page_directory_entry_t dir_entry = {
        .pr = 1, .rw = 0, .user = 0, .pt = pmm_get_page(dir_phys, 0)
    };
    dir[ENTRIES - 1] = dir_entry;
    vmm_unmap_physical_memory(dir, PAGE_SIZE);
    return dir_phys; // this is a physical address for a page directory
}

void vmm_destroy_page_directory(page_directory_t* dir_phys) {
    logln("VMM", "Destroying page directory at %08x", dir_phys);
    page_directory_t* old_directory = page_directory;
    page_directory_t* dir = vmm_map_physical_memory(dir_phys, PAGE_SIZE, VMM_KERNEL);
    page_directory = dir; // operate on the directory we want to destroy
    vmm_virtual_address_t start = (vmm_virtual_address_t) user_domain.start,
            end = (vmm_virtual_address_t) user_domain.end;
    // delete any unique page tables associated with this directory
    vmm_destroy_page_table(0); // first VM86, then user domain page tables
    for (int i = start.bits.page_table; i <= end.bits.page_table; i++)
        if (page_directory[i].pr)
            vmm_destroy_page_table(i);
    vmm_destroy_page_table(ENTRIES - 1); // free the directory (last page table)
    page_directory = old_directory; // change back so we can unmap the
    vmm_unmap_physical_memory(dir, PAGE_SIZE); // destroyed directory
}

static void vmm_refresh_page_directory(page_directory_t* dir_phys) {
    page_directory_t* dir = vmm_map_physical_memory(dir_phys, PAGE_SIZE, VMM_KERNEL);
    vmm_virtual_address_t start = (vmm_virtual_address_t) kernel_domain.start,
            end = (vmm_virtual_address_t) kernel_domain.end;
    uint32_t offset = start.bits.page_table * sizeof(page_directory_entry_t);
    memcpy((void*) ((uintptr_t) dir + offset),
            (void*) ((uintptr_t) page_directory + offset),
            (end.bits.page_table - start.bits.page_table + 1) *
            sizeof(page_directory_entry_t));
    vmm_unmap_physical_memory(dir, PAGE_SIZE);
}

page_directory_t* vmm_load_page_directory(page_directory_t* new_directory) {
    if (new_directory != VMM_PAGEDIR) {
        logln("VMM", "Loading page directory at %08x", new_directory);
        page_directory_t* old_directory = vmm_get_physical_address(page_directory);
        if (mmu_get_paging()) {
            vmm_refresh_page_directory(new_directory);
            mmu_load_page_directory(new_directory);
        } else
            mmu_enable_paging(new_directory);
        page_directory = VMM_PAGEDIR;
        return old_directory;
    }
    return 0;
}

void vmm_modify_page_directory(page_directory_t* new_directory) {
    if (old_directory) {
        println("VMM: Already modifying a page directory at %08x", old_directory);
        return;
    }
    // we don't want to be interrupted when modifying kernel-relevant
    old_interrupts = isr_enable_interrupts(0); // page directories
    old_directory = vmm_load_page_directory(new_directory);
}

void vmm_modified_page_directory() {
    if (!old_directory) {
        println("VMM: Not yet modifying a page directory");
        return;
    }
    vmm_load_page_directory(old_directory);
    old_directory = 0;
    isr_enable_interrupts(old_interrupts);
}

static page_table_entry_t* vmm_get_page_table(
    page_directory_entry_t* dir_entry, vmm_virtual_address_t vaddr) {
    // If paging is already enabled and we access the virtually mapped page
    // directory, we need to calculate the page table pointer in a different way
    if (page_directory == VMM_PAGEDIR) // (see vmm_init for details).
        return VMM_PAGETAB(vaddr.bits.page_table);
    else
        return (page_table_t*) pmm_get_address(dir_entry->pt, 0);
}

static page_table_entry_t* vmm_get_page_table_entry(
    page_directory_entry_t* dir_entry, vmm_virtual_address_t vaddr) {
    return vmm_get_page_table(dir_entry, vaddr) + vaddr.bits.page;
}

static vmm_domain_t* vmm_get_domain(vmm_flags_t flags) {
    return flags & VMM_USER ? &user_domain : &kernel_domain;
}

static uint8_t vmm_is_in_domain(void* vaddr, vmm_domain_t* domain) {
    return (uintptr_t) vaddr >= (uintptr_t) domain->start &&
           (uintptr_t) vaddr <= (uintptr_t) domain->end;
}

static vmm_domain_t* vmm_get_domain_from_address(void* vaddr) {
    return vmm_is_in_domain(vaddr, &kernel_domain) ? &kernel_domain :
          (vmm_is_in_domain(vaddr, &user_domain)   ? &user_domain   : 0);
}

static uint8_t vmm_domain_check(void* vaddr, vmm_flags_t flags) {
    if (domain_check_enabled &&
        vmm_get_domain_from_address(vaddr) != vmm_get_domain(flags)) {
        println("%4aVMM: Domain mismatch%a");
        return 0;
    }
    return 1;
}

uint8_t vmm_map(void* _vaddr, void* paddr, vmm_flags_t flags) {
    if (!vmm_domain_check(_vaddr, flags)) return 0;
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    // find the page directory entry associated with this page
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) { // if the table doesn't exist yet, create it
        // We assume the table's pages to be writable and in userspace for now,
        // this is overridden by individual pages below.
        dir_entry->pr = dir_entry->rw = dir_entry->user = 1;
        dir_entry->pt = pmm_get_page(pmm_alloc(PAGE_SIZE, PMM_KERNEL), 0);
        page_table_t* tab = vmm_get_page_table(dir_entry, vaddr);
        memset(tab, 0, PAGE_SIZE); // initialize with zeroes
    }
    page_table_entry_t* tab_entry = vmm_get_page_table_entry(dir_entry, vaddr);
    if (tab_entry->pr) {
        println("%4aVMM: %08x is already mapped%a", vaddr);
        return 0; // if we already mapped this page, cancel and return
    }
    tab_entry->pr = 1;
    tab_entry->rw = !!(flags & VMM_WRITABLE);
    tab_entry->user = flags & VMM_USER;
    tab_entry->page = pmm_get_page(paddr, 0); // otherwise, map the page
    if (page_directory == VMM_PAGEDIR) // if we changed the currently used
        mmu_flush_tlb(_vaddr); // directory, flush the TLB to apply changes
    return 1;
}

void vmm_unmap(void* _vaddr) {
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) // if the table doesn't exist yet, do nothing
        return;
    page_table_t* page_table = vmm_get_page_table(dir_entry, vaddr);
    page_table_entry_t* tab_entry = page_table + vaddr.bits.page;
    if (!tab_entry->pr)
        return; // if the page was not yet mapped, do nothing
    memset(tab_entry, 0, sizeof(page_table_entry_t)); // remove the mapping
    int i;
    for (i = 0; i < ENTRIES; i++) // search the page table
        if (page_table[i].pr) // for other present entries
            break;
    if (i == ENTRIES - 1) // if there are none, we can safely free the table
        vmm_destroy_page_table(vaddr.bits.page_table);
    if (page_directory == VMM_PAGEDIR)
        mmu_flush_tlb(_vaddr);
}

static void vmm_map_range_detailed(void* vaddr, void* paddr, size_t len,
        vmm_flags_t flags, uint8_t map) {
    if (map && !vmm_domain_check(vaddr, flags)) return;
    uint32_t virtual_page = pmm_get_page(vaddr, 0),
            physical_page = pmm_get_page(paddr, 0),
            pages         = pmm_get_page(vaddr, len - 1) - virtual_page + 1;
    logln("VMM", map ? "Map   virtual %08x-%08x (page %05x-%05x) "
            "to physical %08x-%08x (page %05x-%05x)" :
            "Unmap virtual %08x-%08x (page %05x-%05x)",
            vaddr, vaddr + len - 1, virtual_page,  virtual_page  + pages - 1,
            paddr, paddr + len - 1, physical_page, physical_page + pages - 1);
    if (map)
        for (int i = 0; i < pages; i++)
            vmm_map(pmm_get_address(virtual_page  + i, 0),
                    pmm_get_address(physical_page + i, 0), flags);
    else
        for (int i = 0; i < pages; i++)
            vmm_unmap(pmm_get_address(virtual_page + i, 0));
}

void vmm_map_range(void* vaddr, void* paddr, size_t len, vmm_flags_t flags) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, paddr, len, flags, 1);
}

void vmm_unmap_range(void* vaddr, size_t len) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, 0, len, 0, 0);
}

void* vmm_get_physical_address(void* _vaddr) {
    if (!mmu_get_paging())
        return _vaddr;
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr)
        return 0; // this virtual address is not currently mapped
    page_table_entry_t* tab_entry = vmm_get_page_table_entry(dir_entry, vaddr);
    if (!tab_entry->pr)
        return 0;
    return pmm_get_address(tab_entry->page, vaddr.bits.page_offset);
}

void vmm_dump() {
    log("VMM", "Page directory at %08x (physical %08x):",
            page_directory, vmm_get_physical_address(page_directory));
    uint32_t logged = 0;
    for (int i = 0; i < ENTRIES; i++) {
        page_directory_entry_t* dir_entry = page_directory + i;
        if (dir_entry->pr) {
            vmm_virtual_address_t vaddr = {.bits = {.page_table = i}};
            page_table_t* page_table = vmm_get_page_table(dir_entry, vaddr);
            for (int j = 0; j < ENTRIES; j++)
                if (page_table[j].pr) {
                    uint32_t vpage = i * ENTRIES + j, ppage = page_table[j].page;
                    if (logged % 8 == 0)
                        logln(0, ""), log("VMM", "");
                    log(0, vpage == ppage ? "%s%05x to itself" : "%s%05x to  %05x",
                            logged % 8 ? ", " : "", vpage, ppage);
                    logged++;
                }
        }
    }
    logln(0, "");
}

static void* vmm_find_free(size_t len, vmm_domain_t* domain) {
    if (len == 0) return 0;
    uint32_t pages = len / PAGE_SIZE + (len % PAGE_SIZE ? 1 : 0),
            free_pages = 0, end_page = pmm_get_page(domain->end, 0);
    // first-fit as in PMM
    for (int i = pmm_get_page(domain->start, 0); i <= end_page; i++) {
        if (vmm_get_physical_address(pmm_get_address(i, 0)) == 0)
            free_pages++;
        else
            free_pages = 0;
        if (free_pages >= pages)
            return pmm_get_address(i - free_pages + 1, 0);
    }
    println("%4aVMM: Not enough memory%a");
    return 0;
}

static pmm_flags_t vmm_get_pmm_flags(vmm_flags_t flags) {
    return flags & VMM_USER ? PMM_USER : PMM_KERNEL;
}

void* vmm_map_physical_memory(void* paddr, size_t len, vmm_flags_t flags) {
    if (!mmu_get_paging())
        return paddr;
    void* vaddr = vmm_find_free(len, vmm_get_domain(flags));
    if (!vaddr)
        return 0;
    vmm_map_range(vaddr, paddr, len, flags);
    return vaddr;
}

void vmm_unmap_physical_memory(void* vaddr, size_t len) {
    if (mmu_get_paging())
        vmm_unmap_range(vaddr, len);
}

void vmm_use(void* vaddr, void* paddr, size_t len, vmm_flags_t flags) {
    if (len == 0 || !vmm_domain_check(vaddr, flags)) return;
    pmm_use(paddr, len, vmm_get_pmm_flags(flags), "vmm_use");
    vmm_map_range(vaddr, paddr, len, flags);
}

void* vmm_use_physical_memory(void* paddr, size_t len, vmm_flags_t flags) {
    void* vaddr = vmm_find_free(len, vmm_get_domain(flags));
    if (!vaddr)
        return 0;
    vmm_use(vaddr, paddr, len, flags);
    return vaddr;
}

void* vmm_use_virtual_memory(void* vaddr, size_t len, vmm_flags_t flags) {
    if (!vmm_domain_check(vaddr, flags)) return 0;
    void* paddr = pmm_alloc(len, vmm_get_pmm_flags(flags));
    if (!paddr)
        return 0;
    vmm_map_range(vaddr, paddr, len, flags);
    return paddr;
}

void* vmm_alloc(size_t len, vmm_flags_t flags) {
    // Allocate some memory and find unmapped virtual space to map it into.
    // Note that this does not necessarily identity-map!
    void* paddr = pmm_alloc(len, vmm_get_pmm_flags(flags));
    void* vaddr = vmm_find_free(len, vmm_get_domain(flags));
    if (!paddr || !vaddr)
        return 0;
    vmm_map_range(vaddr, paddr, len, flags);
    return vaddr;
}

void vmm_free(void* vaddr, size_t len) {
    if (len == 0) return;
    void* paddr = vmm_get_physical_address(vaddr);
    vmm_unmap_range(vaddr, len);
    pmm_free(paddr, len);
}

void vmm_enable_domain_check(uint8_t enable) {
    domain_check_enabled = enable;
}

void vmm_init() {
    print("VMM init ... ");
    mmu_init();
    page_directory = vmm_create_page_directory();
    // Identity map all up to now used kernel pages. The PMM has memorized up
    // to which page we need to map at most to speed up the process.
    uint32_t last_kernel_page = pmm_get_last_kernel_page();
    for (int i = 0; i <= last_kernel_page; i++) {
        void* addr = pmm_get_address(i, 0);
        if (pmm_check(addr) != PMM_UNUSED && pmm_check(addr) != PMM_RESERVED)
            vmm_map(addr, addr, VMM_KERNEL); // only accessible to the kernel
    }
    vmm_enable_domain_check(1);
    // Above we let the last directory entry point to itself so we can use it
    // as a page table to be able to dynamically (un)map and virtual addresses.
    // This allows us to refer to the page directory (wherever it is in physical
    // memory) with the virtual address 0xFFFFF000 as soon as paging is enabled:
    // 0xFFFFF000 = 0b 1111111111 1111111111 000000000000
    //                 dir_idx    tab_idx    page_offset, so 0xFFFFF000 refers
    // to the last directory entry (which is the page directory itself) and then
    // the last table entry which is the directory again, in a recursive manner.
    // So 0xFFFFF000 points to the page directory and is one page (4096 or
    // 0x1000 bytes) long, so it lies at 0xFFFFF000-0xFFFFFFFF (at the very end).
    vmm_load_page_directory(page_directory);
    // Now we are working with virtual addresses, so to access our directory we
    // use 0xFFFFF000 as explained above. From now on all addresses are virtual
    // addresses, but this is transparent because we identity-mapped the kernel!
    // Note that in this way, we can access all page tables as well. Take, for
    // example, the first page table: Because we mapped the last directory entry
    // to the directory itself, the last 4MB of our address space (0xFFC00000-
    // 0xFFFFFFFF) represent the whole page directory. To access the first page
    // table, we just have to use the address 0xFFC00000 as explained above:
    // 0xFFC00000 = 0b 1111111111 0000000000 000000000000
    //                 dir_idx    tab_idx    page_offset, so 0xFFC00000 refers
    // to the last directory entry (the page directory) and then the first page
    // table inside the page directory which points to the physical location of
    // the page table. In other words, to access arbitrary parts of the page
    // directory after we enabled paging, we need to use following pointers:
    // 0xFFC00000 - the first page table (handles 0x00000000-0x003FFFFF)
    // 0xFFC01000 - the second page table (handles 0x00400000-0x007FFFFF)
    // ... (formula: 0xFFC00000 + tab_idx * PAGE_SIZE)
    // 0xFFFFE000 - the last "regular" page table (0xFF800000-0xFFBFFFFF)
    // 0xFFFFF000 - the page directory
    // In this way, we can map addresses as we like even with paging enabled.
    io_use_video_memory(); // necessary to keep print and friends working
    println("%2aok%a.");
}
