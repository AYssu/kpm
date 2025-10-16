# PRCTL_MODULE_BASE 使用说明

## 功能概述

`PRCTL_MODULE_BASE` 是一个通过 prctl 系统调用获取进程中模块（so库/可执行文件）基址的功能。

## 接口定义

### 结构体

```c
struct module_base {
    pid_t pid;              // 输入：目标进程 PID
    char *module_name;      // 输入：模块名字符串
    uint64_t base_address;  // 输出：模块基址（内核回写）
};
```

### 命令码

```c
#define PRCTL_MODULE_BASE   0x4D454D04  // "MEM\x04"
```

## 使用方法

### C/C++ 代码示例

```c
#include <sys/prctl.h>
#include "fscan_prctl.h"

uint64_t get_module_base_addr(pid_t pid, const char *module_name) {
    struct module_base mb;
    int ret;
    
    // 准备结构体
    mb.pid = pid;
    mb.module_name = (char *)module_name;
    mb.base_address = 0;
    
    // 调用 prctl
    ret = prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0);
    
    if (ret == 0) {
        printf("模块 '%s' 的基址: 0x%lx\n", module_name, mb.base_address);
        return mb.base_address;
    } else {
        printf("查找失败\n");
        return 0;
    }
}
```

### 查找模块示例

```c
// 查找 libc.so (Android/Linux)
uint64_t libc_base = get_module_base_addr(1234, "libc.so");

// 查找 ART 虚拟机 (Android)
uint64_t art_base = get_module_base_addr(1234, "libart.so");

// 查找 Unity 引擎
uint64_t unity_base = get_module_base_addr(1234, "libunity.so");

// 查找应用主模块
uint64_t app_base = get_module_base_addr(1234, "libapp.so");
```

## 编译和运行

### 编译示例程序

```bash
# 编译
gcc -o example_module_base example_module_base.c

# 运行（需要 root 权限）
su -c "./example_module_base 1234 libc.so"
# 或
sudo ./example_module_base 1234 libc.so
```

### 在 Android 上使用

```bash
# 1. 推送到设备
adb push example_module_base /data/local/tmp/
adb shell chmod +x /data/local/tmp/example_module_base

# 2. 查找进程 PID
adb shell ps | grep com.example.app

# 3. 以 root 权限运行
adb shell su -c "/data/local/tmp/example_module_base 1234 libc.so"
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功，`mb.base_address` 包含模块基址 |
| -EPERM | 权限不足（需要 root） |
| -ENOENT | 模块不存在 |
| -ESRCH | 进程不存在 |
| -EFAULT | 内存访问错误 |

## 完整示例：定位并读取函数

```c
#include <sys/prctl.h>
#include "fscan_prctl.h"

int main() {
    pid_t pid = 1234;
    
    // 步骤 1: 获取模块基址
    struct module_base mb;
    mb.pid = pid;
    mb.module_name = "libtarget.so";
    mb.base_address = 0;
    
    if (prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0) != 0) {
        printf("获取模块基址失败\n");
        return 1;
    }
    
    printf("libtarget.so 基址: 0x%lx\n", mb.base_address);
    
    // 步骤 2: 计算函数地址
    // 假设从 IDA/Ghidra 中得知函数偏移为 0x12345
    uint64_t func_offset = 0x12345;
    uint64_t func_addr = mb.base_address + func_offset;
    
    printf("目标函数地址: 0x%lx\n", func_addr);
    
    // 步骤 3: 读取函数代码
    struct mem_operation op;
    unsigned char code[64];
    
    op.target_pid = pid;
    op.addr = func_addr;
    op.buffer = code;
    op.size = sizeof(code);
    
    if (prctl(PRCTL_MEM_READ, &op, 0, 0, 0) == 0) {
        printf("成功读取函数代码:\n");
        for (int i = 0; i < sizeof(code); i++) {
            printf("%02x ", code[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
    }
    
    return 0;
}
```

## 配合其他功能使用

### 示例 1: 获取进程PID → 获取模块基址 → 读取内存

```c
// 1. 获取进程 PID
struct process_pid pp;
pp.taskname = "com.example.app";
pp.pid = 0;
prctl(PRCTL_PROCESS_PID, &pp, 0, 0, 0);

// 2. 获取模块基址
struct module_base mb;
mb.pid = pp.pid;
mb.module_name = "libtarget.so";
mb.base_address = 0;
prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0);

// 3. 读取内存
struct mem_operation op;
uint8_t buffer[1024];
op.target_pid = pp.pid;
op.addr = mb.base_address + 0x1000;
op.buffer = buffer;
op.size = sizeof(buffer);
prctl(PRCTL_MEM_READ, &op, 0, 0, 0);
```

### 示例 2: Hook 函数

```c
// 1. 获取模块基址
struct module_base mb;
mb.pid = target_pid;
mb.module_name = "libtarget.so";
mb.base_address = 0;
prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0);

