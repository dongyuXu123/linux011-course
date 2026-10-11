# ============ 调试设施：GDB stub + 符号调试（所有课通用）============
#
#   make debug   启动 QEMU 图形窗口, 开启 gdb stub (localhost:1234)。
#                会先结束正在运行的课程 QEMU 实例。
#   make gdb     另开终端执行: 连接 stub, 载入内核符号, 常用断点已备好。
#
# 典型流程:
#   make -C days/day30-release debug        # 窗口起来, 系统在跑
#   make -C days/day30-release gdb          # 进入 gdb ( guests 被暂停)
#   (gdb) break schedule                    # 设断点
#   (gdb) continue                          # 放行, 命中后单步/打印
#   (gdb) print jiffies                     # 全局变量直接看
#   (gdb) detach                            # 脱离, 系统恢复运行
#
# 内核编译带 -g, 源码级调试直接 list/next。

GDB_PORT ?= 1234

.PHONY: debug gdb quietly

# 结束本课程之前启动的调试实例(精确匹配进程名, 不误伤)
quietly:
	@-for p in $$(pgrep -x qemu-system-i38); do kill $$p; done 2>/dev/null || true

debug: quietly $(BUILD)/artifact $(EXTRA_DEPS)
	@cp $(EXTRA_DEPS) /tmp/initrd-$(DAY).img 2>/dev/null || true
	setsid -f $(QEMU) $(QEMU_ARGS) $(if $(filter iso,$(IMG)),-cdrom $(BUILD)/os.iso -boot d,-fda $(BUILD)/os.img -boot a) \
	  -s -serial file:$(BUILD)/debug-serial.log \
	  $(if $(EXTRA),$(subst $(BUILD)/initrd.img,/tmp/initrd-$(DAY).img,$(EXTRA)),) \
	  & echo "QEMU 调试实例已启动 (gdb stub :$(GDB_PORT))。连接: make -C $(CURDIR) gdb"

$(BUILD)/course-gdbinit: $(BUILD)/kernel.elf $(REPO)/mk/gdbinit.tmpl
	@mkdir -p $(BUILD)
	@sed -e 's|@KERNEL@|$(BUILD)/kernel.elf|g' -e 's|@PORT@|$(GDB_PORT)|g' \
	     -e 's|@DAY@|$(DAY)|g' $(REPO)/mk/gdbinit.tmpl > $@

gdb: $(BUILD)/course-gdbinit
	gdb -q -x $(BUILD)/course-gdbinit
