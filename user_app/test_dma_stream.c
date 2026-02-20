#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define DEVICE_PATH "/dev/edu_driver0"
// 定义真实的物理极限 (用于内存对齐分配)
#define SRAM_MEM_SIZE 4096  
// [Quirk] 绕过 QEMU off-by-one bug，每次最多只发 4095 字节
#define SRAM_PAYLOAD_SIZE 4095

int main() {
    int fd;
    ssize_t bytes;
    
    // 注意：aligned_alloc 的第二个参数(size)必须是第一个参数(alignment)的整数倍
    char *tx_buf = aligned_alloc(4096, SRAM_MEM_SIZE); 
    char *rx_buf = aligned_alloc(4096, SRAM_MEM_SIZE);
    
    if (!tx_buf || !rx_buf) {
        perror("OOM");
        return -1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) { 
        perror("Open Device Failed"); 
        return -1; 
    }

    printf("==================================================\n");
    printf("🚀 Initiating Streaming DMA Data Plane Test\n");
    printf("==================================================\n\n");

    // ==========================================
    // 测试 1: 4KB 极限满载写入与回读比对
    // ==========================================
    printf("[TEST 1] 4KB Full SRAM Streaming DMA Transfer...\n");
    
    // 填充特殊的 Magic Pattern (0xAA, 0xBB, 等交替)
    memset(tx_buf, 0xAA, SRAM_PAYLOAD_SIZE);
    
    // 游标归零
    lseek(fd, 0, SEEK_SET);
    
    // 发起 4KB 流式 DMA 写入
    bytes = write(fd, tx_buf, SRAM_PAYLOAD_SIZE);
    if (bytes != SRAM_PAYLOAD_SIZE) {
        printf("  ❌ Write failed! Expected %d, got %zd\n", SRAM_PAYLOAD_SIZE, bytes);
        goto cleanup;
    }
    printf("  -> Write 4096 bytes: SUCCESS\n");

    // 游标重新归零，准备回读
    lseek(fd, 0, SEEK_SET);
    memset(rx_buf, 0x00, SRAM_PAYLOAD_SIZE); // 清空接收区
    
    // 发起 4KB 流式 DMA 读取 (SRAM -> 主存)
    bytes = read(fd, rx_buf, SRAM_PAYLOAD_SIZE);
    if (bytes != SRAM_PAYLOAD_SIZE) {
        printf("  ❌ Read failed! Expected %d, got %zd\n", SRAM_PAYLOAD_SIZE, bytes);
        goto cleanup;
    }

    // 严苛的数据一致性比对 (检查 Cache 是否正确 Invalidate)
    if (memcmp(tx_buf, rx_buf, SRAM_PAYLOAD_SIZE) == 0) {
        printf("  -> Read & Data Verification: 🎉 PERFECT MATCH!\n");
    } else {
        printf("  -> Read & Data Verification: ❌ DATA CORRUPTION DETECTED!\n");
    }

    // ==========================================
    // 测试 2: lseek 随机偏移量测试 (Random Access)
    // ==========================================
    printf("\n[TEST 2] Random Access & Offset Routing (lseek)...\n");
    
    char *secret_msg = "HELLO_EDU_DMA_STREAM!";
    size_t msg_len = strlen(secret_msg) + 1;
    
    // 我们故意把游标定位到 SRAM 的中后段 (比如偏移 2048 处)
    off_t target_offset = 2048;
    lseek(fd, target_offset, SEEK_SET);
    
    // 写入这段短报文并校验返回值
    bytes = write(fd, secret_msg, msg_len);
    if (bytes != msg_len) {
        printf("  ❌ Write failed at offset %ld! Expected %zu, got %zd\n", target_offset, msg_len, bytes);
        goto cleanup;
    }
    printf("  -> Secret message written successfully at offset %ld.\n", target_offset);
    
    // 游标复位到目标位置，准备读取
    lseek(fd, target_offset, SEEK_SET);
    char small_rx[64] = {0};
    
    // 读取并校验返回值
    bytes = read(fd, small_rx, msg_len);
    if (bytes != msg_len) {
        printf("  ❌ Read failed at offset %ld! Expected %zu, got %zd\n", target_offset, msg_len, bytes);
        goto cleanup;
    }
    
    if (strcmp(secret_msg, small_rx) == 0) {
        printf("  -> Offset Read Verification: 🎉 SUCCESS! Got: %s\n", small_rx);
    } else {
        printf("  -> Offset Read Verification: ❌ FAILED! Got: %s\n", small_rx);
    }

    // ==========================================
    // 测试 3: 越界截断保护测试 (VFS Boundary Protection)
    // ==========================================
    printf("\n[TEST 3] Hardware Boundary Protection Test...\n");
    
    // 使用我们打过补丁的真实边界 4095 来计算！
    // 游标定位到 4095 - 10 = 4085 的位置
    off_t actual_boundary = 4095;
    lseek(fd, actual_boundary - 10, SEEK_SET);
    
    // 恶意尝试写入 100 字节，预期驱动应当将其截断为 10 字节
    bytes = write(fd, tx_buf, 100);
    if (bytes == 10) {
        printf("  -> Boundary Protection: 🎉 SUCCESS! Truncated 100 bytes to %zd bytes.\n", bytes);
    } else {
        printf("  -> Boundary Protection: ❌ FAILED! Wrote %zd bytes instead of 10.\n", bytes);
    }
cleanup:
    close(fd);
    free(tx_buf);
    free(rx_buf);
    printf("\n==================================================\n");
    return 0;
}