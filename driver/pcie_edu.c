#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "pcie_edu.h"
#include <linux/wait.h>
//模块信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OLIVER");
MODULE_DESCRIPTION("Qemu EDU PCIe Device Driver");
MODULE_VERSION("0.1");

//宏定义设备名称，出现在/proc/devices里
#define DRIVER_NAME "edu_driver"


//全局变量
static dev_t dev_number;//设备号

//----------中断服务程序ISR------------
//dev就是request_irq传入的最后一个参数
static irqreturn_t edu_isr(int irq, void *dev){
    struct edu_device *edu = (struct edu_device *)dev;
    u32 int_status;

    //查询中断原因寄存器
    int_status = ioread32(edu->mmio_base + EDU_REG_INT_STATUS);
    if (!int_status) return IRQ_NONE;

    //检查是否是阶乘中断
    if (int_status && INT_STATUS_FACT){
        //清除中断
        iowrite32(INT_STATUS_FACT, edu->mmio_base + EDU_REG_INT_ACK);
        edu->fact_ready = 1;
        //唤醒等待队列的进程
        wake_up_interruptible(&edu->wait_q);
        return IRQ_HANDLED;
    }
    return IRQ_NONE;
}



//-----------文件操作函数----------
static int edu_open(struct inode *inode, struct file *file) {
    struct edu_device *edu;
    //edu的地址：通过inode指向的i_cdev对应于edu_device中的cdev字段
    edu = container_of(inode->i_cdev, struct edu_device, cdev);
    //把文件的私有数据指向edu
    file->private_data = edu;
    printk(KERN_INFO "[EDU] Device file opened.\n");
    return 0;
}
//读硬件ID操作
static ssize_t edu_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    struct edu_device *edu = file->private_data;
    u32 val;
    unsigned long copy_status;
    //检查硬件还是否存在
    if (!edu->mmio_base) {
        //硬件错误
        return -EIO;
    }
    //根据偏移量分发任务
    switch(*off) {
        case EDU_REG_ID: //读取ID（非阻塞）
            val = ioread32(edu->mmio_base + EDU_REG_ID);
            break;
        case EDU_REG_FACTORIAL: //读取阶乘结果（阻塞）
            if (wait_event_interruptible(edu->wait_q, edu->fact_ready == 1)){
                return -ERESTARTSYS;
            }
            //重置计算标志位
            edu->fact_ready = 0;
            //读计算完的阶乘
            val = ioread32(edu->mmio_base + EDU_REG_FACTORIAL);
            break;
        default:
            return -EINVAL;
    }

    printk(KERN_INFO "[EDU] Read Offset 0x%llx, Value: 0x%x\n", *off, val);
    //数据从内核区搬到用户区
    copy_status = copy_to_user(buf, &val, sizeof(u32));
    if (copy_status){
        //用户虚拟地址错误
        return -EFAULT;
    }
    //更新文件偏移量，并返回读取的字节数
    *off += sizeof(u32);
    return sizeof(u32);
}
//写操作
static ssize_t edu_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
    struct edu_device *edu = file->private_data;
    u32 user_val;
    unsigned long copy_status;
    //1. 检查传过来的数据长度
    if (len < sizeof(u32)) return -EINVAL;
    //2. 数据从用户区搬到内核区
    copy_status = copy_from_user(&user_val, buf, sizeof(u32));
    printk(KERN_INFO "[EDU] User wrote: %u. Writing to Factorial Reg...\n", user_val);
    //出错则是用户区地址出错
    if (copy_status) return -EFAULT;
    //3. 检查硬件是否存在，不存在返回硬件io错误
    if (!edu) return -EIO;
    //4. 中断使能
    iowrite32(STATUS_IRQ_EN, edu->mmio_base+EDU_REG_STATUS);
    //5. 重置计算标记位
    edu->fact_ready = 0;
    //6. 写入硬件
    iowrite32(user_val, edu->mmio_base + EDU_REG_FACTORIAL);
    //7. 返回写的长度
    return sizeof(u32);

}
//定义文件操作
static struct file_operations edu_fops = {
    .owner = THIS_MODULE,
    .open = edu_open,
    .read = edu_read,
    .write = edu_write,
};


