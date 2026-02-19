#include <linux/module.h> // 提供 THIS_MODULE 宏
#include <linux/fs.h>     // 提供 file_operations, inode, file 结构体
#include <linux/uaccess.h>// 提供 copy_to_user, copy_from_user
#include <linux/io.h>     // 提供 ioread32, iowrite32, writeq 等 IO 操作
#include <linux/wait.h>   // 提供 wait_event_interruptible
#include "pcie_edu.h"     // 我们的核心头文件

// ----------- 1. 文件打开操作 ----------
static int edu_open(struct inode *inode, struct file *file) {
    struct edu_device *edu;
    
    // 通过 inode 指向的 i_cdev 逆向寻找出 edu_device 实例的物理地址
    // 这一步非常经典：它使得驱动能够支持多个同样的物理设备
    edu = container_of(inode->i_cdev, struct edu_device, cdev);
    
    // 把设备的实例指针存放在 file 的私有数据里，供 read/write/ioctl 使用
    file->private_data = edu;
    printk(KERN_INFO "[EDU] Device file opened.\n");
    return 0;
}

// ----------- 2. 文件读操作 ----------
static ssize_t edu_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    struct edu_device *edu = file->private_data;
    u32 val;
    unsigned long copy_status;
    
    // 检查硬件还是否存在
    if (!edu->mmio_base) return -EIO;

    // 根据偏移量分发任务
    switch(*off) {
        // 用于验证 CPU 能否直接访问这块内存
        case 0x1000:
            if (edu->dma_cpu_addr) {
                val = *(u32 *)edu->dma_cpu_addr;
                printk(KERN_INFO "[EDU DMA] Read from DMA Buffer: 0x%x\n", val);
            } else {
                val = 0xDEAD0000; // 错误码
            }
            break;
            
        // 验证DMA是否成功把数据写到了SRAM，把它搬回来验证
        case 0x40000:
            if (!edu->dma_cpu_addr) return -ENOMEM;

            // 1. 设置 DMA SRC 为设备的 SRAM 地址
            writeq((u64)EDU_SRAM, edu->mmio_base + EDU_REG_DMA_SRC);
            // 2. 设置 DMA DST 为主机的 DMA 物理总线地址
            writeq((u64)edu->dma_bus_addr, edu->mmio_base + EDU_REG_DMA_DST);
            // 3. 设置搬运大小 (4字节)
            writeq((u64)sizeof(u32), edu->mmio_base + EDU_REG_DMA_CNT);
            
            // 4. 发送开始命令，并改变方向！(从 EDU 到 RAM)
            writeq((u64)(DMA_CMD_START | DMA_CMD_DEV_RAM | DMA_CMD_IRQ_EN), edu->mmio_base + EDU_REG_DMA_CMD);

            // 5. 阻塞等待 DMA 把数据搬回主存
            if (wait_event_interruptible(edu->wait_q, atomic_read(&edu->dma_ready) == 1)){
                return -ERESTARTSYS;
            }
            atomic_set(&edu->dma_ready, 0); // 清除标志位

            // 6. 此时数据已经躺在主机的内存里了，CPU可以直接读
            val = *(u32 *)edu->dma_cpu_addr;
            break;

        default:
            return -EINVAL;
    }

    printk(KERN_INFO "[EDU] Read Offset 0x%llx, Value: 0x%x\n", *off, val);
    
    // 数据从内核区搬到用户区
    copy_status = copy_to_user(buf, &val, sizeof(u32));
    if (copy_status) return -EFAULT;
    
    *off += sizeof(u32);
    return sizeof(u32);
}

// ----------- 3. 文件写操作 ----------
static ssize_t edu_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
    struct edu_device *edu = file->private_data;

    if (len < sizeof(u32)) return -EINVAL;
    if (!edu->mmio_base) return -EIO;

    switch (*off) {
        // --- dma 正向传输 ---
        case 0x2000:
            if (!edu->dma_cpu_addr) return -ENOMEM;
            if (copy_from_user(edu->dma_cpu_addr, buf, sizeof(u32))) return -EFAULT;

            // 配置DMA相关寄存器
            writeq((u64)edu->dma_bus_addr, edu->mmio_base + EDU_REG_DMA_SRC);
            writeq((u64)EDU_SRAM, edu->mmio_base + EDU_REG_DMA_DST);
            writeq((u64)sizeof(u32), edu->mmio_base + EDU_REG_DMA_CNT);
            writeq((u64)(DMA_CMD_START | DMA_CMD_IRQ_EN), edu->mmio_base + EDU_REG_DMA_CMD);

            // 阻塞等待
            if (wait_event_interruptible(edu->wait_q, atomic_read(&edu->dma_ready) == 1)){
                return -ERESTARTSYS;
            }
            atomic_set(&edu->dma_ready, 0);                 
            break;

        default:
            return -EINVAL;
    }
    return sizeof(u32);
}

// ----------- 4. 文件 IOCTL 控制操作 ----------
static long edu_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    struct edu_device *edu = file->private_data;
    if (!edu->mmio_base) return -EIO;

    switch (cmd){
        case EDU_IOC_GET_ID:
        {
            u32 id_val = ioread32(edu->mmio_base + EDU_REG_ID);
            if (copy_to_user((u32 __user *)arg, &id_val, sizeof(id_val))) return -EFAULT;
            break;
        }

        case EDU_IOC_CALC_FACT:
        { 
            struct edu_fact_req req;
            if (copy_from_user(&req, (struct edu_fact_req __user *)arg, sizeof(req))) return -EFAULT;

            //加锁独占硬件的阶乘计算单元
            mutex_lock(&edu->hw_lock);

            iowrite32(STATUS_IRQ_EN, edu->mmio_base + EDU_REG_STATUS);
            atomic_set(&edu->fact_ready, 0);
            iowrite32(req.val, edu->mmio_base + EDU_REG_FACTORIAL);

            if (wait_event_interruptible(edu->wait_q, atomic_read(&edu->fact_ready) == 1)) {
                mutex_unlock(&edu->hw_lock);//异常解锁
                return -ERESTARTSYS;
            }
            atomic_set(&edu->fact_ready, 0);

            req.result = ioread32(edu->mmio_base + EDU_REG_FACTORIAL);

            // 数据读取完毕，正常解锁
            mutex_unlock(&edu->hw_lock);

            if (copy_to_user((struct edu_fact_req __user *)arg, &req, sizeof(req))) return -EFAULT;
            break; 
        }
        
        case EDU_IOC_DMA_LOOPBACK:
            int ret;
            // 【加锁】独占 DMA 引擎。因为环回测试会修改 SRC/DST 和 CMD 寄存器
            mutex_lock(&edu->hw_lock);
            ret = edu_dma_loopback_test(edu);
            mutex_unlock(&edu->hw_lock);
            return ret;

        default:
            return -ENOTTY;              
    }
    return 0;  
}

// ----------- 5. 导出 file_operations 结构体 ----------
const struct file_operations edu_fops = {
    .owner = THIS_MODULE,
    .open = edu_open,
    .read = edu_read,
    .write = edu_write,
    .unlocked_ioctl = edu_ioctl,
};