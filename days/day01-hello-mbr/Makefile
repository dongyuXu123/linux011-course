DAY  := day01-hello-mbr
IMG  := mbr

REPO := $(abspath $(CURDIR)/../..)
include $(REPO)/mk/kernel.mk
include $(REPO)/mk/test.mk

$(BUILD)/os.img: boot/bootsect.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin -Iboot $< -o $@
