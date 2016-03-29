#include <common.h>
#include <string.h>
#include <boot/multiboot.h>
#include <mem/pmm.h>

/*
 * Multiboot - info passed by and to the bootloader
 *
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 * https://www.gnu.org/software/grub/manual/multiboot/html_node/multiboot.h.html
*/

static multiboot_info_t* mb_info;

void multiboot_init(multiboot_info_t* _mb_info, uint32_t mb_magic) {
    print("Multiboot init ... ");
    mb_info = _mb_info;
    if (mb_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        println("%4afail%a. Multiboot magic not found.");
        return;
    }
    if (mb_info->flags.mem)
        print("%dKB lower and %dKB upper memory, ",
                mb_info->mem_lower, mb_info->mem_upper);
    if (mb_info->flags.boot_device)
        print("booted from %s disk %d, ",
                mb_info->boot_device.drive.hard_disk ? "hard" : "floppy",
                mb_info->boot_device.drive.number);
    if (mb_info->flags.cmdline)
        print("boot options %s, ", mb_info->cmdline);
    if (mb_info->flags.mods)
        print("%d modules, ", mb_info->mods_count);
    if (mb_info->flags.elf_section_header_table)
        print("%d ELF sections, ", mb_info->elf_sec.num); // TODO
    if (mb_info->flags.mmap)
        print("memory map, "); // TODO
    if (mb_info->flags.drives)
        print("%d drives, ", mb_info->drives_length); // TODO
    if (mb_info->flags.config_table)
        print("config table, "); // TODO
    if (mb_info->flags.boot_loader_name)
        print("booted by %s, ", mb_info->boot_loader_name); 
    if (mb_info->flags.apm_table)
        print("APM table, "); // TODO
    if (mb_info->flags.vbe)
        print("VBE, "); // TODO
    println("%2aok%a.");
}

// returns the start address of the module with the specified string
void* multiboot_get_module(char* str) {
    if (!mb_info || !mb_info->flags.mods)
        return 0;
    for (int i = 0; i < mb_info->mods_count; i++) {
        multiboot_module_t* module = mb_info->mods_addr + i;
        if (strcmp(str, module->string) == 0)
            return module->mod_start;
    }
    return 0; // no module found
}

static void multiboot_free_memory() {
    uintptr_t mmap = mb_info->mmap_addr, // get the mmap start and end addresses
            mmap_end = (uintptr_t) mb_info->mmap_addr + mb_info->mmap_length;
    for (multiboot_memory_map_t* mmap_entry; // iterate over every mmap entry
            mmap_entry = (multiboot_memory_map_t*) mmap, mmap < mmap_end;
            // because offset 0 is not size but base_addr_low, we add size + 4 bytes
            mmap += mmap_entry->size + sizeof(mmap_entry->size)) {
        // We ignore the upper bytes here because we assume the machine does not
        // have more than 4 GiB RAM. (Might be relevant if we wanted to use PAE.)
        pmm_use((void*) mmap_entry->base_addr_low, mmap_entry->length_low,
                mmap_entry->type == 1 ? PMM_UNUSED : PMM_RESERVED, "BIOS memory");
    }
}

uint8_t multiboot_init_memory() {
    if (!mb_info || !mb_info->flags.mmap)
        return 0;
    // Start off by assuming all memory is used. Now determine from the memory
    multiboot_free_memory(); // map which parts of the memory are usable.
    // Now we mark some data as used we don't want to overwrite:
    pmm_use(mb_info, sizeof(multiboot_info_t), PMM_KERNEL, "multiboot info");
    // the memory map (if we want to use it later)
    pmm_use((void*) mb_info->mmap_addr, mb_info->mmap_length,
            PMM_KERNEL, "multiboot memory map");
    if (mb_info->flags.mods) { // if there are modules, mark them as used
        pmm_use(mb_info->mods_addr, mb_info->mods_count * sizeof(multiboot_module_t),
                PMM_KERNEL, "multiboot modules");
        for (int i = 0; i < mb_info->mods_count; i++) {
            multiboot_module_t* module = mb_info->mods_addr + i;
            pmm_use(module->mod_start, module->mod_end - module->mod_start + 1,
                    PMM_KERNEL, "multiboot module");
            pmm_use(module->string, strlen(module->string) + 1,
                    PMM_KERNEL, "multiboot module string");
        }
    }
    return 1;
}
