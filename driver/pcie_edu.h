/* pcie_edu.h - Kernel private header for QEMU EDU Driver */
#ifndef _PCIE_EDU_H_
#define _PCIE_EDU_H_

#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/types.h>
// 包含刚才剥离出去的 UAPI 头文件
#include "uapi_edu.h"

#define MAX_EDU_DEVICES 16 //最多支持16个EDU设备
// ========================================================
// 🌟 硬件规格与寄存器定义
// ========================================================
#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

// --- 寄存器映射 (BAR0 Offset) ---
#define EDU_REG_ID          0x00    // IDENTIFICATION (RO)
#define EDU_REG_ALIVE       0x04    // LIVENESS_CHECK (RW)
#define EDU_REG_FACTORIAL   0x08    // FACTORIAL (RW)
#define EDU_REG_STATUS      0x20    // STATUS (RW)
#define EDU_REG_INT_STATUS  0x24    // INT_STATUS (RO) - 读中断原因
#define EDU_REG_INT_RAISE   0x60    // INT_RAISE (WO) - 手动触发中断
#define EDU_REG_INT_ACK     0x64    // INT_ACK (WO) - 写入以清除中断

// DMA 相关寄存器, 64位寄存器
#define EDU_REG_DMA_SRC     0x80    // 配置DMA数据源地址
#define EDU_REG_DMA_DST     0x88    // 配置DMA数据目的地址
#define EDU_REG_DMA_CNT     0x90    // 配置传输长度
#define EDU_REG_DMA_CMD     0x98    // 配置CMD

// --- 寄存器位定义 ---
#define STATUS_BUSY         0x01    // Bit 0: 0x20 寄存器，计算中
#define STATUS_IRQ_EN      0x80     // Bit 7: 0x20 寄存器，开启阶乘完成中断

#define INT_STATUS_FACT     0x01    // Bit 0: 0x24 寄存器，阶乘计算完成中断标志
#define INT_STATUS_DMA      0x100   // Bit 8: 0x24 寄存器，DMA 完成中断标志

#define DMA_CMD_START       0x01    // Bit0: 0x98寄存器，DMA启动
#define DMA_CMD_DEV_RAM     0x02    // Bit1: 0x98寄存器，DMA传输方向从设备到主存
#define DMA_CMD_IRQ_EN      0x04    // Bit3: 0x98寄存器，DMA完成后触发中断

// ---- EDU的SRAM定义---
#define EDU_SRAM           0x40000

// 定义 DMA 缓冲区大小 (4KB = 1页)
#define EDU_DMA_SIZE        0x1000
// 修改为带硬件 Quirk 补丁的定义：
// [Hardware Quirk] QEMU EDU DMA engine has an off-by-one bug checking bounds. 
// It panics if addr + size > 0x40fff. We must clamp the usable SRAM to 4095 bytes.
#define EDU_SRAM_SIZE 0x0FFF

// --- 驱动配置 ---
#define DRIVER_NAME "edu_driver"
#define CLASS_NAME  "edu_class"

// ========================================================
// 🛑 私有设备结构体
// ========================================================
struct edu_device {
    struct pci_dev *pdev;          // 指向 PCI 设备实例
    void __iomem *mmio_base;       // 映射后的 MMIO 基地址
    
    dev_t dev_num;                 // 该设备申请到的设备号
    struct cdev cdev;              // 字符设备对象
    struct device *device;         // 设备节点

    // 中断与异步通知
    wait_queue_head_t wait_q;      // 进程睡眠的“宿舍”
    atomic_t fact_ready;           // 条件标记：1 表示阶乘算完了（原子变量保证读写是原子化的）
    atomic_t dma_ready;            // 条件标记：1 表示DMA传完了
    atomic_t status;               // 设备状态（可选）

    // DMA相关
    void *dma_cpu_addr;            // CPU用的虚拟内核地址（一致性DMA）
    dma_addr_t dma_bus_addr;       // 设备用的总线物理地址

    struct mutex hw_lock;          // 硬件互斥锁
};

// ========================================================
// 🤝 跨文件函数接口声明 (Extern APIs)
// ========================================================

// 从 edu_cdev.c 暴露出来，供 edu_main.c 注册字符设备时使用
extern const struct file_operations edu_fops;

// 从 edu_hw.c 暴露出来，供 edu_main.c 和 edu_cdev.c 调用
irqreturn_t edu_isr(int irq, void *dev);
int edu_dma_loopback_test(struct edu_device *edu);

#endif /* _PCIE_EDU_H_ */