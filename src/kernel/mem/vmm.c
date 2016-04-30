/**
 * @file
 * @addtogroup vmm
 * @{
 * Virtual Memory Manager
 * 
 * The VMM manages paging and memory contexts. It maps virtual to physical
 * memory, allocates and frees virtual memory and switches page directories.
 * @see http://wiki.osdev.org/Paging
 * @see http://www.lowlevel.eu/wiki/Paging
 * @see http://www.rohitab.com/discuss/topic/31139-tutorial-paging-memory-mapping-with-a-recursive-page-directory/
 */

#include <common.h>
#include <string.h>
#include <mem/vmm.h>
#include <mem/mmu.h>
#include <interrupts/isr.h>
#include <boot/multiboot.h>

#define ENTRIES 1024 ///< number of entries in page directories and tables
/// The number of bytes per page directory "happens" to equal the size of a page.
#define PAGE_SIZE (ENTRIES * sizeof(page_directory_entry_t))
#define MEMORY_SIZE 0x100000000 ///< 4GB address space
#define PAGE_NUMBER (MEMORY_SIZE / PAGE_SIZE) ///< total number of pages

/** We use two domains, kernel and user memory. This is used for permissions and
 * to determine which parts of a page directory to link and which to clone. */
typedef struct {
    void* start; ///< start address of the domain
    void* end;   ///< end address of the domain
} vmm_domain_t;

/** A virtual address. It can be split up into indices into a page directory. */
typedef union {
    struct {
        uint16_t page_offset : 12;  ///< location in the page
        uint16_t page        : 10;  ///< index in the page table
        uint16_t page_table  : 10;  ///< index in the page directory
    } __attribute__((packed)) bits; ///< bit field
    void* ptr;                      ///< useful for casting
} vmm_virtual_address_t;

static page_directory_t* page_directory = 0; ///< the current page directory
static page_directory_t* old_directory = 0; ///< for temporary modifications
static uint8_t old_interrupts = 0; ///< for temporary modifications
/** We use 0-1GiB as kernel memory. This will be mapped into all processes.
 * We exclude the first page table so we can use it freely for VM86. */
static vmm_domain_t kernel_domain = {.start = (void*) 0x400000,
    .end = (void*) 0x3FFFFFFF};
/** The memory 1GiB-4GiB is process-specific. Process images are loaded to 1GiB.
 * The last page table is excluded (it contains the page directory and tables). */
static vmm_domain_t user_domain = {.start = (void*) 0x40000000,
    .end = (void*) 0xFFFFFFFF - ENTRIES * PAGE_SIZE};
static uint8_t domain_check_enabled = 0; ///< whether domain checking is performed

/**
 * Destroys a page table in the current page directory.
 * @param page_table the index of the page table to destroy
 */
static void vmm_destroy_page_table(uint16_t page_table) {
    /// Frees the page table memory using its physical address.
    pmm_free(pmm_get_address(page_directory[page_table].pt, 0), PAGE_SIZE);
    /// Tells the page directory that this page table was deleted.
    memset(page_directory + page_table, 0, sizeof(page_directory_entry_t));
}

/**
 * Creates an empty page directory. It itself is mapped into the last page table.
 * @return the physical address of the new page directory
 */
page_directory_t* vmm_create_page_directory() {   
    page_directory_t* dir_phys = pmm_alloc(PAGE_SIZE, PMM_KERNEL);
    logln("VMM", "Creating page directory at %08x", dir_phys);
    page_directory_t* dir = vmm_map_physical_memory(dir_phys, PAGE_SIZE, VMM_KERNEL);
    memset(dir, 0, PAGE_SIZE);
    /// Maps the last entry to itself, see vmm_init() for a detailed explanation.
    page_directory_entry_t dir_entry = {
        .pr = 1, .rw = 0, .user = 0, .pt = pmm_get_page(dir_phys, 0)
    };
    dir[ENTRIES - 1] = dir_entry;
    vmm_unmap_physical_memory(dir, PAGE_SIZE);
    return dir_phys;
}

