/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TearGame KPM 用户态驱动接口
 * 提供简洁的 C/C++ API 用于跨进程内存读写
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

#ifndef _KPM_DRIVER_TEAR_GAME_H
#define _KPM_DRIVER_TEAR_GAME_H

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/prctl.h>
#include <unistd.h>
#include <dirent.h>

// ==================== 内存操作结构体 ====================
// 与内核模块共享的数据结构，用于传递读写参数
struct mem_operation {
    pid_t target_pid;      // 目标进程 PID
    uint64_t addr;         // 目标虚拟地址
    void *buffer;          // 数据缓冲区（读取时接收数据，写入时提供数据）
    uint64_t size;         // 读写字节数
};

// ==================== prctl 命令码 ====================
// 与内核模块约定的命令码，用于区分读写操作
#define PRCTL_MEM_READ       0x4D454D01  // 内存读取 (access_process_vm)
#define PRCTL_MEM_WRITE      0x4D454D02  // 内存写入 (access_process_vm)
#define PRCTL_GET_PID        0x4D454D03  // 通过包名获取 PID（仅短包名）
#define PRCTL_MEM_READ_SAFE  0x4D454D04  // 物理内存读取（更底层）
#define PRCTL_MEM_WRITE_SAFE 0x4D454D05  // 物理内存写入（更底层）

// ==================== 获取 PID 请求结构体 ====================
#define MAX_PACKAGE_NAME_LEN 64

struct pid_request {
    const char *package_name;  // 包名字符串
    uint32_t name_len;         // 包名长度（不含 \0）
    pid_t result_pid;          // 返回的 PID（内核写入）
};

// ==================== 核心 API ====================

/**
 * 读取目标进程内存（原始接口）
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @param buf   接收数据的缓冲区
 * @param len   要读取的字节数
 * @return      成功返回 0，失败返回 -1
 */
inline int readMem(pid_t pid, uint64_t addr, void* buf, size_t len) {
    struct mem_operation op;
    op.target_pid = pid;
    op.addr = addr;
    op.buffer = buf;
    op.size = len;
    return prctl(PRCTL_MEM_READ, (unsigned long)&op, 0, 0, 0);
}

/**
 * 写入目标进程内存（原始接口）
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @param buf   要写入的数据
 * @param len   要写入的字节数
 * @return      成功返回 0，失败返回 -1
 */
inline int writeMem(pid_t pid, uint64_t addr, const void* buf, size_t len) {
    struct mem_operation op;
    op.target_pid = pid;
    op.addr = addr;
    op.buffer = const_cast<void*>(buf);
    op.size = len;
    return prctl(PRCTL_MEM_WRITE, (unsigned long)&op, 0, 0, 0);
}

// ==================== 物理内存读写接口（更底层更安全） ====================

/**
 * 读取目标进程内存（物理内存方式）
 * 通过页表遍历直接访问物理内存，绕过部分内存保护
 * 
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @param buf   接收数据的缓冲区
 * @param len   要读取的字节数
 * @return      成功返回 0，失败返回 -1
 */
inline int readMemSafe(pid_t pid, uint64_t addr, void* buf, size_t len) {
    struct mem_operation op;
    op.target_pid = pid;
    op.addr = addr;
    op.buffer = buf;
    op.size = len;
    return prctl(PRCTL_MEM_READ_SAFE, (unsigned long)&op, 0, 0, 0);
}

/**
 * 写入目标进程内存（物理内存方式）
 * 通过页表遍历直接写入物理内存，绕过部分内存保护
 * 
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @param buf   要写入的数据
 * @param len   要写入的字节数
 * @return      成功返回 0，失败返回 -1
 */
inline int writeMemSafe(pid_t pid, uint64_t addr, const void* buf, size_t len) {
    struct mem_operation op;
    op.target_pid = pid;
    op.addr = addr;
    op.buffer = const_cast<void*>(buf);
    op.size = len;
    return prctl(PRCTL_MEM_WRITE_SAFE, (unsigned long)&op, 0, 0, 0);
}

// ==================== C++ 模板接口（推荐使用） ====================

/**
 * 读取任意类型数据（模板版本）
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @return      读取到的值（失败时返回默认值）
 * 
 * 示例：
 *   int hp = read<int>(pid, 0x12345678);
 *   float speed = read<float>(pid, baseAddr + 0x100);
 */
template<typename T>
inline T read(pid_t pid, uint64_t addr) {
    T val{};
    readMem(pid, addr, &val, sizeof(T));
    return val;
}