// 2. 计算目标函数地址
uint64_t target_func = mb.base_address + 0x12345;

// 3. 备份原始代码
unsigned char original_code[16];
struct mem_operation read_op;
read_op.target_pid = target_pid;
read_op.addr = target_func;
read_op.buffer = original_code;
read_op.size = sizeof(original_code);
prctl(PRCTL_MEM_READ, &read_op, 0, 0, 0);

// 4. 写入 hook 代码（例如：跳转指令）
unsigned char hook_code[] = {
    // ARM64 跳转指令
    0x00, 0x00, 0x00, 0x14,  // B +0 (需要计算偏移)
};
struct mem_operation write_op;
write_op.target_pid = target_pid;
write_op.addr = target_func;
write_op.buffer = hook_code;
write_op.size = sizeof(hook_code);
prctl(PRCTL_MEM_WRITE, &write_op, 0, 0, 0);
```

## 常见模块名

### Android 系统库

```c
"libc.so"              // C 标准库
"libc++.so"            // C++ 标准库
"libart.so"            // ART 虚拟机
"libandroid.so"        // Android 原生库
"libGLESv2.so"         // OpenGL ES 2.0
"liblog.so"            // Android 日志库
"libutils.so"          // Android 工具库
"libbinder.so"         // Binder IPC
```

### 游戏引擎

```c
"libunity.so"          // Unity 引擎
"libUE4.so"            // Unreal Engine 4
"libcocos2d.so"        // Cocos2d-x
"libil2cpp.so"         // Unity IL2CPP
```

### 应用模块

```c
"libapp.so"            // 应用主模块
"libnative-lib.so"     // JNI 原生库
"libgame.so"           // 游戏逻辑模块
```

## 模块名匹配规则

- 使用 `strstr` 进行**部分匹配**
- 搜索 "libc" 会匹配 "libc.so"、"libc-2.31.so" 等
- 搜索 ".so" 会匹配所有共享库
- 建议使用尽可能精确的名称避免误匹配

## 查看进程的内存映射

如果不确定模块名，可以查看进程的内存映射：

```bash
# Linux/Android
cat /proc/<pid>/maps

# 或使用 grep 过滤
cat /proc/<pid>/maps | grep ".so"

# 示例输出:
# 7f1234567000-7f12345f8000 r-xp 00000000 08:01 12345  /system/lib64/libc.so
# 7f12345f8000-7f1234607000 ---p 00091000 08:01 12345  /system/lib64/libc.so
# 7f1234607000-7f123460a000 r--p 00090000 08:01 12345  /system/lib64/libc.so
# 7f123460a000-7f123460b000 rw-p 00093000 08:01 12345  /system/lib64/libc.so
```

第一列就是内存地址范围，第一个地址就是模块基址。

## 调试技巧

### 1. 启用调试日志

在 `fscan.c` 中设置：

```c
#define ENABLE_DEBUG_LOG 1
```

### 2. 查看内核日志

```bash
# Android
adb shell su -c dmesg | grep FastScan

