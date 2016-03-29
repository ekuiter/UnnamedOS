#ifndef TASKS_ELF_H
#define TASKS_ELF_H

#include <stdint.h>
#include <mem/vmm.h>
#include <tasks/task.h>

typedef struct { // the ELF header at the start of every ELF file
    struct {     // (here for 32 bit and little endian)
        uint8_t EI_MAG0, EI_MAG1, EI_MAG2, EI_MAG3, // ELF magic
            EI_CLASS, EI_DATA,    // 32 or 64 bit, little or big endian
            EI_VERSION, EI_OSABI; // ELF version, which ABI to use
        uint32_t : 32, : 32;      // unused
    } __attribute__((packed)) e_ident;
    uint16_t e_type;      // object type (relocatable or executable)
    uint16_t e_machine;   // targeted ISA (=Instruction Set Architecture)
    uint32_t e_version;   // ELF version
    void*    e_entry;     // entry point
    uint32_t e_phoff;     // program header table (important for executables)
    uint32_t e_shoff;     // section header table (important for relocatables)
    uint32_t e_flags;     // architecture specific flags
    uint16_t e_ehsize;    // header size (52 for 32-bit)
    uint16_t e_phentsize; // size of a single program header entry
    uint16_t e_phnum;     // number of program header entries
    uint16_t e_shentsize; // size of a single section header entry
    uint16_t e_shnum;     // number of section header entries
    uint16_t e_shstrndx;  // index of the section that contains section strings
} __attribute__((packed)) elf_header_t;

typedef elf_header_t elf_t; // an ELF file starts with the header

typedef struct {
    elf_t* elf;
    task_t* task;
} elf_task_t;

void* elf_load(elf_t* elf, page_directory_t* page_directory);
void elf_unload(elf_t* elf, page_directory_t* page_directory);
elf_task_t* elf_create_task(elf_t* elf, size_t kernel_stack_len, size_t user_stack_len);
void elf_destroy_task(elf_task_t* elf_task);

#endif
