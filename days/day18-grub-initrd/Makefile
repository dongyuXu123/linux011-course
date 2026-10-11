DAY  := day18-grub-initrd
IMG  := iso
BUILD := $(CURDIR)/build
MODULES := $(BUILD)/initrd.img
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o \
        kernel/fork_asm.o kernel/syscall.o kernel/syscall_entry.o \
        kernel/printk.o kernel/vsprintf.o kernel/lib_string.o \
        kernel/main.o fs/ramfs.o

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

$(BUILD)/hello.bin: user/hello.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/initrd.img: $(BUILD)/hello.bin
	python3 $(REPO)/tools/mkinitrd.py $@ hello=$<

TIMEOUT := 120
KEYS := r,u,n,spc,h,e,l,l,o,ret
