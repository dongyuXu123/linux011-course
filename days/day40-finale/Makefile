DAY  := day40-finale
IMG  := iso
BUILD := $(CURDIR)/build
EXTRA_DEPS := $(BUILD)/minix.img
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o \
        kernel/syscall.o kernel/syscall_entry.o \
        kernel/printk.o kernel/vsprintf.o kernel/lib_string.o \
        kernel/main.o fs/ramfs.o kernel/selftest.o kernel/ata.o kernel/blk.o kernel/buffer.o kernel/minix.o kernel/tty.o kernel/fb.o kernel/gfx.o kernel/desktop.o kernel/mouse.o kernel/term.o kernel/splash.o

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

$(BUILD)/clock.bin: user/clock.c
	$(CC) $(UCFLAGS) -T user/user.ld $< -o $(BUILD)/clock.elf -no-pie -Wl,--build-id=none
	objcopy -O binary $(BUILD)/clock.elf $@

$(BUILD)/files.bin: user/files.c
	$(CC) $(UCFLAGS) -T user/user.ld $< -o $(BUILD)/files.elf -no-pie -Wl,--build-id=none
	objcopy -O binary $(BUILD)/files.elf $@

$(BUILD)/gdemo.bin: user/gdemo.c
	$(CC) $(UCFLAGS) -T user/user.ld $< -o $(BUILD)/gdemo.elf -no-pie -Wl,--build-id=none
	objcopy -O binary $(BUILD)/gdemo.elf $@

.PHONY: FORCE
FORCE:

$(BUILD)/minix.img: FORCE $(BUILD)/init.bin $(BUILD)/hello.bin $(BUILD)/gdemo.bin $(BUILD)/clock.bin $(BUILD)/files.bin $(REPO)/tools/mkminix.py
	python3 $(REPO)/tools/mkminix.py $@ init=$(BUILD)/init.bin hello=$(BUILD)/hello.bin gdemo=$(BUILD)/gdemo.bin clock=$(BUILD)/clock.bin files=$(BUILD)/files.bin

TIMEOUT := 120
EXTRA := -drive file=$(BUILD)/minix.img,format=raw,if=ide
MOUSE := -120,0;-100,-192;0,0,down-left;60,20;0,0,up-left
PIXEL := 300,120=e8e8e8; 170,461=303058
KEYS := f,i,l,e,s,ret,g,u,i,ret,w,spc,n,2,dot,t,x,t,spc,h,i,ret,c,a,t,spc,n,2,dot,t,x,t,ret,n,e,x,t,ret
