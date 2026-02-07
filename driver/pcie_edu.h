#ifndef _EDU_DRIVER_H_
#define _EDU_DRIVER_H_

#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/wait.h>

// --- 硬件规格常量 ---
#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8

// --- 寄存器偏移 (EDU Spec) ---
#define EDU_REG_ID          0x00
#define EDU_REG_ALIVE       0x04
#define EDU_REG_FACTORIAL   0x08
#define EDU_REG_STATUS      0x20  // 中断状态/使能
#define EDU_REG_IRQ_ACK     0x24  // 中断应答/清除

// --- 驱动配置 ---
#define DRIVER_NAME "edu_driver"
#define CLASS_NAME  "edu_class"

// --- 私有设备结构体 ---
//动态的、属于特定硬件实例的属性
struct edu_device {
    struct pci_dev *pdev;          // 指向 PCI 设备实例
    void __iomem *mmio_base;       // 映射后的 MMIO 基地址
    
    dev_t dev_num;                 // 该设备申请到的设备号
    struct cdev cdev;              // 字符设备对象
    struct class *class;           // 设备类
    struct device *device;         // 设备节点

    // P4 核心：中断与异步通知
    wait_queue_head_t wait_q;      // 进程睡眠的“宿舍”
    int fact_ready;                // 条件标记：1 表示阶乘算完了
    atomic_t status;               // 设备状态（可选）
};

#endif