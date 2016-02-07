SRCS = $(shell find -E ./ -regex '.*(c|S)')
OBJS = $(addsuffix .o,$(basename $(SRCS)))

CC = /usr/local/bin/i586-elf-gcc
LD = /usr/local/bin/i586-elf-ld

ASFLAGS = -m32
CFLAGS = -m32 -Wall -O3 -I. -std=gnu99 -ffreestanding -nostartfiles -fno-stack-protector -nostdinc -nostdlib -fno-builtin -finline-functions -funswitch-loops -fpredictive-commoning -fgcse-after-reload -ftree-slp-vectorize -fvect-cost-model -ftree-partial-pre -fipa-cp-clone -g
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
