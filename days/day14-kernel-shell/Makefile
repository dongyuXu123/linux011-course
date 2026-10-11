DAY  := day14-kernel-shell
IMG  := iso
BUILD := $(CURDIR)/build
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o kernel/fork_asm.o kernel/syscall.o kernel/syscall_entry.o kernel/printk.o kernel/vsprintf.o kernel/lib_string.o kernel/main.o $(BUILD)/user_blob.o

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

# 用户程序: nasm 平坦二进制 → objcopy 转成内核可链接的目标
$(BUILD)/user.bin: user/user.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/user_blob.o: user/blob.asm $(BUILD)/user.bin
	$(NASM) -f elf32 -I$(BUILD)/ $< -o $@

TIMEOUT := 90
KEYS := p,s,ret,u,p,t,i,m,e,ret,h,e,l,p,ret
TIMEOUT := 90
KEYS := p,s,ret,u,p,t,i,m,e,ret,h,e,l,p,ret
