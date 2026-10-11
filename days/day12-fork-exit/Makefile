DAY  := day12-fork-exit
IMG  := iso
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/irq_entry.o \
        kernel/console.o kernel/traps.o kernel/irq.o kernel/timer.o \
        kernel/keyboard.o kernel/mm.o kernel/sched.o kernel/switch.o kernel/fork_asm.o kernel/printk.o kernel/vsprintf.o kernel/lib_string.o kernel/main.o

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk
KEYS := h,i,backspace,i,ret
