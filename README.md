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
|   ├── Makefile
|   ├── edu_main.c     # [总线层] 模块初始化与 PCI 子系统对接
|   ├── edu_cdev.c     # [用户层] 字符设备 VFS 接口 (fops)
|   ├── edu_hw.c       # [硬件层] 硬件控制、中断、DMA 操作
|   ├── pcie_edu.h     # [内核侧头文件] 私有结构体与跨文件函数声明
|   └── uapi_edu.h     # [用户侧头文件] ioctl 宏与共享结构
│
├── user_app/               # [用户态] 测试程序
│   ├── test_ioctl.c        # 控制面测试
│   ├── test_dma_stream.c   # dma读写测试
|   ├── stress_test.c       # 并发压力测试
│   └── benchmark.py        # Python 性能测试
│
├── scripts/                # [工具] 辅助脚本
│   └── run_qemu.sh         # QEMU 启动命令封装
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

### P5: DMA核心机制实现与双向数据流打通

* [x] **2026-02-15**: 一致性 DMA 内存管理与物理寻址。
  * **寻址越界规避**: 及时修正了初期习惯性使用的 32位 DMA 掩码。通过深挖设备手册，改用 `dma_set_mask_and_coherent(..., DMA_BIT_MASK(28))` 严格对齐硬件限制，从根源上杜绝了高位地址截断导致的静默内存踩踏风险。
  * 共享内存分配: 使用 `dma_alloc_coherent` 成功申请 CPU 与外设共享的一致性物理内存区，安全获取内核虚拟地址与总线物理地址 (`dma_bus_addr`)，打通主存与总线的数据桥梁。

* [x] **2026-02-15**: 突破硬件位宽限制与总线异常排查。
  * 隐蔽硬件异常排查: 测试中发现 DMA 命令静默失效。经底层分析与文档溯源，确认为 QEMU 对偏移量 `>= 0x80` 的寄存器存在严格的 64 位 (8-byte) 访问拦截。
  * 接口重构与兼容性保障: 将涉及 DMA 的 `iowrite32` 全局替换为标准 `writeq`，成功唤醒底层 DMA 控制器。

* [x] **2026-02-15**: 双向 DMA 流控与轮询同步 (Busy-Waiting)。
  * 正向传输 (RAM -> SRAM): 成功通过配置 SRC/DST 寄存器，实现 payload 向设备内部 SRAM (0x40000) 的一键下发。
  * 逆向回读与内存屏障: 验证阶段发现 CPU 缺乏权限直接 MMIO 访问设备 SRAM (强读返回 `0xffffffff`)。重构验证逻辑，利用“逆向 DMA (SRAM -> RAM)”将其搬回一致性缓冲区再由 CPU 宣读，完美规避硬件隔离限制。
  * 并发同步机制: 利用 `readq` 监控 DMA_CMD 最低位状态，并结合 `cpu_relax()` 宏实现优雅的忙等待 (Polling) 轮询，有效防止了 CPU 功耗异常与流水线饥饿，实现 payload `0x8899aabb` 的 100% 无损传输。

### P6: 驱动接口规范化与 UAPI 重构

* [x] **2026-02-15**: 控制通路与数据通路分离。
  * 接口重构: 废弃原先基于 `read/write` 偏移量下发控制指令的非标实现。引入 `unlocked_ioctl` 接口，专门负责处理设备查询 (Get ID) 与计算触发 (Calc Factorial) 等控制流业务。
  * 命令号标准化: 严格使用 `<linux/ioctl.h>` 提供的 `_IOR` / `_IOWR` 宏生成 32 位命令号。将读写方向、载荷大小 (Size) 与设备魔数 ('E') 编码至命令字中，防止越权访问及内核栈溢出。

* [x] **2026-02-15**: UAPI (User API) 头文件分离与类型对齐。
  * 宏隔离: 重构 `pcie_edu.h`，引入 `#ifdef __KERNEL__` 预处理指令，将 `struct edu_device` 等内核私有结构与用户态可见的 `ioctl` 宏完全隔离，规范化对外接口。
  * 跨界类型修复: 将共享结构体中的数据类型统一替换为内核标准暴露类型 `__u32`，彻底解决目标机应用层 (`uint32_t`) 与内核侧 (`u32`) 包含不同标准库导致的编译冲突问题

