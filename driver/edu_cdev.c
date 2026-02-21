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
    void *kbuf;
    dma_addr_t dma_handle;
    long timeout;
    ssize_t ret_len = 0;

    // 1. 严格的 VFS 边界检查
    if (*off >= EDU_SRAM_SIZE) return 0; // 读到 EOF
    if (*off + len > EDU_SRAM_SIZE) {
        len = EDU_SRAM_SIZE - *off;
    }
    if (len == 0) return 0;
    //内存分配
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    // 2. 流式映射 (方向：设备将向主存写入数据)
    dma_handle = dma_map_single(&edu->pdev->dev, kbuf, len, DMA_FROM_DEVICE);
    if (dma_mapping_error(&edu->pdev->dev, dma_handle)) {
        ret_len = -ENOMEM;
        goto out_free;
    }

    // 可中断加锁
    // 专门获取 DMA 锁。如果用户按了 Ctrl+C，立刻清理并返回 -ERESTARTSYS
    if (mutex_lock_interruptible(&edu->dma_mutex)) {
        dma_unmap_single(&edu->pdev->dev, dma_handle, len, DMA_FROM_DEVICE);
        kfree(kbuf);
        return -ERESTARTSYS;
    }
    // 临界区
    // 源地址 = 硬件 SRAM 基地址 + 当前的文件读写偏移量
    writeq((u64)(EDU_SRAM + *off), edu->mmio_base + EDU_REG_DMA_SRC);
    writeq((u64)dma_handle, edu->mmio_base + EDU_REG_DMA_DST);
    writeq((u64)len, edu->mmio_base + EDU_REG_DMA_CNT);
    
    atomic_set(&edu->dma_ready, 0); 
    // 注意方向标志：DMA_CMD_DEV_RAM
    writeq((u64)(DMA_CMD_START | DMA_CMD_DEV_RAM | DMA_CMD_IRQ_EN), edu->mmio_base + EDU_REG_DMA_CMD);

    // 【超时保护】挂起等待
    timeout = wait_event_interruptible_timeout(edu->wait_q, atomic_read(&edu->dma_ready) == 1, msecs_to_jiffies(1000));

    // 安全解锁
    mutex_unlock(&edu->dma_mutex);

    // 5. 解除流式映射 (核心：内核在此处 Invalidate CPU Cache)
    dma_unmap_single(&edu->pdev->dev, dma_handle, len, DMA_FROM_DEVICE);

    // 6. 错误处理与用户态拷贝
    if (timeout == 0) {
        printk(KERN_ERR "[EDU] DMA Read Timeout at offset 0x%llx!\n", *off);
        ret_len = -ETIMEDOUT;
    } else if (timeout < 0) {
        ret_len = -ERESTARTSYS;
    } else {
        // 只有硬件成功完成搬运，才将数据拷贝给用户
        if (copy_to_user(buf, kbuf, len)) {
            ret_len = -EFAULT;
        } else {
            *off += len; // 成功读取，推进文件指针
            ret_len = len;
        }
    }

out_free:
    kfree(kbuf);
    return ret_len;
}

