#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
//模块信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OLIVER");
MODULE_DESCRIPTION("Qemu EDU PCIe Device Driver");
MODULE_VERSION("0.1");

//宏定义设备名称，出现在/proc/devices里
#define DRIVER_NAME "edu_driver"

//全局变量存申请的设备号
static dev_t dev_number;

//初始化函数，即向内核申请设备号
static int __init dev_init(void){
    //存放申请结果
    int ret;
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
    //
    printk(KERN_INFO "[EDU_DRIVER] Success! Major: %d, Minor: %d.\n", MAJOR(dev_number), MINOR(dev_number));
    return 0;    
}
//退出函数
static void __exit dev_exit(void){
    //释放设备号
    unregister_chrdev_region(dev_number,1);
    printk(KERN_INFO "[EDU_DRIVER] Module Exit.\n");
}
//内核初始化/退出关联函数
module_init(dev_init);
module_exit(dev_exit);