/* uapi_edu.h - User API for QEMU EDU Driver */
#ifndef _UAPI_EDU_H_
#define _UAPI_EDU_H_

#include <linux/ioctl.h>
#include <linux/types.h>

// ========================================================
// 🌟 ioctl 通信契约 (用户态与内核态共享)
// ========================================================
struct edu_fact_req {
    __u32 val;
    __u32 result;
};

#define EDU_MAGIC 'E'
#define EDU_IOC_GET_ID        _IOR(EDU_MAGIC, 1, __u32)
#define EDU_IOC_CALC_FACT     _IOWR(EDU_MAGIC, 2, struct edu_fact_req)
#define EDU_IOC_DMA_LOOPBACK  _IO('E', 3)

#endif /* _UAPI_EDU_H_ */