#ifndef AMELIAPRO_H
#define AMELIAPRO_H

#include <sys/types.h>
#include <cstdint>
#include <cstddef>

class AmeLiaDriver {
public:
    /* 构造函数 别动！！！*/
    AmeLiaDriver();
    /* 清理函数 别动！！！*/
    ~AmeLiaDriver();
    
    /* 初始化pid */
    bool initialize(pid_t pid = -1);
    
    /* 读写稳定性 新方法硬件读写 > 硬件读写 > 普通读写 */
    /* 请参考您是哪个游戏的辅助再做选择 */
    /* 读写效率 新方法硬件读写 ≈ 硬件读写 < 普通读写 */
    
    /* 普通读写 */
    bool read(uintptr_t addr, void* buffer, size_t size);    
    bool write(uintptr_t addr, void* buffer, size_t size);
    /* 硬件读写 */
    bool read_safe(uintptr_t addr, void* buffer, size_t size);
    bool write_safe(uintptr_t addr, void* buffer, size_t size);
    /* 新方法硬件读写 */
    bool read_fast(uintptr_t addr, void* buffer, size_t size);
    bool write_fast(uintptr_t addr, void* buffer, size_t size);
    
    /* 模板方法 部分开发者会用到 如果你不用请忽略别删除 */
    template <typename T>
    T read(uintptr_t addr) {
        T data{};
        if (read(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    template <typename T>
    T read_safe(uintptr_t addr) {
        T data{};
        if (read_safe(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    template <typename T>
    T read_fast(uintptr_t addr) {
        T data{};
        if (read_fast(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    template <typename T>
    T write(uintptr_t addr) {
        T data{};
        if (write(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    template <typename T>
    T write_safe(uintptr_t addr) {
        T data{};
        if (write_safe(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    template <typename T>
    T write_fast(uintptr_t addr) {
        T data{};
        if (write_fast(addr, &data, sizeof(T))) {
            return data;
        }
        return T{};
    }
    
    /* 用户层获取pid */
    pid_t get_pid(const char* package_name);
    /* 内核层获取pid */
    pid_t get_pid_kernel(const char* name);
    /* 新方法用户层获取pid */
    pid_t find_process_pid(const char* process_name);
    
    /* 获取模块基址 */
    uintptr_t get_module_base(const char* module_name);
    /* 重载函数需要传入pid的获取模块基址函数 */
    uintptr_t find_module_base(pid_t pid, const char* module_name);
    
    /* 触摸初始化 必须正确传入屏幕尺寸 */
    bool init_touch(int width, int height);
    /* 触摸按下 用于自瞄 */
    bool touch_down(int x, int y);
    /* 触摸移动 用于自瞄 */
    bool touch_move(int x, int y);
    /* 触摸抬起 用于自瞄 */
    bool touch_up();
    /* 触摸连点 用于枪械连点等新奇操作 */
    bool touch_click(int x, int y, int delay_ms = 50);
    
    /* 陀螺仪控制 传入xy轴即可实现陀螺仪自瞄 */
    bool gyro_inject(float x, float y);
    /* 陀螺仪停止 每次停止自瞄判断必须调用一次 */
    bool gyro_stop();
    
    /* 初始化硬件断点 */
    bool hwbp_init();
    /* 清除所有硬件断点操作 */
    bool hwbp_cleanup();
    /* 设置硬件断点 */
    /* 使用方法: hwbp_set(target_pid, 0x12345678, 0, 4); */
    bool hwbp_set(pid_t pid, uintptr_t addr, int type = 0, int size = 4, int index = -1);
    
    /* 初始化硬件断点方案二 */
    bool real_hwbp_init();
    /* 设置硬件断点 */
    /* 使用方法: real_hwbp_set(target_pid, 0x12345678, ARM64_HWBP_TYPE_INSTRUCTION, ARM64_HWBP_LEN_4_BYTES, ARM64_HWBP_PRIV_EL0_EL1); */
    bool real_hwbp_set(pid_t pid, uintptr_t addr, int type, int len, int priv, int index, uint64_t user_data);
    /* 清除硬件断点 */
    bool real_hwbp_remove(int index);
    
    /** 硬件断点方案一与方案二适配的设备各不相同请自测 **/
private:
    /* 这部分是私有类 别动！！！*/
    class PrivateData;
    PrivateData* pData;
};

#endif // AMELIAPRO_H