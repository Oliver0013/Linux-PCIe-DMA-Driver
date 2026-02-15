#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "../driver/pcie_edu.h"
#define DEVICE_PATH "/dev/edu_driver"

int main() {
    int fd;
    uint32_t val;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) { perror("Open"); return -1; }

    // --- 测试 1: 单纯查阅 ID (应该瞬间返回，不阻塞) ---
    __u32 hardware_id = 0;
    if (ioctl(fd, EDU_IOC_GET_ID, &hardware_id) == 0) {
        printf("[TEST 1] ioctl Get ID: 0x%08x\n", hardware_id);
    } else {
        perror("ioctl GET_ID failed");
    }

    // --- 测试 2: 触发阶乘计算 ---
    struct edu_fact_req fact_req = { .val = 5, .result = 0 };

    printf("[TEST 2/3] Calling ioctl to calculate factorial of %u...\n", fact_req.val);
    
    if (ioctl(fd, EDU_IOC_CALC_FACT, &fact_req) == 0) {
        printf("[TEST 2/3] ioctl Factorial Result: %u (Should be 120)\n", fact_req.result);
    } else {
        perror("ioctl CALC_FACT failed");
    }

    // ==========================================
    // --- [P5 新增] 测试 4: 验证 DMA 缓冲区 ---
    // ==========================================
    printf("[TEST 4] Checking DMA Buffer via backdoor Offset 0x1000...\n");
    if (pread(fd, &val, 4, 0x1000) == 4) {
        printf("[TEST 4] Read DMA Buffer (Offset 0x1000): 0x%08x\n", val);
        if (val == 0x12345678) {
            printf("         -> SUCCESS! CPU can see the DMA memory.\n");
        } else {
            printf("         -> FAIL! Value does not match magic number.\n");
        }
    } else {
        perror("Read DMA Buffer failed");
    }

    // ==========================================
    // --- [P5 新增] 测试 5: 触发 DMA 并验证 SRAM ---
    // ==========================================
    uint32_t dma_payload = 0x8899AABB;
    printf("\n[TEST 5] Initiating DMA Transfer with payload: 0x%08x\n", dma_payload);
    
    // 往 0x2000 写数据，触发驱动里的 DMA 发车逻辑
    if (pwrite(fd, &dma_payload, 4, 0x2000) == 4) {
        printf("         -> DMA Command sent to driver.\n");
    } else {
        perror("DMA trigger failed");
    }

    // 稍微等一下硬件搬运（QEMU 模拟器环境下其实是瞬时完成的）
    usleep(10000); 

    // 验证：去设备内部 SRAM (0x40000) 查岗，看看货到了没
    uint32_t sram_val;
    if (pread(fd, &sram_val, 4, 0x40000) == 4) {
        printf("[TEST 6] Read from EDU SRAM (Offset 0x40000): 0x%08x\n", sram_val);
        if (sram_val == dma_payload) {
            printf("         -> 🎉 BINGO! DMA hardware successfully moved the data!\n");
        } else {
            printf("         -> ❌ FAIL! Data mismatch.\n");
        }
    } else {
        perror("Read SRAM failed");
    }

    close(fd);
    return 0;
}