/**
 * Destroys a page directory.
 * @param dir_phys the physical address of the page directory to be destroyed
 */
void vmm_destroy_page_directory(page_directory_t* dir_phys) {
    logln("VMM", "Destroying page directory at %08x", dir_phys);
    page_directory_t* old_directory = page_directory;
    page_directory_t* dir = vmm_map_physical_memory(dir_phys, PAGE_SIZE, VMM_KERNEL);
    page_directory = dir; /// Operates on the directory we want to destroy.
    vmm_virtual_address_t start = (vmm_virtual_address_t) user_domain.start,
            end = (vmm_virtual_address_t) user_domain.end;
    /// Deletes any unique page tables associated with this directory:
    vmm_destroy_page_table(0); /// First VM86, then user domain page tables.
    for (int i = start.bits.page_table; i <= end.bits.page_table; i++)
        if (page_directory[i].pr)
            vmm_destroy_page_table(i);
    vmm_destroy_page_table(ENTRIES - 1); /// Frees the page directory itself.
    page_directory = old_directory; // change back so we can unmap the
    vmm_unmap_physical_memory(dir, PAGE_SIZE); // destroyed directory
}

/**
 * Refreshes the page directory entries that are shared across page directories.
 * All memory inside the kernel domain will be shared.
 * @param dir_phys the physical address of the page directory to be refreshed
 */
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

/**
 * Loads a new page directory.
 * @param new_directory the physical address of the page directory to be loaded
 * @return the physical address of the previously loaded page directory
 */
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

/**
 * Loads a page directory for temporary modification. Disables interrupts until
 * vmm_modified_page_directory() is called.
 * @param new_directory the physical address of the page directory to be modified
 */
void vmm_modify_page_directory(page_directory_t* new_directory) {
    if (old_directory) {
        println("VMM: Already modifying a page directory at %08x", old_directory);
        return;
    }
    // we don't want to be interrupted when modifying kernel-relevant
    old_interrupts = isr_enable_interrupts(0); // page directories
    old_directory = vmm_load_page_directory(new_directory);
}

/**
 * Ends a page directory modification. The previous page directory is loaded
 * and interrupts are re-enabled (if they were enabled before).
 * @see vmm_modify_page_directory
 */
void vmm_modified_page_directory() {
    if (!old_directory) {
        println("VMM: Not yet modifying a page directory");
        return;
    }
    vmm_load_page_directory(old_directory);
    old_directory = 0;
    isr_enable_interrupts(old_interrupts);
}

/**
 * Returns a virtual or physical address to a page table in memory.
 * @param dir_entry the page directory entry for the requested page table
 * @param vaddr     a virtual address belonging to the requested page table
 * @return a usable pointer (virtual or physical) to the page table
 */
static page_table_entry_t* vmm_get_page_table(
    page_directory_entry_t* dir_entry, vmm_virtual_address_t vaddr) {
    // If paging is already enabled and we access the virtually mapped page
    // directory, we need to calculate the page table pointer in a different way
    if (page_directory == VMM_PAGEDIR) // (see vmm_init for details).
        return VMM_PAGETAB(vaddr.bits.page_table);
    else
        return (page_table_t*) pmm_get_address(dir_entry->pt, 0);
}

/**
 * Returns a virtual or physical address to a page table entry in memory.
 * @param dir_entry the page directory entry for the requested page table entry
 * @param vaddr     a virtual address belonging to the requested page table entry
 * @return a usable pointer (virtual or physical) to the page table entry
 */
static page_table_entry_t* vmm_get_page_table_entry(
    page_directory_entry_t* dir_entry, vmm_virtual_address_t vaddr) {
    return vmm_get_page_table(dir_entry, vaddr) + vaddr.bits.page;
}