### P7: 一致性 DMA 策略与硬件环回测试机制

* [x] **2026-02-17**: 完善模块依赖与动态节点管理。
  + 依赖构建: 优化 Makefile 部署逻辑，利用宿主机 `depmod` 工具在根文件系统离线生成 `modules.alias` 与 `modules.dep`，避免目标板开机扫描带来的启动延迟。
  + 节点生成: 依托内核 `devtmpfs` 与 `mdev` 机制，通过挂载系统 `Uevent` 热插拔事件，实现驱动加载后 `/dev/edu_driver` 设备节点的自动注册与动态生成。

* [x] **2026-02-17**: 明确 DMA 内存管理策略 (控制流分离)。
  + 机制选型: 针对高频、小数据量的状态交互与自检场景，保留并优化一致性 DMA (`dma_alloc_coherent`) 方案，避免了频繁调用流式 DMA 带来的 IOMMU 页表修改与 Cache 同步 (Cache Coherency) 刷写开销。
  + ioctl 重构: 针对无载荷控制流，使用标准的无数据交互宏 `_IO('E', 3)` 定义 `EDU_IOC_DMA_LOOPBACK` 命令，将测试逻辑完全收敛于内核态，简化了用户态系统调用的参数传递。

* [x] **2026-02-17**: 实现硬件 DMA 环回测试 (Loopback Test) 与 Probe 状态校验。
  + 环回测试闭环: 提取 `edu_dma_loopback_test` 辅助函数。利用一致性内存作为反弹缓冲区 (Bounce Buffer)，由 CPU 写入 Magic Number (`0xDEADBEEF`)，触发硬件 DMA 引擎将数据搬入 SRAM 并原路回写。通过 CPU 最终比对，全链路验证 PCIe 设备的 Bus Master (总线主控) 能力与 DMA 通道状态。
  + 设备上线前校验: 将 DMA 环回测试集成至 `edu_probe` 阶段（位于 IRQ 注册之后，cdev 暴露之前）。作为设备初始化的一部分，若校验失败则立即返回错误码 `-EIO` 并执行异常处理 (goto) 级联释放资源，避免 DMA 引擎异常的设备暴露给应用层，提升驱动健壮性。

### P8: 驱动模块化拆分与代码重构
* [x] **2026-02-18**: UAPI 头文件与内核私有结构隔离。
  + 物理拆分头文件：移除 #ifdef __KERNEL__ 宏，将用户态系统调用契约（ioctl 宏、共享结构体）剥离至独立的 uapi_edu.h。
  + 内核结构收敛：硬件物理偏移量及内核私有 struct edu_device 统一封装于 pcie_edu.h，实现严格的跨空间视图隔离。

* [x] **2026-02-18**: 核心逻辑解耦与多文件架构剥离。
  + 按子系统职责将单文件切分为三层：edu_main.c (PCI总线探测与生命周期)、edu_cdev.c (VFS文件操作)、edu_hw.c (底层硬件I/O与中断)。
  + 修正跨文件编译边界：在公共头文件中引入 extern 声明，调整 edu_fops 与 edu_isr 等核心符号的可见性。

* [x] **2026-02-18**: Kbuild 构建系统多文件链接适配。
  + 更改驱动 Makefile，引入 pcie_edu-y 多目标链接规则，实现多个独立编译单元 (.o) 至单一内核模块 (.ko) 的构建对接，并经上板测试确认逻辑无退化。

### P9: 并发控制与多实例架构

* [x] **2026-02-19**: 互斥锁与临界区保护。
  + 机制：在 `struct edu_device` 中引入 `mutex`，严格保护硬件寄存器读写与状态机流转的临界区。
  + 成果：解决多进程并发 `ioctl` 导致的硬件竞态，并完善了 `-ERESTARTSYS` 信号中断的异常解锁逻辑。