//---------PCI核心逻辑---------
//1. probe函数，检测到硬件自动调用该函数
static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id){
    int ret;

    //0. 创建设备实例
    struct edu_device *edu;
    // 为设备申请堆空间，清空，GFP_KERNEL代表会等待有空间了才分配
    edu = kzalloc(sizeof(struct edu_device), GFP_KERNEL);
    if (!edu) return -ENOMEM;

    //pdev和edu相互关联
    edu->pdev = pdev;
    pci_set_drvdata(pdev, edu);//等价于pdev->dev.driver_data = edu;
    //设备号赋值
    edu->dev_num = dev_number;

    //1. 启动硬件设备/硬件初始化
    ret = pci_enable_device(pdev);
    if (ret) goto err_free;

    //2. 申请BAR资源
    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret) goto err_disable;

    //3. MMIO映射(BAR 0)物理地址映射为虚拟地址
    //pci_iomap(设备指针，BAR序号，映射长度，0代表整个区域)
    edu->mmio_base = pci_iomap(pdev, 0, 0);
    if (!edu->mmio_base) {
        ret = -ENOMEM;
        goto err_regions;
    }

    //3. 中断相关
    //a. 初始化等待队列
    init_waitqueue_head(&edu->wait_q);
    edu->fact_ready = 0;
    //b. 注册中断
    ret = request_irq(pdev->irq, edu_isr, IRQF_SHARED, DRIVER_NAME, edu);
    if (ret) goto err_iounmap;

    
    //4. 初始化注册字符设备
    //初始化cdev结构，与fops关联
    edu->cdev.owner = THIS_MODULE;
    cdev_init(&edu->cdev, &edu_fops);
    //告知内核cdev，并与申请好的设备号绑定
    ret = cdev_add(&edu->cdev, edu->dev_num, 1);
    if (ret) goto err_irq;

    //5. 创建节点inode
    edu->class = class_create(THIS_MODULE, "edu_class");
    if (IS_ERR(edu->class)) {
        ret = PTR_ERR(edu->class);
        goto err_cdev;
    }

    edu->device = device_create(edu->class, NULL, edu->dev_num, NULL, DRIVER_NAME);
    if (IS_ERR(edu->device)) {
        ret = PTR_ERR(edu->device);
        goto err_class;
    }

    printk(KERN_INFO "[EDU DRIVER V2] Probe called. Found device vender: 0x%x Device: 0x%x\n.", id->vendor, id->device);
    return 0;

// --- 错误处理 ---
err_class:
    class_destroy(edu->class);
err_cdev:
    cdev_del(&edu->cdev);
err_irq:
    free_irq(pdev->irq, edu);
err_iounmap:
    pci_iounmap(pdev, edu->mmio_base);
err_regions:
    pci_release_regions(pdev);
err_disable:
    pci_disable_device(pdev);
err_free:
    kfree(edu); // 【关键】释放内存
    return ret;
}

//2. Remove函数
static void edu_remove(struct pci_dev *pdev){
    struct edu_device *edu;
    edu = pci_get_drvdata(pdev);
    if (edu){
        //删除设备节点
        device_destroy(edu->class, edu->dev_num);
        class_destroy(edu->class);
        //删除字符设备
        cdev_del(&edu->cdev);
        //取消中断
        free_irq(pdev->irq, edu);
        //取消虚拟地址映射
        pci_iounmap(pdev, edu->mmio_base);
        //取消BAR资源
        pci_release_regions(pdev);
        //关闭设备
        pci_disable_device(pdev);
        //释放内存
        kfree(edu);       
    }
    printk(KERN_INFO "[EDU_DRIVER] Device removed and unmapped.\n");
}
//3. 驱动支持的设备号表，表内存放的数据结构是struct pci_device_id
static const struct pci_device_id edu_ids[] = {
    {PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID)},
    {0,}
};

//关键：将 ID 表导出到模块符号表，供 depmod 使用
MODULE_DEVICE_TABLE(pci, edu_ids);

//4. PCI驱动
static struct pci_driver edu_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = edu_ids,
    .probe = edu_probe,
    .remove = edu_remove,
};


//模块初始化函数
static int __init dev_init(void){
    //存放申请结果
    int ret;
    //------------申请设备号---------------
    //内核打印正在申请
    printk(KERN_INFO "[EDU_DRIVE] Initializing Module...\n");
    //申请设备号
    //参数1: 设备号；参数2: 次设备号开始编号；参数3: 分配个数；参数4：设备名称
    ret = alloc_chrdev_region(&dev_number, 0, 1, DRIVER_NAME);
    //申请失败
    if (ret < 0){
        printk(KERN_ERR "[EDU_DRIVER] Failed to allocate dev number.\n");
        return ret;
    }
    //申请设备号成功
    printk(KERN_INFO "[EDU_DRIVER] Success! Major: %d, Minor: %d.\n", MAJOR(dev_number), MINOR(dev_number));
    //------------注册驱动----------------
    ret = pci_register_driver(&edu_pci_driver);
    //注册失败
    if (ret <0){
        printk(KERN_ERR "[EDU_DRIVER] Failed to register.\n");
        //释放设备号
        unregister_chrdev_region(dev_number, 1);
        return ret;
    }
    //注册成功
    printk(KERN_INFO "Register Success!");
    return 0;    
}
//退出函数
static void __exit dev_exit(void){
    //释放注册
    pci_unregister_driver(&edu_pci_driver);    
    //释放设备号
    unregister_chrdev_region(dev_number,1);

    printk(KERN_INFO "[EDU_DRIVER] Module Exit.\n");
}
//内核初始化/退出关联函数
module_init(dev_init);
module_exit(dev_exit);