#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#define DEVICE_PATH "/dev/edu_driver"

int main() {
    int fd;
    uint32_t val;
    uint32_t write_val = 5;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) { perror("Open"); return -1; }

    // --- 测试 1: 单纯查阅 ID (应该瞬间返回，不阻塞) ---
    if (pread(fd, &val, 4, 0x00) == 4) {
        printf("[TEST 1] Read ID (Offset 0x00): 0x%08x\n", val);
    } else {
        perror("Read ID failed");
    }

    // --- 测试 2: 触发阶乘计算 ---
    printf("[TEST 2] Writing %u to trigger Factorial...\n", write_val);
    // 【修改点】：统一使用 pwrite，显式指定偏移量 0x08，防止偏移量错乱
    if (pwrite(fd, &write_val, 4, 0x08) != 4) {
        perror("Write Factorial failed");
    }

    // --- 测试 3: 获取阶乘结果 (应该阻塞等待中断) ---
    printf("[TEST 3] Waiting for result at Offset 0x08...\n");
    if (pread(fd, &val, 4, 0x08) == 4) {
        printf("[TEST 3] Read Factorial (Offset 0x08): %u (Should be 120)\n", val);
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