* [x] **2026-02-19**: 上下文同步与原子变量。
  + 机制：引入 `atomic_t` 替换原有的普通整型标志位 (`fact_ready`, `dma_ready`)。
  + 成果：依托底层硬件原子指令，实现进程上下文与中断上下文之间的无锁同步，杜绝 ISR 误用锁休眠导致的内核 Panic。

* [x] **2026-02-19**: IDA 机制与多实例重构。
  + 机制：引入内核 IDA动态分配/回收次设备号。
  + 架构修正：修复 LDM 生命周期错位，将 `class_create` 提升至模块级 `dev_init` 全局挂载，保留 `device_create` 在 `probe` 阶段执行。
  + 成果：彻底打通多物理 PCIe 设备同载，成功实现 `/dev/edu_driver0` 与 `/dev/edu_driver1` 独立内存上下文隔离。

* [x] **2026-02-19**: 多线程混沌压测与构建优化。
  + 构建：重构 `user_app/Makefile`，引入自动化模式规则 (`% : %.c`) 与 `-pthread` 链接支持。
  + 验证：编写多线程压测工具，利用 `sched_yield()` 主动诱发 CPU 调度抢占。在双卡环境下经受百万级并发请求测试，零数据损坏，验证并发控制策略有效。

### P10: 现代中断架构升级与 BSP 配置固化

* [x] **2026-02-20**: MSI/MSI-X 现代中断架构迁移。
  + 机制：弃用 Legacy INTx 共享中断 (`pdev->irq` & `IRQF_SHARED`)，全面引入 `pci_alloc_irq_vectors` API。
  + 兼容性：传入 `PCI_IRQ_ALL_TYPES` 标志位，实现硬件能力的自动降级匹配 (MSI-X -> MSI -> INTx)，保障驱动高可用性。
  + 验证：查阅 `/proc/interrupts`，确认中断路由成功从 `IO-APIC` 切换至设备独占的 `PCI-MSI`。

* [x] **2026-02-20**: 内核能力重构与 BSP 规范化固化。
  + 内核支持：通过 `menuconfig` 重新开启 `CONFIG_PCI_MSI` 选项，修复极限裁剪导致的中断分配失败 (`-ENOSPC`)。
  + 资产固化：遵循工业级 BSP 维护规范，执行 `make linux-savedefconfig`，将内核配置提取至 `bsp/board/qemu_x86_64/linux_defconfig`。
  + 构建闭环：更新 Buildroot 寻址规则，并导出顶级配置至 `bsp/configs/my_qemu_defconfig`，确保跨机器环境的一键级 100% 复现。

### P11: 流式 DMA 引入与 I/O 架构重构
* [x] **2026-02-20**: 双轨制 DMA 架构与死锁规避。
  + 数据通路重构: 引入流式 DMA (dma_map_single/unmap_single) 处理 read/write 大块载荷，利用 CPU Cache 提升吞吐量；保留一致性 DMA (dma_alloc_coherent) 专职控制流与硬件自测，实现控制平面与数据平面分离。
  + I/O 超时容错: 将底层同步机制全面替换为 wait_event_interruptible_timeout，防止因硬件状态机异常或中断丢失导致内核线程永久阻塞 (Deadlock)。

* [x] **2026-02-20**: VFS 语义对齐与硬件 Quirk 规避。
  + 随机寻址契约: 显式挂载 .llseek = default_llseek 解除内核对字符设备的默认寻址拦截 (-ESPIPE)，并将文件游标 (loff_t) 严格映射为 SRAM 物理相对偏移，实现任意位置的随机存取 (Random Access)。
  + 边界截断保护 (Quirk): 针对 QEMU EDU DMA 引擎的 Off-by-one 硬件缺陷（addr + size > 0x40fff 触发 abort），在驱动层将可用容量钳制为 4095 Bytes，并在 VFS 读写接口实现严格的越界截断逻辑 (Boundary Truncation)。