/**
 * Extracts a domain from the given flags.
 * @param flags the flags containing a domain
 * @return the domain contained in the flags, either the kernel or user domain
 */
static vmm_domain_t* vmm_get_domain(vmm_flags_t flags) {
    return flags & VMM_USER ? &user_domain : &kernel_domain;
}

/**
 * Returns whether a virtual address belongs to a given domain.
 * @param vaddr  the virtual address
 * @param domain the domain
 * @return whether the virtual address belongs to the domain
 */
static uint8_t vmm_is_in_domain(void* vaddr, vmm_domain_t* domain) {
    return (uintptr_t) vaddr >= (uintptr_t) domain->start &&
           (uintptr_t) vaddr <= (uintptr_t) domain->end;
}

/**
 * Returns the domain a virtual address belongs to.
 * @param vaddr the virtual address
 * @return the domain the virtual address belongs to
 */
static vmm_domain_t* vmm_get_domain_from_address(void* vaddr) {
    return vmm_is_in_domain(vaddr, &kernel_domain) ? &kernel_domain :
          (vmm_is_in_domain(vaddr, &user_domain)   ? &user_domain   : 0);
}

/**
 * Checks whether a virtual address might be accessed with the given flags.
 * @param vaddr the virtual address
 * @param flags the flags
 * @return whether the virtual address belongs to the domain contained in the flags
 */
static uint8_t vmm_domain_check(void* vaddr, vmm_flags_t flags) {
    if (domain_check_enabled &&
        vmm_get_domain_from_address(vaddr) != vmm_get_domain(flags)) {
        println("%4aVMM: Domain mismatch%a");
        return 0;
    }
    return 1;
}

/**
 * Maps the given page into memory.
 * @param _vaddr a virtual address in the page to map from
 * @param paddr  a physical address in the page to map to
 * @param flags  the flags for mapping
 * @return whether mapping was successful
 */
uint8_t vmm_map(void* _vaddr, void* paddr, vmm_flags_t flags) {
    if (!vmm_domain_check(_vaddr, flags)) return 0;
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    /// Finds the page directory entry associated with this page.
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) { /// If the table doesn't exist yet, creates it.
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
        return 0; /// If we already mapped this page, cancels and returns 0.
    }
    tab_entry->pr = 1;
    tab_entry->rw = !!(flags & VMM_WRITABLE);
    tab_entry->user = flags & VMM_USER;
    tab_entry->page = pmm_get_page(paddr, 0); /// Otherwise, maps the page.
    /// if we changed the current directory, flushes the TLB to apply changes.
    if (page_directory == VMM_PAGEDIR)
        mmu_flush_tlb(_vaddr);
    return 1;
}

/**
 * Unmaps the given page from memory.
 * @see vmm_map
 * @param _vaddr a virtual address in the page to be unmapped
 */
void vmm_unmap(void* _vaddr) {
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) /// if the page table doesn't exist yet, does nothing.
        return;
    page_table_t* page_table = vmm_get_page_table(dir_entry, vaddr);
    page_table_entry_t* tab_entry = page_table + vaddr.bits.page;
    if (!tab_entry->pr)
        return; /// If the page was not yet mapped, does nothing.
    memset(tab_entry, 0, sizeof(page_table_entry_t)); /// Removes the mapping.
    int i;
    for (i = 0; i < ENTRIES; i++)
        if (page_table[i].pr) /// Searches the page table for other present entries.
            break;
    if (i == ENTRIES - 1) /// If there are none, the page table is freed.
        vmm_destroy_page_table(vaddr.bits.page_table);
    if (page_directory == VMM_PAGEDIR)
        mmu_flush_tlb(_vaddr);
}

/**
 * Maps or unmaps the given page(s) into memory.
 * @param vaddr a virtual address in the first page to map from
 * @param paddr a physical address in the first page to map to
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 * @param map   whether to map (1) or unmap (0) memory
 */
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

