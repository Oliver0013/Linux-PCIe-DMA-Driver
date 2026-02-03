# ==========================================
# Linux 驱动开发万能 Makefile 模板
# ==========================================

# 1. 配置区 (只需要改这里!)
# ------------------------------------------
# A. 你的内核源码在哪里？
# (根据实际情况修改路径)
KERNEL_DIR := $(HOME)/workspace/Linux-PCIe-DMA-Driver/buildroot/output/build/linux-6.1.44
# 交叉编译路径
CROSS_COMPILE := $(HOME)/workspace/Linux-PCIe-DMA-Driver/buildroot/output/host/bin/x86_64-buildroot-linux-gnu-
# 3. 目标模块

# B. 你的驱动源文件叫什么？
# (例如：如果源文件是 hello.c，这里就填 hello.o)
obj-m := pcie_edu.o

# ==========================================
# 2. 核心区 (不要动!)
# ==========================================
# 获取当前目录路径
PWD := $(shell pwd)
# 借用内核的构建系统编译模块
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) modules

# 清理编译生成的垃圾文件
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean