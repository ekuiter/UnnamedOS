## "UnnamedOS"

This is an attempt at implementing a simple OS for the IA-32 architecture. Supporting files (like Makefiles) use Mac OS specific commands.

It uses a **i586-elf-gcc crosscompiler** and **Bochs** ([read more](https://github.com/ekuiter/homebrew-crosscompiler)).

Run the following commands in the root directory:
- To compile, run `make`.
- To run in the Bochs emulator, type `make run`.
- To copy the OS to a floppy disk `/dev/diskX`, type `DISK=X make copy` (sudo password needed, show progress with `Ctrl+T`).

**Implemented features**

- flat memory model (GDT)
- printf-esque output
- CPU info (CPUID)
- interrupts (IDT, PIC, ISRs)
- system clock & speaker driver (PIT)
- keyboard & mouse driver (PS/2)
- multiboot info
- userspace multitasking (TSS)
- ELF GRUB modules
- memory manager (PMM)