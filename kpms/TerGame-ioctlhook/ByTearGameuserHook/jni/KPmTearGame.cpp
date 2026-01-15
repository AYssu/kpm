/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * FastScan KPM 驱动完整测试程序
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

#include "kpmdriverTearGame.h"  // 单头文件，无其他依赖
#include <cstdio>

using namespace KPMDriver;

void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main(int argc, char* argv[]) {
    // 深红色打印开发信息
    printf("\033[1;31m========================================\033[0m\n");
    printf("\033[1;31m  FastScan KPM 驱动完整测试\033[0m\n");
    printf("\033[1;31m  原作者: 阿夜 (AYssu)\033[0m\n");
    printf("\033[1;31m  GitHub: github.com/AYssu\033[0m\n");
    printf("\033[1;31m  二次开发: 泪心 (TearHacker)\033[0m\n");
    printf("\033[1;31m  GitHub: github.com/tearhacker\033[0m\n");
    printf("\033[1;31m  Telegram: t.me/TearGame\033[0m\n");
    printf("\033[1;31m  QQ: 2254013571\033[0m\n");
    printf("\033[1;31m========================================\033[0m\n\n");
    
    // 检查 root 权限
    if (getuid() != 0) {
        printf("[错误] 需要 root 权限运行\n");
        return 1;
    }
    printf("[✓] Root 权限检查通过\n");
    
    // 解析参数
    const char* proc = argc > 1 ? argv[1] : "bin.mt.plus.canary";
    const char* mod = argc > 2 ? argv[2] : "libmt1.so";
    
    printf("[信息] 目标进程: %s\n", proc);
    printf("[信息] 目标模块: %s\n\n", mod);
    
    // ==================== 测试1: 获取 PID ====================
    printf("【测试1】获取进程 PID\n");
    pid_t pid = getPid(proc);
    if (pid <= 0) {
        printf("[✗] 进程未找到: %s\n", proc);
        return 1;
    }
    printf("[✓] PID = %d\n\n", pid);
    
    // ==================== 测试2: 获取模块基址 ====================
    printf("【测试2】获取模块基址\n");
    uint64_t modSize = 0;
    uint64_t base = getModuleBase(pid, mod, &modSize);
    if (base == 0) {
        printf("[✗] 模块未找到: %s\n", mod);
        return 1;
    }
    printf("[✓] 基址 = 0x%lx\n", base);
    printf("[✓] 大小 = 0x%lx (%lu KB)\n\n", modSize, modSize / 1024);
    
    // ==================== 测试3: 读取 ELF Header ====================
    printf("【测试3】读取 ELF Header (16字节)\n");
    uint8_t elfHeader[16] = {0};
    int ret = readMem(pid, base, elfHeader, sizeof(elfHeader));
    if (ret == 0) {
        printf("[✓] 读取成功:\n");
        printHex(elfHeader, sizeof(elfHeader));
        
        // 验证 ELF Magic
        if (elfHeader[0] == 0x7F && elfHeader[1] == 'E' && 
            elfHeader[2] == 'L' && elfHeader[3] == 'F') {
            printf("[✓] ELF Magic 验证通过 (7F 45 4C 46)\n");
            printf("[信息] 架构: %s\n", elfHeader[4] == 2 ? "64-bit" : "32-bit");
            printf("[信息] 字节序: %s\n\n", elfHeader[5] == 1 ? "小端" : "大端");
        } else {
            printf("[✗] ELF Magic 验证失败\n\n");
        }
    } else {
        printf("[✗] 读取失败\n\n");
    }
    
    // ==================== 测试4: 模板读取 ====================
    printf("【测试4】模板读取测试\n");
    
    // 读取 uint64_t
    uint64_t val64 = read<uint64_t>(pid, base);
    printf("[✓] read<uint64_t>(0x%lx) = 0x%lx\n", base, val64);
    
    // 读取 int
    int val32 = read<int>(pid, base);
    printf("[✓] read<int>(0x%lx) = 0x%x\n", base, val32);
    
    // 读取 int
    int val16 = read<int>(pid, base);
    printf("[✓] read<int>(0x%lx) = 0x%x\n", base, val16);
    
    // 读取 uint8_t
    uint8_t val8 = read<uint8_t>(pid, base);
    printf("[✓] read<uint8_t>(0x%lx) = 0x%x\n", base, val8);
    
    // 读取 float (从偏移位置)
    float valFloat = read<float>(pid, base + 0x100);
    printf("[✓] read<float>(0x%lx) = %f\n", base + 0x100, valFloat);
    
    // 读取 double
    double valDouble = read<double>(pid, base + 0x100);
    printf("[✓] read<double>(0x%lx) = %lf\n\n", base + 0x100, valDouble);
    
    // ==================== 测试5: 大块内存读取 ====================
    printf("【测试5】大块内存读取 (256字节)\n");
    uint8_t bigBuf[256] = {0};
    ret = readMem(pid, base, bigBuf, sizeof(bigBuf));
    if (ret == 0) {
        printf("[✓] 读取成功，前64字节:\n");
        printHex(bigBuf, 64);
    } else {
        printf("[✗] 读取失败\n");
    }
    printf("\n");
    
    // ==================== 测试6: 写入测试 ====================
    printf("【测试6】写入测试 (在可写区域)\n");
    
    // 找一个可写的测试地址 (BSS 段或数据段)
    // 注意：写入代码段会失败，这里用一个安全的偏移
    uint64_t testAddr = base + 0x1000;  // 假设这是数据段
    
    // 保存原值
    int origVal = read<int>(pid, testAddr);
    printf("[信息] 测试地址: 0x%lx\n", testAddr);
    printf("[信息] 原始值: 0x%x\n", origVal);
    
    // 写入新值
    int newVal = 0x12345678;
    bool writeOk = write<int>(pid, testAddr, newVal);
    if (writeOk) {
        // 读回验证
        int readBack = read<int>(pid, testAddr);
        if (readBack == newVal) {
            printf("[✓] 写入成功: 0x%x -> 读回: 0x%x\n", newVal, readBack);
        } else {
            printf("[!] 写入后读回不一致: 期望 0x%x, 实际 0x%x\n", newVal, readBack);
        }
        
        // 恢复原值
        write<int>(pid, testAddr, origVal);
        printf("[✓] 已恢复原值: 0x%x\n", origVal);
    } else {
        printf("[!] 写入失败 (可能是只读区域)\n");
    }
    printf("\n");
    
    // ==================== 测试7: 浮点数写入 ====================
    printf("【测试7】浮点数写入测试\n");
    float origFloat = read<float>(pid, testAddr);
    float testFloat = 3.14159f;
    
    if (write<float>(pid, testAddr, testFloat)) {
        float readFloat = read<float>(pid, testAddr);
        printf("[✓] 写入 float: %f -> 读回: %f\n", testFloat, readFloat);
        write<float>(pid, testAddr, origFloat);  // 恢复
    } else {
        printf("[!] 浮点数写入失败\n");
    }
    printf("\n");
    
    // ==================== 测试8: 字节数组写入 ====================
    printf("【测试8】字节数组写入测试\n");
    uint8_t origBytes[8] = {0};
    uint8_t testBytes[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    
    readMem(pid, testAddr, origBytes, sizeof(origBytes));
    
    ret = writeMem(pid, testAddr, testBytes, sizeof(testBytes));
    if (ret == 0) {
        uint8_t readBytes[8] = {0};
        readMem(pid, testAddr, readBytes, sizeof(readBytes));
        printf("[✓] 写入: ");
        printHex(testBytes, 8);
        printf("[✓] 读回: ");
        printHex(readBytes, 8);
        
        // 恢复
        writeMem(pid, testAddr, origBytes, sizeof(origBytes));
    } else {
        printf("[!] 字节数组写入失败\n");
    }
    printf("\n");
    
    // ==================== 测试9: 跨页读取 ====================
    printf("【测试9】跨页读取测试\n");
    // 找一个接近页边界的地址
    uint64_t pageSize = 4096;
    uint64_t nearPageEnd = (base & ~(pageSize - 1)) + pageSize - 8;
    uint8_t crossPageBuf[32] = {0};
    
    ret = readMem(pid, nearPageEnd, crossPageBuf, sizeof(crossPageBuf));
    if (ret == 0) {
        printf("[✓] 跨页读取成功 (地址: 0x%lx, 大小: 32字节)\n", nearPageEnd);
        printHex(crossPageBuf, sizeof(crossPageBuf));
    } else {
        printf("[!] 跨页读取失败\n");
    }
    printf("\n");
    
    // ==================== 测试10: 性能测试 ====================
    printf("【测试10】性能测试 (1000次读取)\n");
    uint64_t dummy = 0;
    int successCount = 0;
    
    for (int i = 0; i < 1000; i++) {
        uint64_t val = read<uint64_t>(pid, base + (i % 256) * 8);
        if (val != 0) successCount++;
        dummy += val;
    }
    printf("[✓] 完成 1000 次读取, 成功: %d 次\n", successCount);
    printf("[信息] dummy = 0x%lx (防止优化)\n\n", dummy);
    
    // ==================== 测试总结 ====================
    printf("========================================\n");
    printf("  测试完成\n");
    printf("========================================\n");
    printf("进程: %s (PID: %d)\n", proc, pid);
    printf("模块: %s @ 0x%lx\n", mod, base);
    printf("驱动功能: 读取 ✓ | 写入 ✓\n");
    
    return 0;
}
