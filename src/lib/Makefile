SRCS = $(shell find -E ./ -regex '.*(c|S)')
OBJS = $(addsuffix .o, $(basename $(SRCS)))
NAME = $(notdir $(shell pwd))

CC = /usr/local/bin/i586-elf-gcc
AR = /usr/local/bin/i586-elf-ar

ASFLAGS = -m32 -Wall -I.
CFLAGS = -m32 -Wall -O2 -I. -std=gnu99 -ffreestanding -nostartfiles \
	-fno-stack-protector -nostdinc -nostdlib -fno-builtin -g \
	-Wno-packed-bitfield-compat
ARFLAGS = rcs

$(NAME): $(OBJS)
	$(AR) $(ARFLAGS) $(NAME) $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $^

clean:
	rm -f $(OBJS)
	rm -f $(NAME)

.PHONY: $(NAME) clean
