# ==========================================
# User App Makefile
# ==========================================

# 1. 通过 CROSS_COMPILE 自动推导编译器
# 如果 CROSS_COMPILE 为空，CC 就是 gcc (本机)
# 如果 CROSS_COMPILE 有值，CC 就是 交叉编译器
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc

# 2. 通用配置
CFLAGS ?= -Wall -O2

# 3. 目标定义
TARGET := test_rw
SRCS := test_rw.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "   [APP] Building with $(CC)..."
	$(CC) $(CFLAGS) -o $@ $^

clean:
	@echo "   [APP] Cleaning..."
	rm -f $(TARGET)