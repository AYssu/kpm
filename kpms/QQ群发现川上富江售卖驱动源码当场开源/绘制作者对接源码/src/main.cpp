#include <stdio.h>
#include <unistd.h>
#include "AmeliaPro.h"
using namespace KPMDriver;

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_MAGENTA "\033[1;35m"

int main(int argc, char* argv[]) {
    printf(COLOR_CYAN "==========================================\n");
    printf("   AmeliaPro KPM Driver\n");
    printf("   作者: 川上富江\n");
    printf("==========================================" COLOR_RESET "\n\n");
    
    std::string version = checkVersion();
    if (!version.empty()) {
        printf(COLOR_GREEN "[成功] 驱动头静态库版本验证成功" COLOR_RESET "\n");
        printf(COLOR_CYAN "驱动版本: %s" COLOR_RESET "\n", version.c_str());
    } else {
        printf(COLOR_RED "[失败] 驱动头静态库版本验证失败" COLOR_RESET "\n");
        printf(COLOR_YELLOW "可能原因: 未加载内核模块或权限不足" COLOR_RESET "\n");
    }
    
    if (!isRoot()) {
        printf(COLOR_RED "[错误] 需要 root 权限运行" COLOR_RESET "\n");
        return 1;
    }
    printf(COLOR_GREEN "[OK]" COLOR_RESET " Root 权限检查通过\n\n");
    
    printf(COLOR_CYAN "--- 基本功能测试 ---" COLOR_RESET "\n");
    
    const char* packageName = "com.tencent.tmgp.pubgmhd";
    pid_t pidUser = getPidByName(packageName);
    printf("用户态 getPidByName: %d\n", pidUser);
    
    pid_t pid = pidUser;
    if (pid <= 0) {
        printf(COLOR_RED "[错误]" COLOR_RESET " 未找到进程: %s\n", packageName);
        return 1;
    }
    
    setTargetPid(pid);
    printf(COLOR_GREEN "[OK]" COLOR_RESET " 使用 PID: %d\n", pid);
    
    const char* moduleName = "libUE4.so";
    uint64_t base = getModuleBase(pid, moduleName);
    if (base == 0) {
        printf(COLOR_RED "[错误]" COLOR_RESET " 未找到模块: %s\n", moduleName);
        return 1;
    }
    printf(COLOR_GREEN "[OK]" COLOR_RESET " 模块基址: 0x%lx\n\n", base);
    
    printf(COLOR_CYAN "--- 陀螺仪注入功能测试 ---" COLOR_RESET "\n");
    printf(COLOR_MAGENTA "[注意]" COLOR_RESET " 陀螺仪注入为系统级功能，需要函数地址\n\n");
    
    SystemGyroInjector gyroInjector;
    
    printf(COLOR_CYAN "1. 查找陀螺仪HAL函数地址 ---" COLOR_RESET "\n");
    printf("假设陀螺仪get_data函数地址: 0x%lx\n", base + 0x100000);
    printf("假设陀螺仪set_data函数地址: 0x%lx\n", base + 0x110000);
    
    uint64_t get_func_addr = base + 0x100000;
    uint64_t set_func_addr = base + 0x110000;
    
    printf("\n" COLOR_CYAN "2. 初始化陀螺仪注入器 ---" COLOR_RESET "\n");
    if (gyroInjector.start(get_func_addr, set_func_addr)) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 陀螺仪Hook成功\n");
    } else {
        printf(COLOR_YELLOW "[警告]" COLOR_RESET " 陀螺仪Hook失败，继续模拟测试\n");
    }
    
    printf("\n" COLOR_CYAN "3. 模拟陀螺仪注入测试 ---" COLOR_RESET "\n");
    printf("当前Hook状态: %s\n", gyroInjector.isHooked() ? "已Hook" : "未Hook");
    
    printf("\n" COLOR_CYAN "4. 测试直接调用陀螺仪注入函数 ---" COLOR_RESET "\n");
    
    printf("a) 测试注入陀螺仪数据 (x=1.5, y=2.3)...\n");
    if (setGyroData(1.5f, 2.3f) == 0) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 陀螺仪数据注入成功\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 陀螺仪注入失败（未Hook或地址错误）\n");
    }
    
    printf("b) 测试注入陀螺仪数据 (x=0.0, y=0.0)...\n");
    if (setGyroData(0.0f, 0.0f) == 0) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 陀螺仪数据重置成功\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 陀螺仪重置失败\n");
    }
    
    printf("\n" COLOR_CYAN "5. 测试陀螺仪注入器类功能 ---" COLOR_RESET "\n");
    
    printf("a) 测试注入模式...\n");
    if (gyroInjector.inject(3.0f, -1.5f)) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 通过注入器类注入成功\n");
        auto data = gyroInjector.getCurrentData();
        printf("   当前注入数据: x=%.2f, y=%.2f\n", data.first, data.second);
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 注入器类注入失败\n");
    }
    
    printf("b) 注入状态检查...\n");
    printf("   Hook状态: %s\n", gyroInjector.isHooked() ? "已Hook" : "未Hook");
    printf("   注入状态: %s\n", gyroInjector.isInjecting() ? "正在注入" : "未注入");
    
    printf("c) 停止注入...\n");
    if (gyroInjector.stopInjection()) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 停止注入成功\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 停止注入失败\n");
    }
    
    printf("d) 停止Hook...\n");
    if (gyroInjector.stop()) {
        printf(COLOR_GREEN "[OK]" COLOR_RESET " 陀螺仪Hook已停止\n");
    } else {
        printf(COLOR_YELLOW "[预期]" COLOR_RESET " 停止Hook失败\n");
    }
    
    printf("\n" COLOR_CYAN "6. 陀螺仪注入测试模式 ---" COLOR_RESET "\n");
    printf("模拟连续注入5秒...\n");
    
    if (gyroInjector.start(get_func_addr, set_func_addr)) {
        printf("开始连续注入测试...\n");
        for (int i = 0; i < 5; i++) {
            float x = (float)(i * 0.5f);
            float y = (float)(i * -0.3f);
            
            if (gyroInjector.inject(x, y)) {
                printf("   [%d秒] 注入: x=%.2f, y=%.2f\n", i+1, x, y);
            } else {
                printf("   [%d秒] " COLOR_YELLOW "注入失败" COLOR_RESET "\n", i+1);
            }
            sleep(1);
        }
        
        gyroInjector.stop();
        printf("连续注入测试完成\n");
    } else {
        printf(COLOR_YELLOW "[跳过]" COLOR_RESET " 无法启动Hook，跳过连续注入测试\n");
    }
    
    printf("\n" COLOR_CYAN "--- 总结 ---" COLOR_RESET "\n");
    printf("陀螺仪注入功能测试完成！\n");
    printf("实际使用时需要：\n");
    printf("1. 获取正确的陀螺仪HAL函数地址\n");
    printf("2. 调用hookGyroSystem()设置Hook\n");
    printf("3. 调用setGyroData()注入数据\n");
    printf("4. 注入(0,0)恢复原始数据\n");
    
    printf("\n" COLOR_GREEN "==========================================\n");
    printf("  测试完成！\n");
    printf("==========================================" COLOR_RESET "\n");
    
    return 0;
}