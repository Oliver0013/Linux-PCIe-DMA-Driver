#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include "../driver/uapi_edu.h"

// 预先算好的阶乘结果，用于校验
const __u32 EXPECTED_RESULTS[11] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};

struct thread_args {
    char *dev_path;
    int thread_id;
    __u32 calc_val;
};

// 线程工作函数：疯狂请求阶乘
void *hammer_device(void *arg) {
    struct thread_args *t_arg = (struct thread_args *)arg;
    int fd;
    // 【暴增】把循环次数拉高到 10 万次！
    int loop_count = 100000; 

    fd = open(t_arg->dev_path, O_RDWR);
    if (fd < 0) { 
        printf("❌ [Thread %d] Failed to open %s\n", t_arg->thread_id, t_arg->dev_path);
        pthread_exit(NULL); 
    }

    printf("🚀 [Thread %d] Started. Hammering %s for %u! (Expect %u)\n", 
           t_arg->thread_id, t_arg->dev_path, t_arg->calc_val, EXPECTED_RESULTS[t_arg->calc_val]);

    for (int i = 1; i <= loop_count; i++) {
        struct edu_fact_req req = { .val = t_arg->calc_val, .result = 0 };
        
        // 【制造混乱】在发起 ioctl 前，概率性主动放弃 CPU，引诱调度器打乱执行顺序
        if (i % 7 == 0) {
            sched_yield(); 
        }

        if (ioctl(fd, EDU_IOC_CALC_FACT, &req) == 0) {
            if (req.result != EXPECTED_RESULTS[t_arg->calc_val]) {
                printf("\n💥 [Thread %d] FATAL ERROR at loop %d! Concurrency Failure!\n", t_arg->thread_id, i);
                printf("💥 Expected %u! = %u, but got %u\n", t_arg->calc_val, EXPECTED_RESULTS[t_arg->calc_val], req.result);
                exit(1); 
            }
        } else {
            perror("ioctl failed");
            exit(1);
        }

        // 定期打印进度，防止你以为程序死机了
        if (i % 25000 == 0) {
            printf("   -> [Thread %d] Completed %d / %d iterations...\n", t_arg->thread_id, i, loop_count);
        }
    }

    printf("✅ [Thread %d] Finished %d successful loops.\n", t_arg->thread_id, loop_count);
    close(fd);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    pthread_t threads[4];
    struct thread_args args[4];
    
    // 允许通过命令行参数指定测哪个设备，默认测第一个
    char *dev_path = (argc > 1) ? argv[1] : "/dev/edu_driver0";
    printf("--- Starting Mutex Stress Test on %s ---\n", dev_path);

    // 创建 4 个并发线程，故意让它们计算不同的阶乘
    for (int i = 0; i < 4; i++) {
        args[i].dev_path = dev_path;
        args[i].thread_id = i + 1;
        args[i].calc_val = i + 5; // 分别计算 5!, 6!, 7!, 8!
        pthread_create(&threads[i], NULL, hammer_device, &args[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("🎉 All threads completed successfully. No data corruption! Mutex is ROCK SOLID!\n");
    return 0;
}