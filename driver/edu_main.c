#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/idr.h>
// 引入我们的核心头文件
#include "pcie_edu.h"

// 模块信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("OLIVER");
MODULE_DESCRIPTION("Qemu EDU PCIe Device Driver - Main");
MODULE_VERSION("0.3"); // 升级为多文件版本

// 全局变量
static dev_t base_dev_number; // 全局基础设备号
static DEFINE_IDA(edu_ida);   // 定义一个全局的 IDA 分配器
static struct class *edu_class; // 全局设备类指针

// --------- PCI Probe 核心逻辑 ---------
static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    int ret;
    int minor;
    struct edu_device *edu;

    // 0. 创建设备实例
    edu = kzalloc(sizeof(struct edu_device), GFP_KERNEL);
    if (!edu) return -ENOMEM;

    mutex_init(&edu->hw_lock);

    //动态分配空闲的次设备号
    minor = ida_alloc_max(&edu_ida, MAX_EDU_DEVICES - 1, GFP_KERNEL);
    if (minor < 0) {
        ret = minor;
        goto err_free; 
    }   

    // pdev和edu相互关联
    edu->pdev = pdev;
    pci_set_drvdata(pdev, edu);
    // 将全局主设备号与刚刚分配到的次设备号组合，生成该硬件专属的设备号
    edu->dev_num = MKDEV(MAJOR(base_dev_number), minor);

    // 1. 启动硬件设备/硬件初始化
    ret = pci_enable_device(pdev);
    if (ret) goto err_ida;

    // 开启总线主控模式 (供 DMA 使用)
    pci_set_master(pdev);

    // 2. 申请BAR资源
    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret) goto err_disable;

    // 3. MMIO映射(BAR 0)
    edu->mmio_base = pci_iomap(pdev, 0, 0);
    if (!edu->mmio_base) {
        ret = -ENOMEM;
        goto err_regions;
    }

    // DMA内存分配 (一致性 DMA 掩码设置)
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(28));
    if (ret) goto err_iounmap;
    
    // 申请一致性缓存区
    edu->dma_cpu_addr = dma_alloc_coherent(&pdev->dev, EDU_DMA_SIZE, &edu->dma_bus_addr, GFP_KERNEL);
    if (!edu->dma_cpu_addr){
        ret = -ENOMEM;
        goto err_iounmap;
    }
    memset(edu->dma_cpu_addr, 0, EDU_DMA_SIZE); // 清零
    *(u32 *)edu->dma_cpu_addr = 0x12345678;     // 写入 Magic Number

    // 3. 中断与异步通知相关
    init_waitqueue_head(&edu->wait_q);
    atomic_set(&edu->fact_ready, 0);
    atomic_set(&edu->dma_ready, 0);
    
    // 申请MSI中断向量
    // 内核会读取设备的 PCIe 配置空间，判断支持哪种机制并自动分配
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_ALL_TYPES);
    if (ret < 0) {
        printk(KERN_ERR "[EDU] Failed to allocate MSI/MSI-X vectors\n");
        goto err_dma; 
    }

    // 挂载MSI中断服务程序
    ret = request_irq(pci_irq_vector(pdev, 0), edu_isr, 0, DRIVER_NAME, edu);
    if (ret) {
        printk(KERN_ERR "[EDU] Failed to request IRQ\n");
        goto err_msi; 
    }

    // -----硬件DMA环回自测-----
    printk(KERN_INFO "[EDU] Running Power-On Self-Test (POST)...\n");
    // 【注意：这里调用了 edu_hw.c 里的 edu_dma_loopback_test】
    ret = edu_dma_loopback_test(edu);
    if (ret) {
        printk(KERN_ERR "[EDU] FATAL: DMA Loopback failed! Hardware is faulty. Aborting load.\n");
        goto err_irq; 
    }
    printk(KERN_INFO "[EDU] POST passed. DMA Engine is fully healthy.\n");    

    // 4. 初始化注册字符设备
    edu->cdev.owner = THIS_MODULE;
    // 【注意：这里使用了 edu_cdev.c 里定义的 edu_fops】
    cdev_init(&edu->cdev, &edu_fops);
    
    ret = cdev_add(&edu->cdev, edu->dev_num, 1);
    if (ret) goto err_irq;

    //5. 创建设备节点
    edu->device = device_create(edu_class, NULL, edu->dev_num, NULL, "%s%d", DRIVER_NAME, minor);
    if (IS_ERR(edu->device)) {
        ret = PTR_ERR(edu->device);
        goto err_cdev;
    }

    printk(KERN_INFO "[EDU DRIVER V3] Probe successful. Vendor: 0x%x Device: 0x%x\n", id->vendor, id->device);
    return 0;

