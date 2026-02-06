#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define DEVICE_PATH "/dev/edu_driver"

int main() {
    int fd;
    uint32_t read_val;
    uint32_t write_val = 5; // 我们来算 5 的阶乘

    printf("=== QEMU EDU Device RW Test ===\n");

    // 1. 打开设备
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }
    printf("Device opened successfully.\n");

    // 2. 读测试
    ssize_t ret = read(fd, &read_val, sizeof(read_val));
    
    // 【真相探针】打印返回值！
    printf("[APP DEBUG] read() returned: %ld\n", ret);

    if (ret < 0) {
        perror("Read failed");
    } else if (ret == 0) {
        printf("[APP WARNING] Read returned 0 (EOF)! Driver sent nothing.\n");
    } else {
        printf("[APP] Read ID from driver: 0x%08x\n", read_val);
    }

    // 3. 写测试 (写入 5，让硬件计算 5!)
    printf("[APP] Writing %u to factorial register...\n", write_val);
    if (write(fd, &write_val, sizeof(write_val)) < 0) {
        perror("Write failed");
    }

    // 4. 再读一次阶乘结果 (注意：你需要实现 IOCTL 或者读偏移 0x08 才能读回结果，
    // 但目前我们的 read 固定读 0x00。你可以看 dmesg 验证写入是否成功)
    
    close(fd);
    return 0;
}