#!/bin/bash

# 1. 指定 Buildroot 生成的那两个文件在哪里
KERNEL_IMG="buildroot/output/images/bzImage"
ROOTFS_IMG="buildroot/output/images/rootfs.ext2"

# 2. 检查一下文件还在不在，不在就报错
if [ ! -f "$KERNEL_IMG" ] || [ ! -f "$ROOTFS_IMG" ]; then
    echo "Error: Images not found! Did the build finish?"
    exit 1
fi

# 3. 启动 QEMU (这就是在组装硬件)
# -nographic: 不弹窗，直接在终端里显示
# -device edu: 挂载我们要开发的 PCIe 设备！
qemu-system-x86_64 \
    -M pc \
    -kernel "$KERNEL_IMG" \
    -drive file="$ROOTFS_IMG",format=raw,if=virtio \
    -append "root=/dev/vda console=ttyS0" \
    -nographic \
    -device edu