/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * 函数名混淆映射表
 * 使用随机生成的标识符替代有意义的函数名
 */

#ifndef _OBFUSCATE_H
#define _OBFUSCATE_H

// 核心功能函数混淆
#define _pid_virt_to_phys       __x7a2fb9c
#define read_physical_address   __m3e9d1f7
#define read_mem                __k5n1q8p2
#define before_prctl            __AYssu520

// 初始化和退出函数混淆
#define syscall_hook_demo_init  __d2h7j5r1
#define syscall_hook_control0   __b8f4c9x3
#define syscall_hook_demo_exit  __g1l6p4z9

// 内联函数混淆（如果需要）
#define my_find_task_by_vpid    __a4s8n2m7

// 时间戳验证函数混淆
#define ktime_get_real_seconds  __t6r9k3w8

#endif /* _OBFUSCATE_H */