# Linux
dmesg | grep FastScan
```

### 3. 日志输出示例

```
[FastScan] get_module_base: pid=1234, module_name='libc.so'
[FastScan] get_module_base: found task: ffff8881a2b4c800
[FastScan] get_module_base: got mm: ffff8881a3d5f000
[FastScan] get_module_base: found vm_file_offset=0x40
[FastScan] get_module_base: found module 'libc.so' at 0x7f1234567000
[FastScan] prctl get_module_base successful: 'libc.so' in pid 1234 -> 0x7f1234567000
```

## 错误排查

### 问题：返回 -ENOENT（模块不存在）

**原因**：
1. 模块名拼写错误
2. 模块未加载到进程中
3. 模块路径不匹配

**解决方法**：
```bash
# 查看进程的所有模块
cat /proc/<pid>/maps | grep ".so"

# 确认模块名
cat /proc/<pid>/maps | grep <module_name>
```

### 问题：返回 -ESRCH（进程不存在）

**原因**：PID 无效或进程已退出

**解决方法**：
```bash
# 确认进程存在
ps -p <pid>

# 或在 Android 上
adb shell ps | grep <pid>
```

### 问题：返回 -EPERM（权限不足）

**原因**：非 root 用户调用

**解决方法**：
```bash
# 使用 root 权限运行
su -c ./example_module_base <pid> <module>
# 或
sudo ./example_module_base <pid> <module>
```

### 问题：vm_file_offset 探测失败

**原因**：进程没有有效的内存映射

**解决方法**：
- 确保目标进程已完全启动
- 尝试先查找一个常见模块（如 "libc"）进行探测
- 检查进程是否有合法的共享库映射

## 性能优化

### 1. 缓存模块基址

```c
// 全局变量缓存
static uint64_t g_libc_base = 0;

uint64_t get_libc_base(pid_t pid) {
    if (g_libc_base == 0) {
        struct module_base mb;
        mb.pid = pid;
        mb.module_name = "libc.so";
        mb.base_address = 0;
        
        if (prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0) == 0) {
            g_libc_base = mb.base_address;
        }
    }
    return g_libc_base;
}
```

### 2. 批量查询

```c
// 一次查询多个模块
struct {
    const char *name;
    uint64_t base;
} modules[] = {
    {"libc.so", 0},
    {"libart.so", 0},
    {"libunity.so", 0},
};

for (int i = 0; i < 3; i++) {
    struct module_base mb;
    mb.pid = target_pid;
    mb.module_name = (char *)modules[i].name;
    mb.base_address = 0;
    
    if (prctl(PRCTL_MODULE_BASE, &mb, 0, 0, 0) == 0) {
        modules[i].base = mb.base_address;
    }
}
```

## API 总结

| 功能 | prctl 命令 | 结构体 | 说明 |
|------|------------|--------|------|
| 读内存 | PRCTL_MEM_READ | struct mem_operation | 读取进程内存 |
| 写内存 | PRCTL_MEM_WRITE | struct mem_operation | 写入进程内存 |
| 获取PID | PRCTL_PROCESS_PID | struct process_pid | 根据进程名获取PID |
| **获取基址** | **PRCTL_MODULE_BASE** | **struct module_base** | **获取模块基址** ✨ |

## 完整工作流程

```
用户空间                          内核空间
   |                                |
   | prctl(PRCTL_MODULE_BASE)      |
   |------------------------------>|
   |                                | 验证权限 (root)
   |                                | 复制 module_base 结构体
   |                                | 复制模块名字符串
   |                                | 调用 get_module_base()
   |                                |   查找进程 task_struct
   |                                |   获取 mm_struct
   |                                |   遍历 VMA
   |                                |   匹配模块名
   |                                |   获取 vm_start
   |                                | 回写基址到用户空间
   |<------------------------------|
   | mb.base_address 已更新        |
```

## 注意事项

1. **需要 root 权限** - 必须以 root 用户身份运行
2. **模块名匹配** - 使用部分匹配，建议使用精确名称
3. **首次探测** - 第一次调用会探测 vm_file_offset，可能稍慢
4. **缓存结果** - 模块基址通常不变，建议缓存避免重复查询
5. **ASLR** - 即使开启 ASLR，每次进程启动基址会变，但运行期间不变

## 相关资源

- [example_module_base.c](example_module_base.c) - 完整示例程序
- [MODULE_BASE_INFO.md](MODULE_BASE_INFO.md) - 内核实现详解
- [fscan_prctl.h](fscan_prctl.h) - API 接口定义

