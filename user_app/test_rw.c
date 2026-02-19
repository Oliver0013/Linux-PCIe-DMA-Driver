#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "../driver/uapi_edu.h"
#define DEVICE_PATH "/dev/edu_driver0"

int main() {
    int fd;

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
    // --- [工业级架构] 测试 4: 硬件一致性 DMA 环回自检 ---
    // ==========================================
    printf("\n[TEST 4] Initiating Coherent DMA Hardware Loopback Diagnostic...\n");
    
    // 核心精髓：因为我们在头文件定义的是 _IO('E', 3)，这代表无数据载荷交互
    // 所以 ioctl 的第三个参数直接传 0 即可。内核会自动生成魔数并指挥硬件跑圈！
    if (ioctl(fd, EDU_IOC_DMA_LOOPBACK, 0) == 0) {
        printf("         -> 🎉 BINGO! DMA Loopback Test Passed!\n");
        printf("         -> (Bus Mastering & DMA Engine are fully operational!)\n");
    } else {
        perror("         -> ❌ FAIL! DMA Loopback Test failed");
    }

    close(fd);
    return 0;
}