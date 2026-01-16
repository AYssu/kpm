/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TearGame KPM 用户态测试程序
 * 演示如何使用 prctl 接口进行跨进程内存读写
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 * 
 * 编译方法：
 *   aarch64-linux-gnu-g++ -o test KPmTearGame.cpp -static
 */

#include "kpmdriverTearGame.h"

// ==================== 颜色输出 ====================
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"

// ==================== 主函数 ====================
int main(int argc, char* argv[]) {
    printf(COLOR_CYAN "==========================================\n");
    printf("  TearGame KPM v3.3.0 测试\n");
    printf("  作者: 泪心 | t.me/TearGame\n");
    printf("==========================================" COLOR_RESET "\n\n");
    
    // 检查 root 权限
    if (!isRoot()) {
        printf(COLOR_RED "[错误] 需要 root 权限运行" COLOR_RESET "\n");
        return 1;
    }
    printf(COLOR_GREEN "[OK]" COLOR_RESET " Root 权限检查通过\n\n");
    
    // ==================== PID 获取测试 ====================
    printf(COLOR_CYAN "--- PID 获取测试 ---" COLOR_RESET "\n");
    
    const char* packageName = "com.tencent.tmgp.sgame";
    
    // 用户态获取 PID
    pid_t pidUser = getPidByName(packageName);
    printf("用户态 getPidByName: %d\n", pidUser);
    
    // 内核态获取 PID（短包名才有效）
    pid_t pidKernel = getPidByNameKernel(packageName);
    printf("内核态 getPidByNameKernel: %d\n", pidKernel);
    
    // 使用用户态获取的 PID（更可靠）
    pid_t pid = pidUser;
    if (pid <= 0) {
        printf(COLOR_RED "[错误]" COLOR_RESET " 未找到进程: %s\n", packageName);
        printf("请确保王者荣耀正在运行\n");
        return 1;
    }
    printf(COLOR_GREEN "[OK]" COLOR_RESET " 使用 PID: %d\n\n", pid);
    
    // ==================== 模块基址获取测试 ====================
    printf(COLOR_CYAN "--- 模块基址获取测试 ---" COLOR_RESET "\n");
    
    const char* moduleName = "libil2cpp.so";
    
    // 用户态获取模块基址（Linux 6.1+ 只能用这个）
    uint64_t base = getModuleBase(pid, moduleName);
    printf("用户态 getModuleBase: 0x%lx\n", base);
    
    if (base == 0) {
        printf(COLOR_RED "[错误]" COLOR_RESET " 未找到模块: %s\n", moduleName);
        return 1;
    }
    printf(COLOR_GREEN "[OK]" COLOR_RESET " 模块基址: 0x%lx\n\n", base);

    // ==================== 内存读取测试 ====================
    printf(COLOR_CYAN "--- 内存读取测试 (常规内核读取方案) ---" COLOR_RESET "\n");
    
    // 读取 int
    int val32 = read<int>(pid, base);
    printf(COLOR_GREEN "[OK]" COLOR_RESET " read<int>(0x%lx) = 0x%08x\n", base, val32);
    
    // 读取 uint64_t
    uint64_t val64 = read<uint64_t>(pid, base);
    printf(COLOR_GREEN "[OK]" COLOR_RESET " read<uint64_t>(0x%lx) = 0x%016lx\n", base, val64);
    
    // 读取字节数组
    uint8_t bytes[16] = {0};
    if (readMem(pid, base, bytes, sizeof(bytes)) == 0) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " readMem(0x%lx, 16) = ", base);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", bytes[i]);
        }
        printf("\n");
    }
    
    // ==================== 物理内存读取测试 ====================
    printf("\n" COLOR_CYAN "--- 物理内存读取测试 (页表遍历) ---" COLOR_RESET "\n");
    
    // 物理内存读取 int
    int val32Safe = readSafe<int>(pid, base);
    printf(COLOR_GREEN "[OK]" COLOR_RESET " readSafe<int>(0x%lx) = 0x%08x\n", base, val32Safe);
    
    // 物理内存读取 uint64_t
    uint64_t val64Safe = readSafe<uint64_t>(pid, base);
    printf(COLOR_GREEN "[OK]" COLOR_RESET " readSafe<uint64_t>(0x%lx) = 0x%016lx\n", base, val64Safe);
    
    // 物理内存读取字节数组
    uint8_t bytesSafe[16] = {0};
    if (readMemSafe(pid, base, bytesSafe, sizeof(bytesSafe)) == 0) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " readMemSafe(0x%lx, 16) = ", base);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", bytesSafe[i]);
        }
        printf("\n");
    } else {
        printf(COLOR_YELLOW "[警告]" COLOR_RESET " readMemSafe 失败\n");
    }
    
    // 对比两种方式结果
    printf("\n" COLOR_CYAN "--- 两种方式对比 ---" COLOR_RESET "\n");
    if (val32 == val32Safe && val64 == val64Safe) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 两种读取方式结果一致\n");
    } else {
        printf(COLOR_YELLOW "[警告]" COLOR_RESET " 两种读取方式结果不一致\n");
    }
    
    // ==================== 内存写入测试 ====================
    printf("\n" COLOR_CYAN "--- 内存写入测试 (常规内核写入方案) ---" COLOR_RESET "\n");
    printf(COLOR_YELLOW "[注意]" COLOR_RESET " libil2cpp.so 代码段是只读的，写入测试可能失败\n");
    printf(COLOR_YELLOW "[注意]" COLOR_RESET " 这是正常的，只有数据段/堆/栈才能写入\n\n");
    
    // 测试地址：base + 0x8 (ELF头，只读区域)
    uint64_t testAddr = base + 0x8;
    
    // 保存原值
    int origVal = read<int>(pid, testAddr);
    printf(COLOR_CYAN "[INFO]" COLOR_RESET " 测试地址: 0x%lx, 原始值: 0x%08x\n", testAddr, origVal);
    
    // 写入测试值
    int newVal = 0xDEADBEEF;
    if (write<int>(pid, testAddr, newVal)) {
        int readBack = read<int>(pid, testAddr);
        if (readBack == newVal) {
            printf(COLOR_GREEN "[OK]" COLOR_RESET " 写入成功: 0x%08x -> 读回: 0x%08x\n", newVal, readBack);
        } else {
            printf(COLOR_YELLOW "[警告]" COLOR_RESET " 写入后读回不一致: 期望 0x%08x, 实际 0x%08x\n", newVal, readBack);
        }
        
        // 恢复原值
        write<int>(pid, testAddr, origVal);
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 已恢复原值\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 写入失败（代码段只读，这是正常的）\n");
    }
    
    // ==================== 物理内存写入测试 ====================
    printf("\n" COLOR_CYAN "--- 物理内存写入测试 ---" COLOR_RESET "\n");
    
    // 保存原值
    int origValSafe = readSafe<int>(pid, testAddr);
    printf(COLOR_CYAN "[INFO]" COLOR_RESET " 测试地址: 0x%lx, 原始值: 0x%08x\n", testAddr, origValSafe);
    
    // 物理内存写入测试值
    int newValSafe = 0xCAFEBABE;
    if (writeSafe<int>(pid, testAddr, newValSafe)) {
        int readBackSafe = readSafe<int>(pid, testAddr);
        if (readBackSafe == newValSafe) {
            printf(COLOR_GREEN "[OK]" COLOR_RESET " 物理写入成功: 0x%08x -> 读回: 0x%08x\n", newValSafe, readBackSafe);
        } else {
            printf(COLOR_YELLOW "[警告]" COLOR_RESET " 物理写入后读回不一致: 期望 0x%08x, 实际 0x%08x\n", newValSafe, readBackSafe);
        }
        
        // 恢复原值
        writeSafe<int>(pid, testAddr, origValSafe);
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 已恢复原值\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 物理写入失败（代码段只读，这是正常的）\n");
    }
    
    // ==================== 完成 ====================
    printf("\n" COLOR_GREEN "==========================================\n");
    printf("  测试完成！\n");
    printf("==========================================" COLOR_RESET "\n");
    
    return 0;
}
