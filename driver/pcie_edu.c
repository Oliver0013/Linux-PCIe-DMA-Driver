#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
//模块信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OLIVER");
MODULE_DESCRIPTION("Qemu EDU PCIe Device Driver");
MODULE_VERSION("0.1");

//宏定义设备名称，出现在/proc/devices里
#define DRIVER_NAME "edu_driver"

//宏定义设备的ID，llspci -v得
#define EDU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8
//宏定义测试用的寄存器偏移
#define EDU_REG_ID 0x00
#define EDU_REG_FACTORIAL 0x08

//全局变量
static dev_t dev_number;//设备号
static struct cdev edu_cdev;//字符设备
static void __iomem *edu_mmio_base;//硬件操作基地址
static struct class *edu_class;
static struct device *edu_device;

//-----------文件操作函数----------
static int edu_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "[EDU] Device file opened.\n");
    return 0;
}
//读硬件ID操作
static ssize_t edu_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    u32 id_val;
    unsigned long copy_status;
    //1. 字符设备模拟EOF
    if (*off > sizeof(u32)) return 0;
    //2. 检查硬件还是否存在
    if (!edu_mmio_base) {
        //硬件错误
        return -EIO;
    }
    //3. 读硬件ID
    id_val = ioread32(edu_mmio_base + EDU_REG_ID);
    printk(KERN_INFO "[EDU] User reading... Hardware ID is 0x%x\n", id_val);
    //4. 数据从内核区搬到用户区
    copy_status = copy_to_user(buf, &id_val, sizeof(id_val));
    if (copy_status){
        //用户虚拟地址错误
        return -EFAULT;
    }
    //5. 更新文件偏移量，并返回读取的字节数
    *off += sizeof(u32);
    return sizeof(u32);
}
//写操作
static ssize_t edu_write(struct file *file, const char __user *buf, size_t len, loff_t *off){
    u32 user_val;
    unsigned long copy_status;
    //*off = 0;
    //1. 检查传过来的数据长度
    if (len < sizeof(u32)) return -EINVAL;
    //2. 数据从用户区搬到内核区
    copy_status = copy_from_user(&user_val, buf, sizeof(u32));
    printk(KERN_INFO "[EDU] User wrote: %u. Writing to Factorial Reg...\n", user_val);
    //出错则是用户区地址出错
    if (copy_status) return -EFAULT;
    //3. 检查硬件是否存在，不存在返回硬件io错误
    if (!edu_mmio_base) return -EIO;
    //4. 写入硬件
    iowrite32(user_val, edu_mmio_base + EDU_REG_FACTORIAL);
    //5. 返回写的长度
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
    u32 id_check;
    //1. 启动硬件设备
    ret = pci_enable_device(pdev);
    if (ret) return ret;
    //2. 申请BAR资源
    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret) {
        printk(KERN_ERR "[EDU] Failed to request PCI regions\n");
        pci_disable_device(pdev);
        return ret;
    }
    //3. MMIO映射(BAR 0)物理地址映射为虚拟地址
    //pci_iomap(设备指针，BAR序号，映射长度，0代表整个区域)
    edu_mmio_base = pci_iomap(pdev, 0, 0);
    
    //【关键验证】直接在 Probe 里测试硬件通信
    id_check = ioread32(edu_mmio_base + 0x00);
    printk(KERN_INFO "[EDU] MMIO Test: Identification Register = 0x%08x\n", id_check);

    // 阶乘测试：往 0x08 写 5
    iowrite32(5, edu_mmio_base + 0x08);
    printk(KERN_INFO "[EDU] MMIO Test: Wrote 5 to Factorial Register. Result: %u\n", 
           ioread32(edu_mmio_base + 0x08));
    //4. 初始化注册字符设备
    //初始化cdev结构，与fops关联
    edu_cdev.owner = THIS_MODULE;
    cdev_init(&edu_cdev, &edu_fops);
    //告知内核cdev，并与申请好的设备号绑定
    ret = cdev_add(&edu_cdev, dev_number, 1);
    if (ret) {
        printk(KERN_ERR "[EDU] Failed to cdev add\n");
        pci_iounmap(pdev, edu_mmio_base);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        return ret;
    }
    //5. 创建节点inode
    edu_class = class_create(THIS_MODULE, "edu_class");
    edu_device = device_create(edu_class,NULL,dev_number,NULL,DRIVER_NAME);

    printk(KERN_INFO "[EDU DRIVER V2] Probe called. Found device vender: 0x%x Device: 0x%x\n.", id->vendor, id->device);
    return 0;
}

//2. Remove函数
static void edu_remove(struct pci_dev *dev){
    //删除设备节点
    device_destroy(edu_class, dev_number);
    class_destroy(edu_class);
    //删除字符设备
    cdev_del(&edu_cdev);
    //1. 取消虚拟地址映射
    if (edu_mmio_base){
        pci_iounmap(dev, edu_mmio_base);
    }
    //2. 取消BAR资源
    pci_release_regions(dev);
    //3. 关闭设备
    pci_disable_device(dev);
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