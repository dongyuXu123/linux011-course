DAY  := day06-paging-idt
IMG  := iso
OBJS := boot/multiboot.o boot/head.o kernel/trap_entry.o kernel/console.o kernel/traps.o kernel/main.o

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk
