# ============ 共享构建规则：每课 Makefile 只声明差异 ============
#
# 每课 Makefile 需要提供的变量：
#   DAY    := day04-grub-multiboot     # 目录名（用于测试报告）
#   IMG    := iso | mbr                # 产物类型：GRUB ISO 或 裸引导软盘镜像
#   OBJS   := boot/multiboot.o kernel/main.o ...   # 参与链接的目标文件
#   MODULES:= boot/initrd.img          # (可选) 放入 ISO /boot 的附加文件(initrd)
#
# 提供：all / clean；配合 mk/test.mk 提供 test / run。

DAY_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
REPO    := $(patsubst %/,%,$(dir $(abspath $(DAY_DIR)/../)))
BUILD   := $(DAY_DIR)/build

CC      := gcc
LD      := ld
AS      := gcc
NASM    := nasm

CFLAGS  := -m32 -march=i486 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -fno-builtin -nostdlib -nostdinc -fno-asynchronous-unwind-tables \
           -fno-omit-frame-pointer -fno-tree-loop-distribute-patterns \
           -mno-mmx -mno-sse -mno-sse2 \
           -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
           -I$(DAY_DIR)/include -I$(REPO)/mk/include
ASFLAGS := -m32 --32 -I$(DAY_DIR)/boot
LDFLAGS := -m elf_i386 -no-pie --build-id=none

QEMU    := qemu-system-i386
QEMU_ARGS ?= -m 32M -vga std

.PHONY: all clean
all: $(BUILD)/artifact

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.asm
	$(NASM) -f elf32 -I$(dir $<) $< -o $@

# ---------- 产物类型 ----------

ifeq ($(IMG),iso)
EXT     := iso
ISO_ROOT := $(BUILD)/iso-root
LINKER   := $(REPO)/mk/linker.ld

$(BUILD)/kernel.elf: $(OBJS) $(LINKER)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -T $(LINKER) $(OBJS) -o $@

$(BUILD)/artifact: $(BUILD)/kernel.elf
	@rm -rf $(ISO_ROOT) && mkdir -p $(ISO_ROOT)/boot/grub
	cp $(BUILD)/kernel.elf $(ISO_ROOT)/boot/
ifdef MODULES
	cp $(MODULES) $(ISO_ROOT)/boot/
endif
	printf 'set timeout=0\nset default=0\n' > $(ISO_ROOT)/boot/grub/grub.cfg
	cat $(REPO)/mk/grub.head >> $(ISO_ROOT)/boot/grub/grub.cfg
	if [ -f $(DAY_DIR)/grub.cfg ]; then cat $(DAY_DIR)/grub.cfg >> $(ISO_ROOT)/boot/grub/grub.cfg; else printf 'menuentry "linux011-course" {\n  multiboot /boot/kernel.elf\n  boot\n}\n' >> $(ISO_ROOT)/boot/grub/grub.cfg; fi
ifdef MODULES
	if [ -f $(DAY_DIR)/grub.cfg ]; then true; fi
endif
	grub-mkrescue -volid LINUX011_COURSE -o $(BUILD)/os.iso $(ISO_ROOT) >/dev/null 2>&1

else ifeq ($(IMG),mbr)
EXT     := img

$(BUILD)/artifact: $(BUILD)/os.img
	@true

else
$(error IMG 必须是 iso 或 mbr)
endif

clean:
	rm -rf $(BUILD)
