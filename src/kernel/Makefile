SRCS = $(shell find -E ./ -regex '.*(c|S)') # collect all source files
OBJS = $(addsuffix .o, $(basename $(SRCS))) # the target object files

CC = /usr/local/bin/i586-elf-gcc
LD = /usr/local/bin/i586-elf-ld

# flags for assembler, compiler and linker
ASFLAGS = -m32 -Wall -I. -Ilib
CFLAGS = -m32 -Wall -O2 -I. -Ilib -std=gnu99 -ffreestanding -nostartfiles \
	-fno-stack-protector -nostdinc -nostdlib -fno-builtin -g \
	-Wno-packed-bitfield-compat
LDFLAGS = -Tscript.ld # the linker script with details on the kernel sections

# kernel is a phony target, meaning it is not a file but a recipe to call make
# with. It depends on every object file, so they are taken care of first. Then
# it invokes the linker with flags and $@ (which is the target's name 'kernel',
# the output file) and $^ (which is the target's dependencies '$(OBJS)').
kernel: $(OBJS) 
	$(LD) $(LDFLAGS) -o $@ $^

# This is not a phony target: This describes how .o files are made from .c
# files, just like above. $@ expands here to the actual name of the object
# file and $^ to the source file name.
%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $^

# similar to above, but we invoke the assembler
%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $^

clean:
	rm -f $(OBJS)
	rm -f kernel

# which targets are only meant as recipes, not as files
.PHONY: kernel clean 