// ----------- 3. 文件写操作 ----------
static ssize_t edu_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    struct edu_device *edu = file->private_data;
    void *kbuf;
    dma_addr_t dma_handle;
    long timeout;
    ssize_t ret_len = 0;

    // 1. 严格的 VFS 边界检查
    if (*off >= EDU_SRAM_SIZE) return -ENOSPC; // 文件指针已到末尾
    if (*off + len > EDU_SRAM_SIZE) {
        len = EDU_SRAM_SIZE - *off; // 截断超出的部分，防止 DMA 越界写坏硬件寄存器
    }
    if (len == 0) return 0;

    // 2. 分配 Cacheable 的普通内核内存作为流式 DMA 跳板
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, len)) {
        ret_len = -EFAULT;
        goto out_free;
    }

    // 3. 流式映射 (核心：自动处理 Cache 刷写，将数据同步到主存供 DMA 读取)
    dma_handle = dma_map_single(&edu->pdev->dev, kbuf, len, DMA_TO_DEVICE);
    if (dma_mapping_error(&edu->pdev->dev, dma_handle)) {
        ret_len = -ENOMEM;
        goto out_free;
    }

    // 4. 【加锁】进入 DMA 硬件配置临界区
    if (mutex_lock_interruptible(&edu->dma_mutex)) {
        dma_unmap_single(&edu->pdev->dev, dma_handle, len, DMA_TO_DEVICE);
        kfree(kbuf);
        return -ERESTARTSYS;
    }

    // 目标地址 = 硬件 SRAM 基地址 + 当前的文件读写偏移量
    writeq((u64)dma_handle, edu->mmio_base + EDU_REG_DMA_SRC);
    writeq((u64)(EDU_SRAM + *off), edu->mmio_base + EDU_REG_DMA_DST);
    writeq((u64)len, edu->mmio_base + EDU_REG_DMA_CNT);
    
    atomic_set(&edu->dma_ready, 0); 
    writeq((u64)(DMA_CMD_START | DMA_CMD_IRQ_EN), edu->mmio_base + EDU_REG_DMA_CMD);

    // 5. 【超时保护】挂起等待硬件中断，最多等 1000 毫秒
    timeout = wait_event_interruptible_timeout(edu->wait_q, atomic_read(&edu->dma_ready) == 1, msecs_to_jiffies(1000));

    // 6. 退出临界区
    mutex_unlock(&edu->dma_mutex);

    // 7. 解除流式映射 (释放总线地址)
    dma_unmap_single(&edu->pdev->dev, dma_handle, len, DMA_TO_DEVICE);

    // 8. 错误处理与 VFS 语义维护
    if (timeout == 0) {
        printk(KERN_ERR "[EDU] DMA Write Timeout at offset 0x%llx!\n", *off);
        ret_len = -ETIMEDOUT;
    } else if (timeout < 0) {
        ret_len = -ERESTARTSYS;
    } else {
        *off += len; // 成功写入，推进文件指针
        ret_len = len;
    }

out_free:
    kfree(kbuf);
    return ret_len;
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
            long timeout; // 超时变量

            if (copy_from_user(&req, (struct edu_fact_req __user *)arg, sizeof(req))) return -EFAULT;

            //加锁独占硬件的阶乘计算单元
            if (mutex_lock_interruptible(&edu->fact_mutex))
                return -ERESTARTSYS;

            iowrite32(STATUS_IRQ_EN, edu->mmio_base + EDU_REG_STATUS);
            atomic_set(&edu->fact_ready, 0);
            iowrite32(req.val, edu->mmio_base + EDU_REG_FACTORIAL);

            // 替换为带超时机制的等待，比如给它 500 毫秒
            timeout = wait_event_interruptible_timeout(edu->wait_q, atomic_read(&edu->fact_ready) == 1, msecs_to_jiffies(500));

            if (timeout == 0) {
                printk(KERN_ERR "[EDU] HW Fault: Factorial calculation timed out!\n");
                mutex_unlock(&edu->fact_mutex);
                return -ETIMEDOUT; // 返回标准超时错误码
            } else if (timeout < 0) {
                mutex_unlock(&edu->fact_mutex);
                return -ERESTARTSYS; // 被信号打断
            }
            
            atomic_set(&edu->fact_ready, 0);

            // 只有成功了才能去读结果
            req.result = ioread32(edu->mmio_base + EDU_REG_FACTORIAL);

            // 数据读取完毕，正常解锁
            mutex_unlock(&edu->fact_mutex);

            if (copy_to_user((struct edu_fact_req __user *)arg, &req, sizeof(req))) return -EFAULT;
            break; 
        }
        
        case EDU_IOC_DMA_LOOPBACK:
        {
            int ret;
            // 【加锁】独占 DMA 引擎。因为环回测试会修改 SRC/DST 和 CMD 寄存器
            if (mutex_lock_interruptible(&edu->dma_mutex))
                return -ERESTARTSYS;
                
            ret = edu_dma_loopback_test(edu);
            mutex_unlock(&edu->dma_mutex);
            return ret;
        }

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
    .llseek = default_llseek,
};