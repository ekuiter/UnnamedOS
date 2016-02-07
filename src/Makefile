all: kernel
	hdiutil mount ../build/image.img
	cp kernel/kernel ../build
	cp ../build/kernel "/Volumes/NO NAME/"

copy: all
ifeq ($(DISK),0)
else
	sudo umount /dev/disk$(DISK) || true
	sudo dd if=../build/image.img of=/dev/disk$(DISK)
endif

kernel:
	$(MAKE) -C kernel
run: all
	cd ../build; bochs -q
count:
	wc -l $(shell find -E ./ -regex '.*(c|cpp|S|h)')
create:
	bximage -fd -size=1.44 -q ../build/created_image.img
clean:
	$(MAKE) -C kernel clean
.PHONY: all copy kernel run count create clean
