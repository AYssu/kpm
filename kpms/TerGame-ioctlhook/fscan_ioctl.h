/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FastScan ioctl 接口定义
 * 内核模块和用户空间程序共享此头文件
 * 仅支持内存读写功能，PID获取请在用户层实现
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

#ifndef _FSCAN_IOCTL_H
#define _FSCAN_IOCTL_H

#ifdef __KERNEL__
    #ifndef __user
        #define __user
    #endif
#else
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
#endif

// ioctl 宏定义
#ifndef _IOC_NRBITS
    #define _IOC_NRBITS     8
    #define _IOC_TYPEBITS   8
    #define _IOC_SIZEBITS   14
    #define _IOC_DIRBITS    2
    
    #define _IOC_NRSHIFT    0
    #define _IOC_TYPESHIFT  (_IOC_NRSHIFT+_IOC_NRBITS)
    #define _IOC_SIZESHIFT  (_IOC_TYPESHIFT+_IOC_TYPEBITS)
    #define _IOC_DIRSHIFT   (_IOC_SIZESHIFT+_IOC_SIZEBITS)
    
    #define _IOC_NONE       0U
    #define _IOC_WRITE      1U
    #define _IOC_READ       2U
    
    #define _IOC(dir,type,nr,size) \
        (((dir)  << _IOC_DIRSHIFT) | \
         ((type) << _IOC_TYPESHIFT) | \
         ((nr)   << _IOC_NRSHIFT) | \
         ((size) << _IOC_SIZESHIFT))
    
    #define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
#endif

// 内存操作结构体
struct mem_operation {
    pid_t target_pid;      // 目标进程 PID
    uint64_t addr;         // 目标地址
#ifdef __KERNEL__
    void __user *buffer;   // 内核空间：用户空间缓冲区指针
#else
    void *buffer;          // 用户空间：普通指针
#endif
    uint64_t size;         // 读写大小
};

// ioctl 命令定义 - 仅内存读写
#define FSCAN_IOC_READ_MEM    _IOWR('v', 0x9F, struct mem_operation)
#define FSCAN_IOC_WRITE_MEM   _IOWR('v', 0xA0, struct mem_operation)

// 兼容旧代码
#define OP_READ_MEM     FSCAN_IOC_READ_MEM
#define OP_WRITE_MEM    FSCAN_IOC_WRITE_MEM

#endif /* _FSCAN_IOCTL_H */