* [x] **2026-02-20**: 测试套件重构。
  + 用例解耦: 将用户态测试验证拆分为 test_ioctl (控制平面验证) 与 test_dma_stream (数据平面验证)。
  + 完整性校验: 补充流式 DMA 满载收发的数据一致性比对、lseek 随机定位校验以及 VFS 边界截断触发测试，严格处理并断言系统调用返回值。

### P12: 并发控制优化与异常处理重构

* [x] **2026-02-21**: 引入细粒度锁 (Fine-grained Locking) 机制。
  + 架构重构: 废弃全局硬件单例锁 (`hw_lock`)，按物理模块独立性拆分为 DMA 引擎专有锁 (`dma_mutex`) 与阶乘单元专有锁 (`fact_mutex`)。
  + 性能收益: 实现控制平面与数据平面的并发隔离，消除无状态关联模块间的强制串行化，提升整体 I/O 吞吐量。

* [x] **2026-02-21**: 修复 TASK_UNINTERRUPTIBLE (D状态) 死锁隐患。
  + 机制替换: 在 VFS 接口层 (`read/write/ioctl`) 将不可中断的 `mutex_lock` 统一替换为 `mutex_lock_interruptible`。
  + 异常响应: 允许阻塞等待硬件资源的进程响应 `SIGINT` 等异步信号，避免因底层故障导致进程僵死，合规拦截并向 VFS 返回 `-ERESTARTSYS`。

* [x] **2026-02-21**: 确立基于同步请求模型的极简并发架构。
  + 竞态排除: 经时序分析确认 ISR 仅访问独立的 Status/ACK 寄存器，不干预 DMA 配置，故在此场景中排除 `spin_lock_irq` 关本地中断的需求，降低中断延迟。
  + 调度定型: 针对 EDU 设备的同步“请求-响应”特性，直接复用被唤醒的用户进程上下文处理 VFS 层的数据解映射与拷贝，避免强行引入 Workqueue/NAPI 造成的上下文切换开销。

---

## 🚀 快速开始 (Quick Start)

本项目采用**树外编译 (Out-of-Tree)** 架构，环境分为两部分：基础系统 (BSP) 与驱动程序 (Driver)。请按以下步骤依次构建。

### 1. 安装环境依赖

首先，在你的宿主机（推荐 Ubuntu 20.04/22.04）上安装必备的交叉编译与虚拟化工具链：

```bash
sudo apt-get update
sudo apt-get install -y build-essential qemu-system-x86 git libncurses-dev wget cpio
```

### 2. 克隆仓库
```bash
git clone git@github.com:Oliver0013/Linux-PCIe-DMA-Driver.git
cd Linux-PCIe-DMA-Driver
```

### 3. 构建基础系统 (BSP) 与内核
由于 Buildroot 体积较大，本项目未将其包含在版本库中。我们通过预设的 defconfig 实现一键自动化部署：

```bash
# 下载指定版本的 Buildroot (2024.02.9 LTS)
wget [https://buildroot.org/downloads/buildroot-2024.02.9.tar.gz](https://buildroot.org/downloads/buildroot-2024.02.9.tar.gz)
tar -zxvf buildroot-2024.02.9.tar.gz

# 重命名目录以匹配 Makefile 路径
mv buildroot-2024.02.9 buildroot
cd buildroot

# 载入本项目专属的极限裁剪配置 (内核压缩至 4.4MB)
make defconfig BR2_DEFCONFIG=../bsp/configs/my_qemu_defconfig

# 启动全量编译
# ⚠️ 注意：GCC 编译时极其消耗内存。如果虚拟机内存 <= 4GB，请使用 make -j2 防止触发 OOM (Out Of Memory) 杀手！内存充足可使用 make -j$(nproc)。
make -j2
cd ..
```

### 4. 编译 PCIe 驱动与测试程序
依托于完善的顶层 Makefile，BSP 编译完成后，只需在项目根目录执行一条命令，即可自动完成驱动与测试程序的编译，并通过 Overlay 机制打包进根文件系统：

```bash
# 在项目根目录执行
make all
```

### 5. 启动 QEMU 仿真与上板测试
```bash
# 启动带有 EDU 硬件参数的 QEMU 环境
make run
```

---

## ⚖️ License

GPL v2