/**
 * 写入任意类型数据（模板版本）
 * @param pid   目标进程 PID
 * @param addr  目标虚拟地址
 * @param val   要写入的值
 * @return      成功返回 true，失败返回 false
 * 
 * 示例：
 *   write<int>(pid, 0x12345678, 9999);
 *   write<float>(pid, baseAddr + 0x100, 100.0f);
 */
template<typename T>
inline bool write(pid_t pid, uint64_t addr, const T& val) {
    return writeMem(pid, addr, &val, sizeof(T)) == 0;
}

// ==================== 物理内存模板接口（更底层更安全） ====================

/**
 * 读取任意类型数据（物理内存方式）
 */
template<typename T>
inline T readSafe(pid_t pid, uint64_t addr) {
    T val{};
    readMemSafe(pid, addr, &val, sizeof(T));
    return val;
}

/**
 * 写入任意类型数据（物理内存方式）
 */
template<typename T>
inline bool writeSafe(pid_t pid, uint64_t addr, const T& val) {
    return writeMemSafe(pid, addr, &val, sizeof(T)) == 0;
}

// ==================== 辅助函数 ====================

/**
 * 检查是否以 root 权限运行
 * @return true 如果是 root，false 否则
 */
inline bool isRoot() {
    return getuid() == 0;
}

// ==================== 内核态获取 PID 接口 ====================

/**
 * 通过包名获取 PID（内核态实现，高效遍历）
 * 使用内核 find_task_by_vpid + 读取 /proc/<pid>/cmdline 实现
 * 相比用户态遍历 /proc 更高效，且能获取完整包名（无 16 字节截断）
 * 
 * @param name  包名（如 com.tencent.tmgp.sgame）
 * @return      PID，失败返回 -1
 * 
 * 示例：
 *   pid_t pid = getPidByNameKernel("com.tencent.tmgp.sgame");
 */
inline pid_t getPidByNameKernel(const char* name) {
    if (!name || !*name) return -1;
    
    struct pid_request req = {0};  // 初始化为0
    req.package_name = name;
    req.name_len = (uint32_t)strlen(name);
    req.result_pid = -1;
    
    if (req.name_len >= MAX_PACKAGE_NAME_LEN) {
        return -1;  // 包名过长
    }
    
    int ret = prctl(PRCTL_GET_PID, (unsigned long)&req, 0, 0, 0);
    if (ret == 0) {
        return req.result_pid;
    }
    return -1;
}

// ==================== 进程/模块辅助函数 ====================

/**
 * 通过进程名获取 PID（读取 /proc 文件系统）
 * 只返回主进程 PID，跳过 :service 等子进程
 * 
 * @param name  进程名（包名，如 com.tencent.tmgp.sgame）
 * @return      PID，失败返回 -1
 * 
 * 示例：
 *   pid_t pid = getPidByName("com.tencent.tmgp.sgame");
 */
inline pid_t getPidByName(const char* name) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;
    
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        // 跳过非目录
        if (entry->d_type != DT_DIR) continue;
        
        // 检查是否为数字（PID 目录）
        bool isNum = true;
        for (const char* p = entry->d_name; *p && isNum; p++) {
            if (*p < '0' || *p > '9') isNum = false;
        }
        if (!isNum) continue;
        
        // 读取 /proc/<pid>/cmdline
        char path[64], cmdline[256] = {0};
        snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        FILE* fp = fopen(path, "r");
        if (fp) {
            fread(cmdline, 1, sizeof(cmdline) - 1, fp);
            fclose(fp);
            
            // 只匹配主进程（cmdline 完全等于包名）
            // 跳过 com.xxx:service 等子进程
            if (strcmp(cmdline, name) == 0) {
                pid_t pid = atoi(entry->d_name);
                closedir(dir);
                return pid;
            }
        }
    }
    closedir(dir);
    return -1;
}

/**
 * 获取模块基址（读取 /proc/<pid>/maps）
 * 
 * 注意：Linux 6.1+ 使用 Maple Tree 管理 VMA，内核层获取模块基址不可行
 *       必须使用此用户态函数
 * 
 * @param pid   目标进程 PID
 * @param name  模块名（如 libil2cpp.so）
 * @return      模块基址，失败返回 0
 * 
 * 示例：
 *   uint64_t base = getModuleBase(pid, "libil2cpp.so");
 */
inline uint64_t getModuleBase(pid_t pid, const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;
    
    char line[512];
    uint64_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, name)) {
            sscanf(line, "%lx-", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

#endif /* _KPM_DRIVER_TEAR_GAME_H */
