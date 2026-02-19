# ==========================================
# Driver Makefile
# ==========================================
# 如果顶层没 export，就用默认值
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
CROSS_COMPILE ?= 

obj-m := pcie_edu.o

pcie_edu-y := edu_main.o edu_cdev.o edu_hw.o

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean