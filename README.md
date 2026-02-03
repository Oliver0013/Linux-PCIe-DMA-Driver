# Linux PCIe DMA Driver for QEMU EDU Device

![License](https://img.shields.io/badge/license-GPLv2-blue.svg)
![Kernel](https://img.shields.io/badge/kernel-5.15%2B-green.svg)
![Status](https://img.shields.io/badge/status-active-orange.svg)

## 📖 项目简介 (Introduction)

本项目旨在无物理硬件环境下，基于 **QEMU** 和 **Buildroot** 构建完整的嵌入式 Linux BSP，并针对 QEMU 提供的 `edu` 教育用设备，开发一个具备**工业级特性**的 PCIe 驱动程序。

项目核心目标是深入理解 Linux 内核子系统，重点攻克 **PCIe 协议栈**、**MSI 中断处理**、**DMA (Direct Memory Access)** 以及内核态的**并发控制**。

这是一个针对嵌入式 Linux 驱动/内核岗位的实战演练项目，旨在解决传统学习中“缺乏真实硬件交互”和“驱动逻辑过于简单”的痛点。

## 📂 项目结构 (Directory Structure)

```text
Linux-PCIe-DMA-Driver/
├── bsp/                    # BSP (Board Support Package) 构建相关
│   └── configs/            # Buildroot 的 defconfig 配置文件
├── driver/                 # Linux 内核驱动源码
│   ├── pcie_edu.c          # 驱动核心代码 (Probe, DMA, ISR)
│   ├── pcie_edu.h          # 寄存器定义与数据结构
│   └── Makefile            # 内核模块构建脚本
├── user_app/               # 用户态测试与交互程序
│   ├── test_rw.c           # 基础读写测试
│   └── benchmark.py        # 性能基准测试脚本
├── scripts/                # 自动化辅助脚本
│   ├── run_qemu.sh         # QEMU 一键启动脚本
│   └── load_driver.sh      # 驱动加载与设备节点创建脚本
├── docs/                   # 技术文档与学习笔记
│   ├── edu_datasheet.txt   # QEMU EDU 设备规范
│   └── dev_notes.md        # 开发过程中的踩坑记录
└── README.md               # 项目主文档

```

## 🛠️ 技术栈 (Tech Stack)

* **Kernel:** Linux 5.15 LTS (or newer)
* **Build System:** Buildroot / Makefile
* **Hypervisor:** QEMU (x86_64 target)
* **Driver Features:**
* PCIe Configuration Space & MMIO Mapping
* MSI/MSI-X Interrupt Handling
* DMA Scatter-Gather Mapping
* Concurrency Control (Mutex/Spinlock)
* Character Device Interface (ioctl)



## 📅 开发进度日志 (DevLog)

### P0: 环境准备与预研

* [x] **2026-01-30**: 初始化 GitHub 仓库，建立符合工业规范的目录结构。
* [x] **2026-01-31**: 下载 Linux 5.15 和 Buildroot 源码，配置 `.gitignore` 规则。

### P1: BSP 构建

* [x] **2026-02-02**: Buildroot 初始配置 (`qemu_x86_64_defconfig`)。
* [x] **2026-02-03**: 深度内核裁剪。
+ 移除多媒体支持 (Sound/Video)、无线网络 (Wireless/Bluetooth)。
+ 移除 IPv6 协议栈与 Netfilter 防火墙，保留基础 TCP/IP, IPV4协议栈。
+ 移除 USB 子系统：为 P3/P4 阶段的性能分析构建纯净环境，消除 USB 轮询中断干扰。
+ 成果：内核体积压缩至 4.4 MB。

### P2: 驱动骨架与开发环境搭建

* [x] **2026-02-03**: 树外编译环境 (Out-of-Tree Build) 搭建。
+ 建立独立驱动目录 driver/，编写通用 Makefile，解耦内核源码与驱动代码。
+ 解决交叉编译问题：修正 Host GCC (9.4) 与 Buildroot GCC (12.4) 版本不匹配导致的 ABI 兼容性错误 (-ftrivial-auto-var-init)。
* [x] **2026-02-03**: 部署与验证闭环。
+ 配置 BR2_ROOTFS_OVERLAY 机制，实现 .ko 文件自动打包至 Rootfs。
+ 解决 Buildroot 构建缓存导致的 Overlay 不更新问题 (手动清理 output/target)。
+ 上板验证：成功执行 insmod 加载模块，通过 dmesg 观测到主设备号分配成功，确认 lspci 能够识别 QEMU EDU 设备物理存在。

---

## 🚀 快速开始 (Quick Start)

### 1. 环境依赖

```bash
sudo apt-get install build-essential qemu-system-x86 git libncurses-dev

```

### 2. 启动 QEMU

```bash
# 编译完 BSP 后
./scripts/run_qemu.sh

```

---

## ⚖️ License

GPL v2