/**
 * Maps the given page(s) into memory.
 * @param vaddr a virtual address in the first page to map from
 * @param paddr a physical address in the first page to map to
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 */
void vmm_map_range(void* vaddr, void* paddr, size_t len, vmm_flags_t flags) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, paddr, len, flags, 1);
}

/**
 * Unmaps the given page(s) from memory.
 * @see vmm_map_range
 * @param vaddr a virtual address in the first page to be unmapped
 * @param len   the number of bytes to be unmapped
 */
void vmm_unmap_range(void* vaddr, size_t len) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, 0, len, 0, 0);
}

/**
 * Translates a virtual address into a physical address.
 * @param _vaddr the virtual address to translate
 * @return the physical address associated with the virtual address
 */
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

/**
 * Dumps the current page directory. The dump is logged.
 */
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

/**
 * Finds unmapped pages.
 * @param len    requested number of consecutive free bytes
 * @param domain requested domain
 * @return the virtual address of a suitable unmapped memory range
 */
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

/**
 * Translates VMM into PMM flags.
 * @param flags the VMM flags
 * @return the PMM flags
 */
static pmm_flags_t vmm_get_pmm_flags(vmm_flags_t flags) {
    return flags & VMM_USER ? PMM_USER : PMM_KERNEL;
}

/**
 * If necessary, maps the given page(s) somewhere into memory.
 * @param paddr a physical address in the first page to map to
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 * @return the virtual address of the newly mapped memory
 */
void* vmm_map_physical_memory(void* paddr, size_t len, vmm_flags_t flags) {
    if (!mmu_get_paging())
        return paddr;
    void* vaddr = vmm_find_free(len, vmm_get_domain(flags));
    if (!vaddr)
        return 0;
    vmm_map_range(vaddr, paddr, len, flags);
    return vaddr;
}

/**
 * If necessary, unmaps the given page(s) from memory.
 * @see vmm_map_physical_memory
 * @param vaddr a virtual address in the first page to be unmapped
 * @param len   the number of bytes to be unmapped
 */
void vmm_unmap_physical_memory(void* vaddr, size_t len) {
    if (mmu_get_paging())
        vmm_unmap_range(vaddr, len);
}

/**
 * Marks the given page(s) as used and maps them into memory.
 * @param vaddr a virtual address in the first page to map from
 * @param paddr a physical address in the first page to map to
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 */
void vmm_use(void* vaddr, void* paddr, size_t len, vmm_flags_t flags) {
    if (len == 0 || !vmm_domain_check(vaddr, flags)) return;
    pmm_use(paddr, len, vmm_get_pmm_flags(flags), "vmm_use");
    vmm_map_range(vaddr, paddr, len, flags);
}

/**
 * Marks the given page(s) as used and maps them somewhere into memory.
 * @param paddr a physical address in the first page to map to
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 * @return the virtual address of the newly mapped memory
 */
void* vmm_use_physical_memory(void* paddr, size_t len, vmm_flags_t flags) {
    void* vaddr = vmm_find_free(len, vmm_get_domain(flags));
    if (!vaddr)
        return 0;
    vmm_use(vaddr, paddr, len, flags);
    return vaddr;
}

/**
 * Marks some page(s) as used and maps them into memory.
 * @param vaddr a virtual address in the first page to map from
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 * @return the physical address of the newly mapped memory
 */
void* vmm_use_virtual_memory(void* vaddr, size_t len, vmm_flags_t flags) {
    if (!vmm_domain_check(vaddr, flags)) return 0;
    void* paddr = pmm_alloc(len, vmm_get_pmm_flags(flags));
    if (!paddr)
        return 0;
    vmm_map_range(vaddr, paddr, len, flags);
    return paddr;
}

/**
 * Marks some page(s) as used and maps them somewhere into memory.
 * @param len   the number of bytes to be mapped
 * @param flags the flags for mapping
 * @return the virtual address of the newly mapped memory
 */
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

