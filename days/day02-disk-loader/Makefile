DAY  := day02-disk-loader
IMG  := mbr

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

$(BUILD)/bootsect.bin: boot/bootsect.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin -Iboot $< -o $@

$(BUILD)/stage2.bin: boot/stage2.asm
	$(NASM) -f bin -Iboot $< -o $@

# 磁盘镜像 = MBR(扇区1) + stage2(扇区2..5)，其余补零到 1.44MB 软盘
$(BUILD)/os.img: $(BUILD)/bootsect.bin $(BUILD)/stage2.bin
	cat $^ > $@
	truncate -s 1474560 $@
