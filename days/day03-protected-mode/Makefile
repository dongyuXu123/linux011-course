DAY  := day03-protected-mode
IMG  := mbr

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

# 镜像 = MBR(扇区1) + PM代码(扇区2~3)，补零到 1.44MB 软盘
$(BUILD)/os.img: boot/bootsect.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin -Iboot $< -o $@
	truncate -s 1474560 $@
