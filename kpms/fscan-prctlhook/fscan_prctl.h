/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FastScan prctl 接口定义
 * 内核模块和用户空间程序共享此头文件
 */

#ifndef _FSCAN_PRCTL_H
#define _FSCAN_PRCTL_H

#ifdef __KERNEL__
    // 内核空间 - 类型由 fscan.c 中的头文件提供
    #ifndef __user
        #define __user
    #endif
#else
    // 用户空间
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/prctl.h>
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

// 进程PID查询结构体
struct process_pid {
#ifdef __KERNEL__
    char __user *taskname; // 内核空间：用户空间字符串指针
#else
    char *taskname;        // 用户空间：进程名字符串
#endif
    pid_t pid;             // 返回的进程 PID（内核回写）
};

// prctl 命令定义
// 使用不常见的 option 值，伪装成自定义进程控制
// "MEM" 的 ASCII 码 + 操作类型
#define PRCTL_MEM_READ      0x4D454D01  // "MEM\x01" 
#define PRCTL_MEM_WRITE     0x4D454D02  // "MEM\x02"
#define PRCTL_PROCESS_PID   0x4D454D03  // "MEM\x03" - 获取进程PID

// 为了兼容性，也可以使用这些别名
#define OP_READ_MEM      PRCTL_MEM_READ
#define OP_WRITE_MEM     PRCTL_MEM_WRITE
#define OP_GET_PID       PRCTL_PROCESS_PID

// 命令码说明:
// 0x4D454D01 = 1297239809 (十进制)
// 这个值不会和任何标准 prctl option 冲突
// 标准 option 值通常都小于 100

#endif /* _FSCAN_PRCTL_H */

