/*
 * Executable and Linking Format - execute external programs
 * 
 * https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
 * http://wiki.osdev.org/ELF
 * http://lowlevel.eu/wiki/ELF
 * http://www.linux-kernel.de/appendix/ap05.pdf
 */

#include <common.h>
#include <string.h>
#include <tasks/elf.h>

#define MAGIC_0 0x7F // magic values expected at the beginning
#define MAGIC_1 'E'  // of every ELF file
#define MAGIC_2 'L'
#define MAGIC_3 'F'
#define VERSION 1    // only ELF version 1 is supported (the current version)

enum class   { CLASS_32_BIT = 1, CLASS_64_BIT };
enum data    { DATA_LITTLE_ENDIAN = 1, DATA_BIG_ENDIAN };
enum type    { TYPE_RELOCATABLE = 1, TYPE_EXECUTABLE, TYPE_SHARED, TYPE_CORE };
enum machine { MACHINE_X86 = 3, /*...*/ }; // we are only interested in x86

typedef struct { // program header entries tell us how to load executables
    enum {       // (here for 32 bit)
        // we will only need PT_LOAD (load a segment into memory) for now
        PT_NULL, PT_LOAD, PT_DYNAMIC, PT_INTERP, PT_NOTE, PT_SHLIB, PT_PHDR
    } p_type;
    uint32_t p_offset; // where the segment starts in the file
    void*    p_vaddr;  // where the segment is located in virtual memory
    void*    p_paddr;  // same for physical memory (only on systems without MMU)
    uint32_t p_filesz; // the segment's length in the file
    uint32_t p_memsz;  // the segment's length in memory
    enum { // whether the segment should be executable, writable or readable
        PF_X = 0b1, PF_W = 0b10, PF_R = 0b100
    } p_flags;
    uint32_t p_align;  // how this segment is aligned
} __attribute__((packed)) elf_program_header_entry_t;

static uint8_t elf_check(elf_t* elf) {
    if (elf->e_ident.EI_MAG0 != MAGIC_0 || elf->e_ident.EI_MAG1 != MAGIC_1 ||
        elf->e_ident.EI_MAG2 != MAGIC_2 || elf->e_ident.EI_MAG3 != MAGIC_3) {
        println("%4aELF magic not found%a");
        return 0;
    }
    if (elf->e_ident.EI_CLASS != CLASS_32_BIT) {
        println("%4aELF not 32-bit%a");
        return 0;
    }
    if (elf->e_ident.EI_DATA != DATA_LITTLE_ENDIAN) {
        println("%4aELF not little endian%a");
        return 0;
    }
    if (elf->e_ident.EI_VERSION != VERSION || elf->e_version != VERSION) {
        println("%4aELF version not 1%a");
        return 0;
    }
    if (elf->e_type != TYPE_EXECUTABLE) {
        println("%4aELF not executable%a");
        return 0;
    }
    if (elf->e_machine != MACHINE_X86) {
        println("%4aELF target not x86%a");
        return 0;
    }
    return 1;
}

// loads the segments of an ELF file into memory and returns its entry point
void* elf_load(elf_t* elf, page_directory_t* page_directory) {
    if (!elf_check(elf)) // check whether it's a suitable ELF file
        return 0;
    // find the program header table that contains info on how to load the file
    elf_program_header_entry_t* program_header_table =
            (elf_program_header_entry_t*) ((uintptr_t) elf + elf->e_phoff);
    logln("ELF", "Program header entries:");
    page_directory_t* old_directory = vmm_load_page_directory(page_directory);
    for (int i = 0; i < elf->e_phnum; i++) { // process every entry in the table
        elf_program_header_entry_t* entry = program_header_table + i;
        logln("ELF", "[%d] type=%d offset=%08x vaddr=%08x paddr=%08x "
                "filesz=%08x memsz=%08x flags=%03b align=%08x", i,
                entry->p_type, entry->p_offset, entry->p_vaddr, entry->p_paddr,
                entry->p_filesz, entry->p_memsz, entry->p_flags, entry->p_align);
        if (entry->p_type == PT_LOAD)  { // we only process LOAD segments for now
            // claim the memory so that we can write to it
            vmm_use_virtual_memory(entry->p_vaddr, entry->p_memsz,
                    entry->p_flags & PF_W ? VMM_USER | VMM_WRITABLE : VMM_USER);
            // Fill the complete segment with zeroes. (There are cases when the
            // segment's p_memsz is bigger than p_filesz, for example for BSS
            // sections which need to be initialized with zeroes.)
            memset(entry->p_vaddr, 0, entry->p_memsz);
            // Now copy the actual segment's data from the file to memory.
            memcpy(entry->p_vaddr, (void*) ((uintptr_t) elf + entry->p_offset),
                    entry->p_filesz);
        }
    }
    vmm_load_page_directory(old_directory);
    return elf->e_entry;
}

void elf_unload(elf_t* elf, page_directory_t* page_directory) {
    if (!elf_check(elf))
        return;
    elf_program_header_entry_t* program_header_table =
            (elf_program_header_entry_t*) ((uintptr_t) elf + elf->e_phoff);
    page_directory_t* old_directory = vmm_load_page_directory(page_directory);
    for (int i = 0; i < elf->e_phnum; i++) {
        elf_program_header_entry_t* entry = program_header_table + i;
        if (entry->p_type == PT_LOAD)
            vmm_free(entry->p_vaddr, entry->p_memsz);
    }
    vmm_load_page_directory(old_directory);
}

elf_task_t* elf_create_task(elf_t* elf, size_t kernel_stack_len,
        size_t user_stack_len) {
    if (!elf) {
        println("%4aELF not found%a");
        return 0;
    }
    page_directory_t* dir = vmm_create_page_directory();
    elf_task_t* elf_task = vmm_alloc(sizeof(elf_task_t), VMM_KERNEL);
    elf_task->elf = elf;
    elf_task->task = task_create_user(elf_load(elf, dir), dir,
            kernel_stack_len, user_stack_len);
    return elf_task;
}

void elf_destroy_task(elf_task_t* elf_task) {
    if (!elf_task) {
        println("%4aELF not found%a");
        return;
    }
    elf_unload(elf_task->elf, elf_task->task->page_directory);
    task_destroy(elf_task->task);
    vmm_free(elf_task, sizeof(elf_task_t));
}