# ==========================================
# 顶层 Makefile - 项目指挥中心
# ==========================================

# 1. 定义路径变量 (使用绝对路径，避免出错)
TOP_DIR := $(shell pwd)
DRIVER_DIR := $(TOP_DIR)/driver
BR_DIR := $(TOP_DIR)/buildroot
# 注意：Overlay 路径变更为 bsp/board/...
OVERLAY_DIR := $(TOP_DIR)/bsp/board/qemu_x86_64/rootfs_overlay/root
# 脚本路径
SCRIPT_DIR := $(TOP_DIR)/scripts

# 伪目标，防止和同名文件冲突
.PHONY: all driver deploy image run clean distclean

# ==========================================
# 2. 核心指令
# ==========================================

# 默认目标：编译驱动 -> 部署 -> 重包镜像
all: driver deploy image
	@echo "======================================="
	@echo "   构建完成！输入 'make run' 启动仿真   "
	@echo "======================================="

# 步骤 1: 编译驱动
driver:
	@echo ">>> [1/3] Compiling Driver..."
	$(MAKE) -C $(DRIVER_DIR)

# 步骤 2: 部署到 Overlay
# mkdir -p 确保目录存在，cp -f 强制覆盖
deploy:
	@echo ">>> [2/3] Deploying to Overlay..."
	@mkdir -p $(OVERLAY_DIR)
	cp -f $(DRIVER_DIR)/pcie_edu.ko $(OVERLAY_DIR)/
	@echo "Driver copied to: $(OVERLAY_DIR)/"

# 步骤 3: 重建文件系统镜像
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
	rm -f $(OVERLAY_DIR)/pcie_edu.ko

# 彻底清理 (慎用！)
distclean: clean
	@echo ">>> Cleaning Buildroot (This will take time to rebuild!)..."
	$(MAKE) -C $(BR_DIR) clean