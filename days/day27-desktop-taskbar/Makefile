DAY  := day27-desktop-taskbar
IMG  := iso
BUILD := $(CURDIR)/build
EXTRA_DEPS := $(BUILD)/initrd.img
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o \
        kernel/syscall.o kernel/syscall_entry.o \
        kernel/printk.o kernel/vsprintf.o kernel/lib_string.o \
        kernel/main.o fs/ramfs.o kernel/selftest.o kernel/fb.o kernel/gfx.o kernel/mouse.o kernel/desktop.o kernel/ata.o

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

$(BUILD)/initrd.img: $(BUILD)/init.bin $(BUILD)/hello.bin
	python3 $(REPO)/tools/mkinitrd.py $@ init=$(BUILD)/init.bin hello=$(BUILD)/hello.bin

TIMEOUT := 60
MOUSE := 80,-172;0,0,down-left;60,80;0,0,up-left;0,0,click-left
TIMEOUT := 90
PIXEL := 250,475=181830; 30,470=e0e0ff; 320,308=a04020
EXTRA := -drive file=$(BUILD)/initrd.img,format=raw,if=ide
KEYS := r,u,n,spc,h,e,l,l,o,ret

PIXEL := 250,475=181830; 30,470=e0e0ff; 320,308=a04020
