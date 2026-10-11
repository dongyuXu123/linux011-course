# ============ 每课测试/交互运行规则（在 mk/kernel.mk 之后 include）============
# 变量：EXPECT(默认 test/expect.txt) TIMEOUT(默认 45) SCREEN(默认 1)

EXPECT  ?= $(DAY_DIR)/test/expect.txt
TIMEOUT ?= 45
SCREEN  ?= 1
KEYS    ?=

.PHONY: test run report
test: $(BUILD)/artifact
	@bash $(REPO)/tools/qemu-test.sh --image $(BUILD)/os.$(EXT) \
	  $(if $(wildcard $(EXPECT)),--expect $(EXPECT),) \
	  --timeout $(TIMEOUT) --screen $(SCREEN) --name $(DAY) \
	  $(if $(KEYS),--send-keys "$(KEYS)",)

run: $(BUILD)/artifact
	$(QEMU) $(QEMU_ARGS) $(if $(filter iso,$(IMG)),-cdrom $(BUILD)/os.iso -boot d,-fda $(BUILD)/os.img -boot a)

report: test
	@cat $(BUILD)/test/result.json