/**
 * Frees the given page(s) and unmaps them from memory.
 * @see vmm_alloc
 * @param vaddr a virtual address in the first page to be unmapped
 * @param len   the number of bytes to be unmapped
 */
void vmm_free(void* vaddr, size_t len) {
    if (len == 0) return;
    void* paddr = vmm_get_physical_address(vaddr);
    vmm_unmap_range(vaddr, len);
    pmm_free(paddr, len);
}

/**
 * Enables or disables domain checking.
 * @param enable whether to enable or disable domain checking
 */
void vmm_enable_domain_check(uint8_t enable) {
    domain_check_enabled = enable;
}

/// Initializes the VMM.
void vmm_init() {
    print("VMM init ... ");
    mmu_init();
    page_directory = vmm_create_page_directory();
    /// Identity maps all up to now used kernel pages. The PMM has memorized up
    /// to which page we need to map at most to speed up the process.
    uint32_t highest_kernel_page = pmm_get_highest_kernel_page();
    for (int i = 0; i <= highest_kernel_page; i++) {
        void* addr = pmm_get_address(i, 0);
        if (pmm_check(addr) != PMM_UNUSED && pmm_check(addr) != PMM_RESERVED)
            vmm_map(addr, addr, VMM_KERNEL); // only accessible to the kernel
    }
    vmm_enable_domain_check(1);
    /// In vmm_create_page_directory() the last entry points to itself so we can use
    /// it as a page table to be able to dynamically (un)map and virtual addresses.
    /// This allows us to refer to the page directory (wherever it is in physical
    /// memory) with the virtual address 0xFFFFF000 as soon as paging is enabled:
    /// ```
    /// 0xFFFFF000 = 0b 1111111111 1111111111 000000000000
    ///                 dir_idx    tab_idx    page_offset
    /// ```
    /// So 0xFFFFF000 refers to the last PDE (which is the directory itself) and
    /// then the last PTE which is the directory again, in a recursive manner.
    /// So 0xFFFFF000 points to the page directory and is one page (4096 or
    /// 0x1000 bytes) long, so it lies at 0xFFFFF000-0xFFFFFFFF (at the very end).
    vmm_load_page_directory(page_directory); /// Loads the new page directory.
    /// Now we are working with virtual addresses, so to access our directory we
    /// use 0xFFFFF000 as explained above. From now on all addresses are virtual
    /// addresses, but this is transparent because we identity-mapped the kernel!
    /// Note that in this way, we can access all page tables as well. Take, for
    /// example, the first page table: Because we mapped the last directory entry
    /// to the directory itself, the last 4MB of our address space (0xFFC00000-
    /// 0xFFFFFFFF) represent the whole page directory. To access the first page
    /// table, we just have to use the address 0xFFC00000 as explained above:
    /// ```
    /// 0xFFC00000 = 0b 1111111111 0000000000 000000000000
    ///                 dir_idx    tab_idx    page_offset
    /// ```
    /// So 0xFFC00000 refers to the last PDE (the page directory) and then the
    /// first table inside the directory which points to the physical location of
    /// the page table. In other words, to access arbitrary parts of the page
    /// directory after we enabled paging, we need to use following pointers:
    /// - 0xFFC00000 - the first page table (handles 0x00000000-0x003FFFFF)
    /// - 0xFFC01000 - the second page table (handles 0x00400000-0x007FFFFF)
    /// - ... (formula: 0xFFC00000 + tab_idx * PAGE_SIZE), see VMM_PAGETAB()
    /// - 0xFFFFE000 - the last "regular" page table (0xFF800000-0xFFBFFFFF)
    /// - 0xFFFFF000 - the page directory, see VMM_PAGEDIR()
    ///
    /// In this way, we can map addresses as we like even with paging enabled.
    io_use_video_memory(); /// Maps video memory in order to keep print working.
    println("%2aok%a.");
}

/// @}
