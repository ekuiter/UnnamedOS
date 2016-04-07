#ifndef BOOT_MULTIBOOT_H
#define BOOT_MULTIBOOT_H

#define MULTIBOOT_HEADER_MAGIC     0x1BADB002
// align modules and provide memory information
#define MULTIBOOT_HEADER_FLAGS     0x00000003
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define STACK_SIZE                 0x4000
#define MULTIBOOT_LOWER_MEMORY     0x100000
#define MULTIBOOT_FIRST_PAGE_TABLE 0x400000

#ifndef ASM

typedef struct {
    uint32_t magic, flags, checksum, header_addr, load_addr, load_end_addr,
        bss_end_addr, entry_addr;
} multiboot_header_t;

typedef struct { // the section header table for ELF
    uint32_t num, size, addr, shndx;
} multiboot_elf_section_header_t;

typedef struct {
    void* mod_start;
    void* mod_end;
    char* string;
    uint32_t reserved;
} multiboot_module_t;

// The memory map. Be careful that the offset 0 is base_addr_low but no size.
typedef struct {
    uint32_t size, base_addr_low, base_addr_high, length_low, length_high, type;
} multiboot_memory_map_t;

typedef struct {
    struct {
        uint32_t mem : 1, boot_device : 1, cmdline : 1, mods : 1,
        aout_symbol_table : 1, elf_section_header_table : 1, mmap : 1, drives : 1,
        config_table : 1, boot_loader_name : 1, apm_table : 1, vbe : 1;
    } flags;
    uint32_t mem_lower, mem_upper;
    struct {
        uint8_t part3, part2, part1;
        struct {
            uint8_t number : 7, hard_disk : 1;
        } drive;
    } boot_device;
    uint32_t cmdline, mods_count;
    multiboot_module_t* mods_addr;
    multiboot_elf_section_header_t elf_sec;
    uint32_t mmap_length, mmap_addr, drives_length, drives_addr, config_table,
        boot_loader_name, apm_table, vbe_control_info, vbe_mode_info,
        vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
} multiboot_info_t;

void multiboot_init(multiboot_info_t* _mb_info, uint32_t mb_magic);
void* multiboot_get_module(char* str);
uint8_t multiboot_free_memory();
void multiboot_copy_memory();

#endif

#endif