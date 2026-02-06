# ==========================================
# 顶层 Makefile
# ==========================================

# 1. 定义路径变量 (使用绝对路径，避免出错)
TOP_DIR := $(shell pwd)
BR_DIR  := $(TOP_DIR)/buildroot

# 定义内核路径
KERNEL_DIR := $(BR_DIR)/output/build/linux-6.1.44
# 定义交叉编译前缀 (注意：结尾有横杠 -)
CROSS_COMPILE := $(BR_DIR)/output/host/bin/x86_64-buildroot-linux-gnu-

# 核心：将这两个变量导出，子 Makefile 直接继承！
# 这样调用子 make 时就不用写一堆参数了，逻辑最简洁。
export KERNEL_DIR CROSS_COMPILE

# --- 2. 目录定义 ---
DRIVER_DIR   := $(TOP_DIR)/driver
USER_APP_DIR := $(TOP_DIR)/user_app
OVERLAY_DIR  := $(TOP_DIR)/bsp/board/qemu_x86_64/rootfs_overlay
SCRIPT_DIR   := $(TOP_DIR)/scripts

# 伪目标，防止和同名文件冲突
.PHONY: all driver user_app deploy image run clean distclean

# ==========================================
# 2. 核心指令
# ==========================================

# 默认目标：编译驱动 -> 部署 -> 重包镜像
all: driver user_app deploy image
	@echo "======================================="
	@echo "   构建完成！输入 'make run' 启动仿真   "
	@echo "======================================="

# 步骤 1: 编译驱动
driver:
	@echo ">>> [1/3] Compiling Driver..."
	$(MAKE) -C $(DRIVER_DIR)
# 步骤 2：编译用户程序
user_app:
	@echo ">>> [App] Building..."
	$(MAKE) -C $(USER_APP_DIR)

# 步骤 3: 部署到 Overlay
# mkdir -p 确保目录存在，cp -f 强制覆盖
deploy:
	@echo ">>> [2/3] Deploying to Overlay..."
	cp -f $(DRIVER_DIR)/pcie_edu.ko $(OVERLAY_DIR)/lib/modules/6.1.44/extra/
	cp -f $(USER_APP_DIR)/test_rw $(OVERLAY_DIR)/root/


# 步骤 4: 重建文件系统镜像
# 技巧：删除 images/rootfs.cpio 会欺骗 Buildroot 认为产物丢失，
# 从而触发 target-finalize 重新打包 overlay，而不会重新编译整个内核，速度极快。
image:
	@echo ">>> [3/3] Re-packing Rootfs..."
	rm -f $(BR_DIR)/output/images/rootfs.ext2
	$(MAKE) -C $(BR_DIR)

# ==========================================
# 3. 辅助指令
# ==========================================

# 启动 QEMU
run:
	@echo ">>> Booting QEMU via script..."
	@chmod +x $(SCRIPT_DIR)/run_qemu.sh
	$(SCRIPT_DIR)/run_qemu.sh

# 清理驱动 (不清理 Buildroot，太慢了)
clean:
	$(MAKE) -C $(DRIVER_DIR) clean
	$(MAKE) -C $(USER_APP_DIR) clean
	rm -f $(OVERLAY_DIR)/lib/modules/6.1.44/extra/pcie_edu.ko $(OVERLAY_DIR)/root/test_rw
