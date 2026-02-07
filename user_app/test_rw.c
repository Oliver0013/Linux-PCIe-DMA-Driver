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
    // 使用 pread 读取偏移量 0x00 处的 4 字节
    //pread原子读，随机存取，不依赖也不改变文件描述符当前的指针位置。
    //lseek会改变off，所以lseek+read不是原子操作
    if (pread(fd, &val, 4, 0x00) == 4) {
        printf("[TEST] Read ID (Offset 0x00): 0x%08x\n", val);
    } else {
        perror("Read ID failed");
    }

    // --- 测试 2: 触发阶乘计算 ---
    printf("[TEST] Writing %u to trigger Factorial...\n", write_val);
    write(fd, &write_val, 4); // 你的 write 还是老样子，默认写到 0x08

    // --- 测试 3: 获取阶乘结果 (应该阻塞等待中断) ---
    printf("[TEST] Waiting for result at Offset 0x08...\n");
    // 使用 pread 读取偏移量 0x08 处的 4 字节
    if (pread(fd, &val, 4, 0x08) == 4) {
        printf("[TEST] Read Factorial (Offset 0x08): %u (Should be 120)\n", val);
    }

    close(fd);
    return 0;
}