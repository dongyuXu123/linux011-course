# ============ 顶层：全课程构建与测试矩阵 + 原有源码校验 ============
DAYS := $(sort $(notdir $(wildcard days/day*)))

.PHONY: all check test test-day test-all report run clean list
all: check

# 原有：只读源码校验（保持不动）
check:
	python3 scripts/check-source-study.py

list:
	@for d in $(DAYS); do echo "$$d"; done

# 单课: make test-day D=day04-grub-multiboot
test-day:
	@$(MAKE) -s -C days/$(D) test

# 全部课程测试，输出矩阵
test-all:
	@total=0; pass=0; \
	echo "| 课次 | 状态 | 断言 | 用时 |"; echo "|---|---|---|---|"; \
	for d in $(DAYS); do \
	  if $(MAKE) -s -C days/$$d test >/tmp/t.log 2>&1; then st=✅; pass=$$((pass+1)); else st=❌; fi; \
	  total=$$((total+1)); \
	  meta=$$(python3 -c "import json;d=json.load(open('days/$$d/build/test/result.json'));print(d['matched'],d['seconds'])" 2>/dev/null || echo '?,?'); \
	  echo "| $$d | $$st | $$meta |"; \
	  [ "$$st" = ❌ ] && tail -5 /tmp/t.log; \
	done; \
	echo; echo "通过 $$pass/$$total"; \
	[ "$$pass" = "$$total" ]

report: test-all ; # test-all 已输出矩阵

clean:
	@for d in $(DAYS); do $(MAKE) -s -C days/$$d clean; done
	@rm -f /tmp/t.log
