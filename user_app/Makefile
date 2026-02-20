# ==========================================
# User App Makefile
# ==========================================

# 1. 通过 CROSS_COMPILE 自动推导编译器
# 如果 CROSS_COMPILE 为空，CC 就是 gcc (本机)
# 如果 CROSS_COMPILE 有值，CC 就是 交叉编译器
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc

# 2. 通用配置
# -pthread 既是编译选项也是链接选项，比单纯加 -lpthread 更规范
CFLAGS ?= -Wall -O2 -pthread

# 3. 目标定义
TARGETS := test_ioctl test_dma_stream stress_test

.PHONY: all clean

all: $(TARGETS)

%: %.c
	@echo "   [APP] Building $@ with $(CC)..."
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@echo "   [APP] Cleaning..."
	rm -f $(TARGETS)