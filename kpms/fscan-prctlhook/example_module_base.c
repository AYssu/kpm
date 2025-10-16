/* 
 * 使用 PRCTL_MODULE_BASE 获取模块基址的示例
 * 编译: gcc -o example_module_base example_module_base.c
 * 运行: ./example_module_base <pid> <module_name>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <errno.h>

// 包含头文件定义
#include "fscan_prctl.h"

void print_usage(const char *prog_name) {
    printf("使用方法: %s <pid> <module_name>\n", prog_name);
    printf("\n");
    printf("参数:\n");
    printf("  pid          目标进程的 PID\n");
    printf("  module_name  要查找的模块名（如 libc.so）\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s 1234 libc.so\n", prog_name);
    printf("  %s 5678 libart.so\n", prog_name);
    printf("  %s 9999 libunity.so\n", prog_name);
    printf("\n");
}

int main(int argc, char *argv[])
{
    struct module_base mb;
    pid_t target_pid;
    char module_name[256] = {0};
    int ret;
    
    // 检查是否是 root 权限
    if (getuid() != 0) {
        fprintf(stderr, "❌ 错误: 需要 root 权限运行此程序\n");
        fprintf(stderr, "请使用: su -c '%s' 或 sudo %s\n", argv[0], argv[0]);
        return 1;
    }
    
    // 检查参数
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    // 解析参数
    target_pid = atoi(argv[1]);
    if (target_pid <= 0) {
        fprintf(stderr, "❌ 错误: 无效的 PID: %s\n", argv[1]);
        return 1;
    }
    
    strncpy(module_name, argv[2], sizeof(module_name) - 1);
    
    printf("查找模块基址...\n");
    printf("  目标进程 PID: %d\n", target_pid);
    printf("  模块名: %s\n", module_name);
    printf("\n");
    
    // 准备结构体
    mb.pid = target_pid;
    mb.module_name = module_name;
    mb.base_address = 0;  // 初始化为 0，内核会回写
    
    // 调用 prctl
    ret = prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0);
    
    if (ret == 0) {
        printf("✅ 成功!\n");
        printf("   模块 '%s' 的基址: 0x%lx\n", module_name, mb.base_address);
        printf("\n");
        printf("可以使用这个基址进行后续操作，例如:\n");
        printf("  - 计算函数地址: 基址 + 偏移量\n");
        printf("  - 读取模块代码: prctl(PRCTL_MEM_READ, ...)\n");
        printf("  - 修改模块数据: prctl(PRCTL_MEM_WRITE, ...)\n");
        return 0;
    } else {
        fprintf(stderr, "❌ 失败: %s\n", strerror(errno));
        fprintf(stderr, "\n");
        fprintf(stderr, "可能的原因:\n");
        
        if (errno == EPERM) {
            fprintf(stderr, "  - 权限不足（需要 root）\n");
        } else if (errno == ENOENT) {
            fprintf(stderr, "  - 模块 '%s' 未在进程 %d 中加载\n", module_name, target_pid);
            fprintf(stderr, "  - 提示: 使用 'cat /proc/%d/maps' 查看进程的内存映射\n", target_pid);
        } else if (errno == ESRCH) {
            fprintf(stderr, "  - 进程 %d 不存在\n", target_pid);
        } else if (errno == EINVAL) {
            fprintf(stderr, "  - 内核模块未加载\n");
            fprintf(stderr, "  - 提示: 使用 'kpm load fscan-prctl.kpm' 加载模块\n");
        } else {
            fprintf(stderr, "  - 未知错误 (errno=%d)\n", errno);
        }
        
        return 1;
    }
}

