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
├── .gitignore              # [配置] Git 忽略规则
├── Makefile                # [构建] 顶层指挥官 Makefile
├── README.md               # [文档] 项目主页
│
├── buildroot/              # [第三方] Buildroot 源码 (建议作为子模块或独立目录)
│   ├── output/             # [垃圾] 编译产物 (被 gitignore)
│   └── ...
│
├── bsp/                    # [板级支持] Board Support Package
│   ├── configs/            # [配置] 你的 defconfig (如 my_qemu_defconfig)
│   └── board/
│       └── qemu_x86_64/
│           ├── rootfs_overlay/   # [关键] 你的 Overlay 目录
│           │   ├── etc/init.d/S40modules # 启动程序自动加载驱动
                ├── lib/modules/6.1.44/extra/ # 编译后的驱动放在这里
│           │   └── root/        # 测试程序将部署到这里
│           └── post-build.sh     # (可选) 构建后钩子
│
├── driver/                 # [内核态] 驱动源码
│   ├── pcie_edu.c
│   ├── pcie_edu.h
│   └── Makefile            # 驱动编译脚本
│
├── user_app/               # [用户态] 测试程序 (P4 阶段重点)
│   ├── test_rw.c           # C 语言读写测试
│   └── benchmark.py        # Python 性能测试
│
├── scripts/                # [工具] 辅助脚本
│   ├── run_qemu.sh         # QEMU 启动命令封装
│   └── load_driver.sh      # (可选) 手动调试用，自动加载已由 S40 完成
│
└── docs/                   # [文档]
    ├── edu_datasheet.txt
    └── dev_notes.md

```

## 🛠️ 技术栈 (Tech Stack)

* **Kernel:** Linux 6.1.44 LTS
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
* [x] **2026-01-31**: 部署 Buildroot 开发环境，配置 `.gitignore` 规则。

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
* [x] **2026-02-04**: 实现 PCI 探测框架与自动化加载。
+ 定义 pci_device_id 过滤表，精准匹配 QEMU EDU 设备 (1234:11e8)。
+ 注册 pci_driver 结构体，实现 probe 与 remove 回调接口。
+ 引入 MODULE_DEVICE_TABLE 导出二进制别名。对齐目标机内核版本 (6.1.44)，利用宿主机 depmod 离线预生成 modules.alias 索引表。
+ 启动脚本实现冷插拔识别，在 /etc/init.d/S40modules 中部署了基于 find 与 xargs 的自动化扫描逻辑。
+ 里程碑：通过 dmesg 确认驱动与硬件自动匹配成功，能自动加载驱动模块，无需手动 insmod。

### P3: 硬件资源映射与用户接口构建

* [x] **2026-02-05**: 完成PCI探测函数，实现硬件资源MMIO映射。
+ 资源申请: 使用 pci_enable_device 激活设备，调用 pci_request_regions 独占 BAR 空间资源。
+ 地址映射: 通过 pci_iomap (BAR0) 将物理地址映射为内核虚拟地址 (void __iomem *)。
+ 内核验证: 在 Probe 阶段通过 ioread32 成功读取硬件 ID (0x123411e8) 及完成阶乘计算测试。
* [x] **2026-02-05**: 字符设备子系统集成。
+ 接口绑定: 初始化 cdev 结构体，通过 file_operations 挂载 open/read 接口，完成驱动逻辑与 VFS 的对接。
+ 节点自动化: 引入 class_create 与 device_create自动创建设备节点inode，无需手动mknod。
* [x] **2026-02-06**: 核心 I/O 实现与跨空间数据搬运。
+ 空间交互: 完成 copy_to_user (内核->用户) 与 copy_from_user (用户->内核) 的逻辑实现，打通数据传输通道。
+ 读写逻辑: 完善 edu_read (读取硬件 ID) 与 edu_write (写入阶乘寄存器) 接口，暂时与功能绑定，不使用off偏移量。
+ 应用测试: 编写并运行用户态测试脚本 (test_rw)，成功验证了 App 对底层硬件寄存器的读写控制。
* [x] **2026-02-06**: 构建系统调试与部署策略修正。
+ 问题描述: 遭遇严重的“幽灵更新”问题——修改驱动代码并重新打包后，QEMU 运行的依然是旧版逻辑，但通过ls查看 target/ 下的文件时间戳却显示最新。
+ 排查过程: 通过 md5sum 对比哈希值，发现系统自动加载的是 /lib/modules 下的旧驱动，而构建脚本只更新了 /root 下的新驱动，导致modprobe无法加载最新驱动。
+ 解决方案: 更改Makefile，将编译完成的驱动移动至rootfs_overlay/lib/modules 下。

### P4: 阻塞式 I/O 与中断处理实现

* [x] **2026-02-06**: 解耦硬件实例与驱动逻辑，面向对象重构
+ 数据封装：创建 pcie_edu.h，将 MMIO 基地址、cdev、pdev 等核心成员封装进 struct edu_device。
+ 动态生命周期：放弃全局变量，在 probe 中使用 kzalloc 动态分配实例内存，在 remove 中通过 kfree 回收，修复了潜在的内存泄漏与多设备冲突风险。
+ 上下文纽带：通过 pci_set_drvdata 建立硬件与实例的绑定；在 open 阶段利用 container_of 逆向寻址，并通过 file->private_data 实现文件操作流的实例跟踪。
+ 意义：完成了从“单例驱动”向“工业级多实例驱动”的跨越，为 wait_queue 的植入提供了合法的内存宿主。
* [x] **2026-02-07**: 中断处理 (ISR) 与异步通知机制。
+ 硬件协议修正: 查阅 QEMU 官方文档，修正寄存器定义偏差。确认 Status Register 为 0x20，Interrupt ACK Register 为 0x64（Write Only）。
+ ISR 实现: 实现 edu_isr 底半部处理。使用 ioread32 识别中断源，使用 iowrite32 向 ACK 寄存器写入对应位以清除中断，防止中断风暴。
+ 阻塞机制: 引入 wait_queue_head_t。在 read 接口中使用 wait_event_interruptible 替代轮询，实现进程在无数据时的睡眠等待（TASK_INTERRUPTIBLE）。
* [x] **2026-02-07**: 死锁修复与 I/O 模型增强。
+ 应用层死锁分析: 发现测试程序先 Read 后 Write 导致的逻辑死锁（Read 阻塞等待 Write 触发的数据，而 Write 永远无法执行），根源在于驱动将所有 Read 操作均视为阻塞的阶乘结果读取。
+ 地址分发: 重构 read 接口，引入基于 loff_t 的地址路由机制 (switch-case)。实现 0x00 (ID) 非阻塞读取与 0x08 (Factorial) 阻塞等待的任务分离。
+ 原子化访问验证: 测试端引入 pread 系统调用，替代了非原子的 lseek + read 组合，消除了多线程下的文件指针竞态风险，成功验证了驱动对随机标准的兼容性。
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
