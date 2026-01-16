/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TearGame KPM 内存读写接口定义
 * 内核模块和用户空间程序共享此头文件
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * **原作者**: 阿夜 (AYssu)
- GitHub: https://github.com/AYssu
- QQ: 2997036064
- QQ群: 903485530



 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

#ifndef _TEAR_KPM_PRCTL_H
#define _TEAR_KPM_PRCTL_H

#ifdef __KERNEL__
    // 内核空间
    #ifndef __user
        #define __user
    #endif
#else
    // 用户空间
    #include <stdint.h>
    #include <sys/types.h>
#endif

// ==================== 内存操作结构体 ====================
// 用于 prctl 系统调用传递内存读写参数
struct mem_operation {
    pid_t target_pid;      // 目标进程 PID
    uint64_t addr;         // 目标虚拟地址
#ifdef __KERNEL__
    void __user *buffer;   // 内核空间：用户态缓冲区指针
#else
    void *buffer;          // 用户空间：数据缓冲区
#endif
    uint64_t size;         // 读写字节数
};

// ==================== 获取 PID 操作结构体 ====================
// 用于 prctl 系统调用传递包名查找参数
#define MAX_PACKAGE_NAME_LEN 256

struct pid_request {
#ifdef __KERNEL__
    const char __user *package_name;  // 内核空间：用户态包名指针
#else
    const char *package_name;         // 用户空间：包名字符串
#endif
    uint32_t name_len;                // 包名长度（不含 \0）
    pid_t result_pid;                 // 返回的 PID（内核写入）
};

// ==================== prctl 命令码定义 ====================
// 使用 0x4D454D 前缀（"MEM" 的 ASCII 码），避免与系统 prctl 冲突
// 标准 prctl option 值通常小于 100，我们使用大数值确保不冲突

#define PRCTL_MEM_READ       0x4D454D01  // 内存读取命令 (access_process_vm)
#define PRCTL_MEM_WRITE      0x4D454D02  // 内存写入命令 (access_process_vm)
#define PRCTL_GET_PID        0x4D454D03  // 通过包名获取 PID
#define PRCTL_MEM_READ_SAFE  0x4D454D04  // 物理内存读取（备用方式）
#define PRCTL_MEM_WRITE_SAFE 0x4D454D05  // 物理内存写入（备用方式）

#endif /* _TEAR_KPM_PRCTL_H */
