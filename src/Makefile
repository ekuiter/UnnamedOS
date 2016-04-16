# Collects all subfolders in src as modules to be compiled, at least 'kernel'.
CP_MODS=$(filter-out lib, $(notdir $(shell find ./ -maxdepth 1 -type d)))
MODULES=lib
MODULES+=$(CP_MODS)

# foreach takes a variable, a space-separated list to iterate and a command
# in which every occurrence of the variable is replaced by every item in the
# list, for example: cp kernel/kernel ../build; cp user/user ../build and so on.
# the - before test lets make ignore test's return value.
all: modules
	-test ! -d "/Volumes/NO NAME/" && hdiutil mount ../build/image.img
	$(foreach MOD, $(CP_MODS), cp $(MOD)/$(MOD) ../build;)
	$(foreach MOD, $(CP_MODS), cp ../build/$(MOD) "/Volumes/NO NAME/";)
	cp ../build/menu.lst "/Volumes/NO NAME/grub/"

copy: all
ifdef DISK
	sudo umount /dev/disk$(DISK) || true
	-sudo dd if=../build/image.img of=/dev/disk$(DISK)
else
	$(warning You need to specify a DISK to copy to.)
endif

# foreach results in something like this: make -C kernel; make -C user ...
modules:
	$(foreach MOD, $(MODULES), $(MAKE) -C $(MOD);)

run-bochs: all
	-cd ../build; /usr/local/bin/bochs -q -rc .debugrc

run-qemu: all
	/usr/local/bin/qemu-system-i386 -fda ../build/image.img -boot a -no-fd-bootchk

count:
	wc -l $(shell find -E ./ -regex '.*(c|cpp|S|h)')

# $@ expands to the targets name 'clean': make -C kernel clean; ...
clean:
	-test ! -d "/Volumes/NO NAME/" && hdiutil mount ../build/image.img
	$(foreach MOD, $(MODULES), $(MAKE) -C $(MOD) $@;)
	$(foreach MOD, $(CP_MODS), rm -f "/Volumes/NO NAME/$(MOD)";)
	rm -f "/Volumes/NO NAME/grub/menu.lst"

.PHONY: all copy modules run count clean
