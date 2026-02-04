#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
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


//全局变量存申请的设备号
static dev_t dev_number;

//---------PCI核心逻辑---------
//1. probe函数，检测到硬件自动调用该函数
static int edu_probe(struct pci_dev *dev, const struct pci_device_id *id){
    printk(KERN_INFO "[EDU DRIVER] Probe called. Found device vender: 0x%x Device: 0x%x\n.", id->vendor, id->device);
    return 0;
}

//2. Remove函数
static void edu_remove(struct pci_dev *dev){
    printk(KERN_INFO "[EDU_DRIVER] Device removed.\n");
}
//3. 驱动支持的设备号表，表内存放的数据结构是struct pci_device_id
static const struct pci_device_id edu_ids[] = {
    {PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID)},
    {0,}
};
//4. PCI驱动
static struct pci_driver edu_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = edu_ids,
    .probe = edu_probe,
    .remove = edu_remove,
};


//初始化函数，即向内核申请设备号
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