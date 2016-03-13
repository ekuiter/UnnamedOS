#ifndef MULTIBOOT_H
#define MULTIBOOT_H

/* multiboot.h - the header for Multiboot */
/* Copyright (C) 1999, 2001  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

/* Macros. */

/* The magic number for the Multiboot header. */
#define MULTIBOOT_HEADER_MAGIC          0x1BADB002

/* The flags for the Multiboot header. */
#ifdef __ELF__
# define MULTIBOOT_HEADER_FLAGS         0x00000003
#else
# define MULTIBOOT_HEADER_FLAGS         0x00010003
#endif

/* The magic number passed by a Multiboot-compliant boot loader. */
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

/* The size of our stack (16KB). */
#define STACK_SIZE                      0x4000

/* C symbol format. HAVE_ASM_USCORE is defined by configure. */
#ifdef HAVE_ASM_USCORE
# define EXT_C(sym)                     _ ## sym
#else
# define EXT_C(sym)                     sym
#endif

#ifndef ASM
/* Do not include here in boot.S. */

/* Types. */

/* The Multiboot header. */
typedef struct multiboot_header
{
  uint32_t magic, flags, checksum, header_addr, load_addr, load_end_addr,
          bss_end_addr, entry_addr;
} multiboot_header_t;

/* The symbol table for a.out. */
typedef struct aout_symbol_table
{
  uint32_t tabsize, strsize, addr, reserved;
} aout_symbol_table_t;

/* The section header table for ELF. */
typedef struct elf_section_header_table
{
  uint32_t num, size, addr, shndx;
} elf_section_header_table_t;

/* The Multiboot information. */
typedef struct multiboot_info
{
  struct {
      uint32_t mem : 1, boot_device : 1, cmdline : 1, mods : 1,
      aout_symbol_table : 1, elf_section_header_table : 1, mmap : 1, drives : 1,
      config_table : 1, boot_loader_name : 1, apm_table : 1, vbe : 1;
  } flags;
  uint32_t mem_lower, mem_upper;
  struct {
      uint8_t part3;
      uint8_t part2;
      uint8_t part1;
      struct {
          uint8_t number : 7, hard_disk : 1;
      } drive;
  } boot_device;
  uint32_t cmdline, mods_count, mods_addr;
  union
  {
    aout_symbol_table_t aout_sym;
    elf_section_header_table_t elf_sec;
  } u;
  uint32_t mmap_length, mmap_addr, drives_length, drives_addr, config_table,
          boot_loader_name, apm_table, vbe_control_info, vbe_mode_info,
          vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
} multiboot_info_t;

/* The module structure. */
typedef struct module
{
  uint32_t mod_start, mod_end, string, reserved;
} module_t;

/* The memory map. Be careful that the offset 0 is base_addr_low
   but no size. */
typedef struct memory_map
{
  uint32_t size, base_addr_low, base_addr_high, length_low, length_high, type;
} memory_map_t;

void multiboot_init(multiboot_info_t* mb_info, uint32_t mb_magic);

#endif /* ! ASM */

#endif