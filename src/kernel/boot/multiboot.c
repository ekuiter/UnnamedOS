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

uint8_t multiboot_free_memory() {
    if (!mb_info || !mb_info->flags.mmap)
        return 0;
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
    return 1;
}

void multiboot_copy_memory() {
    size_t mb_info_length = sizeof(multiboot_info_t),
            mmap_length = mb_info->mmap_length,
            modules_length = mb_info->flags.mods ?
                mb_info->mods_count * sizeof(multiboot_module_t) : 0;
    // we allocate some space all at once to save (some) memory
    void* dst = pmm_alloc(mb_info_length + mmap_length + modules_length, PMM_KERNEL);
    // Now we copy some data we don't want to overwrite:
    mb_info = memcpy(dst, mb_info, mb_info_length);
    // the memory map (if we want to use it later)
    mb_info->mmap_addr = (uint32_t) memcpy((void*) ((uintptr_t) dst + mb_info_length),
            (void*) mb_info->mmap_addr, mmap_length);
    if (mb_info->flags.mods) { // if there are modules, mark them as used
        mb_info->mods_addr = memcpy((void*)
                ((uintptr_t) dst + mb_info_length + mmap_length),
            mb_info->mods_addr, modules_length);
        for (int i = 0; i < mb_info->mods_count; i++) {
            multiboot_module_t* module = mb_info->mods_addr + i;
            size_t module_length = module->mod_end - module->mod_start + 1;
            size_t string_length = strlen(module->string) + 1;
            dst = pmm_alloc(module_length + string_length, PMM_KERNEL);
            module->mod_start = memcpy(dst, module->mod_start, module_length);
            module->string = memcpy((void*) ((uintptr_t) dst + module_length),
                    module->string, string_length);
        }
    }
}
