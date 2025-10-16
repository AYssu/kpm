/* 
 * 使用 PRCTL_PROCESS_PID 获取进程 PID 的示例
 * 编译: gcc -o example_get_pid example_get_pid.c
 * 运行: ./example_get_pid
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

// 包含头文件定义
#include "fscan_prctl.h"

int main(int argc, char *argv[])
{
    struct process_pid pp;
    char process_name[256] = {0};
    int ret;
    
    // 检查是否是 root 权限
    if (getuid() != 0) {
        fprintf(stderr, "错误: 需要 root 权限运行此程序\n");
        fprintf(stderr, "请使用: su -c '%s' 或 sudo %s\n", argv[0], argv[0]);
        return 1;
    }
    
    // 从命令行参数获取进程名
    if (argc > 1) {
        strncpy(process_name, argv[1], sizeof(process_name) - 1);
    } else {
        // 默认查找 system_server (Android) 或 init (Linux)
        printf("使用方法: %s <进程名>\n", argv[0]);
        printf("尝试查找默认进程...\n");
        strcpy(process_name, "system_server");
    }
    
    printf("查找进程: %s\n", process_name);
    
    // 准备结构体
    pp.taskname = process_name;
    pp.pid = 0;  // 初始化为 0，内核会回写
    
    // 调用 prctl
    ret = prctl(PRCTL_PROCESS_PID, &pp, 0, 0, 0);
    
    if (ret == 0) {
        printf("✓ 成功! 进程 '%s' 的 PID 是: %d\n", process_name, pp.pid);
        return 0;
    } else {
        perror("✗ 失败");
        
        if (ret == -1) {
            printf("\n可能的原因:\n");
            printf("1. 内核模块未加载\n");
            printf("2. 进程 '%s' 不存在\n", process_name);
            printf("3. 权限不足（需要 root）\n");
        }
        
        return 1;
    }
}

