DAY  := day35-fs-abi
IMG  := iso
BUILD := $(CURDIR)/build
EXTRA_DEPS := $(BUILD)/minix.img
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o \
        kernel/syscall.o kernel/syscall_entry.o \
        kernel/printk.o kernel/vsprintf.o kernel/lib_string.o \
        kernel/main.o fs/ramfs.o kernel/selftest.o kernel/ata.o kernel/blk.o kernel/buffer.o kernel/minix.o

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

UCFLAGS := -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
  -fno-builtin -nostdlib -nostdinc -O1 -g -fno-asynchronous-unwind-tables \
  -fno-tree-loop-distribute-patterns -mno-mmx -mno-sse

$(BUILD)/init.elf: user/init.c
	@mkdir -p $(BUILD)
	$(CC) $(UCFLAGS) -T user/user.ld $< -o $@ -no-pie -Wl,--build-id=none

$(BUILD)/init.bin: $(BUILD)/init.elf
	objcopy -O binary $< $@

$(BUILD)/hello.bin: user/hello.asm
	$(NASM) -f bin $< -o $@

$(BUILD)/minix.img: $(BUILD)/init.bin $(BUILD)/hello.bin $(REPO)/tools/mkminix.py
	python3 $(REPO)/tools/mkminix.py $@ init=$(BUILD)/init.bin hello=$(BUILD)/hello.bin

TIMEOUT := 120
EXTRA := -drive file=$(BUILD)/minix.img,format=raw,if=ide
KEYS := s,a,v,e,ret,r,u,n,spc,h,e,l,l,o,ret
