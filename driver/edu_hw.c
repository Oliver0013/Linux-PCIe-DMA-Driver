#include <linux/io.h>        // 提供 ioread32, iowrite32, writeq 等底层 IO 操作
#include <linux/interrupt.h> // 提供 irqreturn_t, IRQ_HANDLED 等中断宏
#include <linux/wait.h>      // 提供 wake_up_interruptible, wait_event_interruptible
#include "pcie_edu.h"        // 引入我们的核心头文件

// ========================================================
// ⚡ 中断服务程序 (ISR) - 硬件主动呼叫内核的入口
// ========================================================

// 🚨 关键修改：去掉了 static，供 edu_main.c 中的 request_irq 注册使用
irqreturn_t edu_isr(int irq, void *dev) {
    struct edu_device *edu = (struct edu_device *)dev;
    u32 int_status;
    irqreturn_t ret = IRQ_NONE; // 默认返回未处理

    // 查询中断原因寄存器
    int_status = ioread32(edu->mmio_base + EDU_REG_INT_STATUS);
    if (!int_status) return IRQ_NONE;

    // 1. 检查是否是阶乘中断
    if (int_status & INT_STATUS_FACT) {
        iowrite32(INT_STATUS_FACT, edu->mmio_base + EDU_REG_INT_ACK); // ACK 清除中断
        atomic_set(&edu->fact_ready, 1);
        wake_up_interruptible(&edu->wait_q); // 唤醒正在休眠的进程
        ret = IRQ_HANDLED;
    }

    // 2. 检查是否是 DMA 中断
    if (int_status & INT_STATUS_DMA) {
        iowrite32(INT_STATUS_DMA, edu->mmio_base + EDU_REG_INT_ACK);
        atomic_set(&edu->dma_ready, 1);
        wake_up_interruptible(&edu->wait_q);
        ret = IRQ_HANDLED;
    }

    return ret;
}

// ========================================================
// 🛠️ 硬件辅助诊断 - DMA 一致性环回自测
// ========================================================

int edu_dma_loopback_test(struct edu_device *edu) {
    u32 test_magic = 0xDEADBEEF;
    u32 *dma_buf = (u32 *)edu->dma_cpu_addr;
    long timeout; // 超时变量

    // 准备阶段：dma缓存区划分
    dma_buf[0] = test_magic; // 发送区
    dma_buf[1] = 0x0;        // 接收区

    // --- 第 1 阶段：主存 -> 硬件SRAM ---
    writeq((u64)edu->dma_bus_addr, edu->mmio_base + EDU_REG_DMA_SRC);
    writeq((u64)EDU_SRAM, edu->mmio_base + EDU_REG_DMA_DST);
    writeq((u64)sizeof(u32), edu->mmio_base + EDU_REG_DMA_CNT);
    
    atomic_set(&edu->dma_ready, 0); 
    writeq((u64)(DMA_CMD_START | DMA_CMD_IRQ_EN), edu->mmio_base + EDU_REG_DMA_CMD);
    
    // 加上 1 秒超时
    timeout = wait_event_interruptible_timeout(edu->wait_q, atomic_read(&edu->dma_ready) == 1, msecs_to_jiffies(1000));
    if (timeout == 0) {
        printk(KERN_ERR "[EDU] POST Error: Loopback Stage 1 (Tx) Timeout!\n");
        return -ETIMEDOUT;
    } else if (timeout < 0) return -ERESTARTSYS;

    // --- 第 2 阶段：硬件SRAM -> 主存 ---
    writeq((u64)EDU_SRAM, edu->mmio_base + EDU_REG_DMA_SRC);
    writeq((u64)(edu->dma_bus_addr + 4), edu->mmio_base + EDU_REG_DMA_DST);
    writeq((u64)sizeof(u32), edu->mmio_base + EDU_REG_DMA_CNT);
    
    atomic_set(&edu->dma_ready, 0); 
    writeq((u64)(DMA_CMD_START | DMA_CMD_IRQ_EN | DMA_CMD_DEV_RAM), edu->mmio_base + EDU_REG_DMA_CMD);
    
    // 同样加上 1 秒超时
    timeout = wait_event_interruptible_timeout(edu->wait_q, atomic_read(&edu->dma_ready) == 1, msecs_to_jiffies(1000));
    if (timeout == 0) {
        printk(KERN_ERR "[EDU] POST Error: Loopback Stage 2 (Rx) Timeout!\n");
        return -ETIMEDOUT;
    } else if (timeout < 0) return -ERESTARTSYS;
    
    // --- 第 3 阶段：数据校验 ---
    if (dma_buf[1] == test_magic) {
        printk(KERN_INFO "[EDU] DMA Loopback Test Passed! Magic: 0x%x\n", dma_buf[1]);
        return 0; // 成功
    } else {
        printk(KERN_ERR "[EDU] DMA Loopback Failed! Expected 0x%x, got 0x%x\n", test_magic, dma_buf[1]);
        return -EIO; // 硬件 I/O 错误
    }            
}