// --- 错误级联释放 ---
err_cdev:
    cdev_del(&edu->cdev);
err_irq:
    free_irq(pci_irq_vector(pdev, 0), edu);
err_msi:
    pci_free_irq_vectors(pdev);
err_dma:
    dma_free_coherent(&pdev->dev, EDU_DMA_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
err_iounmap:
    pci_iounmap(pdev, edu->mmio_base);
err_regions:
    pci_release_regions(pdev);
err_disable:
    pci_clear_master(pdev);
    pci_disable_device(pdev);
err_ida:   
    ida_free(&edu_ida, minor);
err_free:
    kfree(edu); 
    return ret;
}

// --------- PCI Remove 核心逻辑 ---------
static void edu_remove(struct pci_dev *pdev)
{
    struct edu_device *edu = pci_get_drvdata(pdev);
    
    if (edu){
        // 按照 Probe 的反向顺序释放资源
        device_destroy(edu_class, edu->dev_num);
        cdev_del(&edu->cdev);
        free_irq(pci_irq_vector(pdev, 0), edu);
        pci_free_irq_vectors(pdev);
        dma_free_coherent(&pdev->dev, EDU_DMA_SIZE, edu->dma_cpu_addr, edu->dma_bus_addr);
        pci_iounmap(pdev, edu->mmio_base);
        pci_release_regions(pdev);
        pci_clear_master(pdev);
        pci_disable_device(pdev);
        ida_free(&edu_ida, MINOR(edu->dev_num));
        kfree(edu);      
    }
    printk(KERN_INFO "[EDU_DRIVER] Device removed and unmapped.\n");
}

// --------- PCI ID 表与驱动注册 ---------
static const struct pci_device_id edu_ids[] = {
    {PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID)},
    {0,}
};
MODULE_DEVICE_TABLE(pci, edu_ids);

static struct pci_driver edu_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = edu_ids,
    .probe = edu_probe,
    .remove = edu_remove,
};

// --------- 模块初始化与退出 ---------
static int __init dev_init(void)
{
    int ret;
    printk(KERN_INFO "[EDU_DRIVER] Initializing Module...\n");
    
    // 1. 申请字符设备号
    ret = alloc_chrdev_region(&base_dev_number, 0, MAX_EDU_DEVICES, DRIVER_NAME);
    if (ret < 0){
        printk(KERN_ERR "[EDU_DRIVER] Failed to allocate dev number.\n");
        return ret;
    }
    printk(KERN_INFO "[EDU_DRIVER] Success! Major: %d, Minor: %d.\n", MAJOR(base_dev_number), MINOR(base_dev_number));
 
    // 2. 创建设备类
    // 整个驱动生命周期只执行一次
    edu_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(edu_class)) {
        unregister_chrdev_region(base_dev_number, MAX_EDU_DEVICES);
        return PTR_ERR(edu_class);
    }

    // 3. 注册 PCI 驱动
    ret = pci_register_driver(&edu_pci_driver);
    if (ret < 0){
        printk(KERN_ERR "[EDU_DRIVER] Failed to register PCI driver.\n");
        unregister_chrdev_region(base_dev_number, MAX_EDU_DEVICES);
        return ret;
    }
    return 0;    
}

static void __exit dev_exit(void)
{
    pci_unregister_driver(&edu_pci_driver); 
    class_destroy(edu_class);   
    unregister_chrdev_region(base_dev_number, MAX_EDU_DEVICES);
    printk(KERN_INFO "[EDU_DRIVER] Module Exit.\n");
}

module_init(dev_init);
module_exit(dev_exit);