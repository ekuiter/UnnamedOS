#include <common.h>
#include <intr/multiboot.h>

/*
 * Multiboot - info passed by and to the bootloader
 *
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 * https://www.gnu.org/software/grub/manual/multiboot/html_node/multiboot.h.html
*/

void multiboot_init(multiboot_info_t* mb_info, uint32_t mb_magic) {
    print("Multiboot init ... ");
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
        print("%d modules, ", mb_info->mods_count); // TODO
    if (mb_info->flags.aout_symbol_table)
        print("%d a.out symbols, ", mb_info->u.aout_sym.tabsize);
    if (mb_info->flags.elf_section_header_table)
        print("%d ELF sections, ", mb_info->u.elf_sec.num); // TODO
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