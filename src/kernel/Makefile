SRCS = $(shell find -E ./ -regex '.*(c|S)')
OBJS = $(addsuffix .o,$(basename $(SRCS)))

CC = /usr/local/bin/i586-elf-gcc
LD = /usr/local/bin/i586-elf-ld

ASFLAGS = -m32 -Wall -I.
CFLAGS = -m32 -Wall -O2 -I. -std=gnu99 -ffreestanding -nostartfiles -fno-stack-protector -nostdinc -nostdlib -fno-builtin -g -Wno-packed-bitfield-compat
LDFLAGS = -Tkernel.ld

kernel: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $^

clean:
	rm -f $(OBJS)
	rm -f kernel

.PHONY: kernel clean
