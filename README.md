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

### P0: 环境准备与预研 (Preparation)

* [x] **2026-01-30**: 初始化 GitHub 仓库，建立符合工业规范的目录结构。
* [ ] **2026-01-31**: 下载 Linux 5.15 和 Buildroot 源码，配置 `.gitignore` 规则。

### P1: BSP 构建 (System Build)

* [ ] **2026-02-02**: Buildroot 配置 (`qemu_x86_64_defconfig`)，尝试初次编译。
* [ ] **2026-02-04**: 编写 `run_qemu.sh`，实现一键启动并验证 PCI 设备扫描。

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
