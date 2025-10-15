# FastScan 内核模块使用指南

# 用 Ollvm 编译
# export USE_CLANG=1
# export CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang"
# export CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
## 📖 概述

FastScan 是一个使用 KPM (Kernel Patch Module) 框架的内核模块，通过 hook `ioctl` 系统调用实现跨进程内存读取功能。

**主要特性：**
- ✅ UID 白名单验证（只允许特定用户调用）
- ✅ 时间戳验证（3秒时间窗口，防止重放攻击）
- ✅ 使用 `_IOWR` 宏生成的命令码（伪装成视频设备命令）
- ✅ 直接操作物理内存（绕过常规权限检查）
- ✅ 日志完全禁用（ENABLE_DEBUG_LOG = 0）

---

## 🚀 快速开始

### 1. 编译测试工具

```bash
cd /root/kpm/kpms/fscan-syscallhook

# 编译所有测试和示例程序
bash build_examples.sh
```

这会编译三个程序：
- `test_uid` - 查看当前进程的 UID
- `show_ioctl_code` - 展示 ioctl 命令码的编码原理
- `example_usage` - 完整的调用示例

### 2. 查看你的 UID

```bash
./test_uid
```

输出示例：
```
===== 当前进程的 UID 信息 =====
Real UID:      0
Effective UID: 0
用户名:        root
```

### 3. 了解命令码

```bash
./show_ioctl_code
```

会显示：
- 旧命令码 (777) 和新命令码的对比
- 命令码的位字段分析
- 为什么新方式更安全

### 4. 配置内核模块

打开 `fscan.c`，修改第 **309-310** 行（具体行数可能略有不同）：

```c
// 允许的 UID 列表（可以设置多个）
#define ALLOWED_UID_1  0     // root，改为你的 UID
```

如果需要允许多个 UID：
```c
#define ALLOWED_UID_1  0     // root
#define ALLOWED_UID_2  1000  // 你的第二个 UID
#define ALLOWED_UID_3  10123 // Android 应用 UID

// 同时修改验证逻辑（约第 334-338 行）
if (caller_uid == ALLOWED_UID_1 || 
    caller_uid == ALLOWED_UID_2 || 
    caller_uid == ALLOWED_UID_3) {
    uid_valid = 1;
}
```

### 5. 编译和加载内核模块

```bash
# 编译内核模块
make clean
make

# 加载模块
kpm load fscan-syscallhook.kpm

# 检查模块是否加载
kpm list
```

### 6. 测试内核模块

```bash
# 查看自己的内存（测试）
./example_usage $$ 0x$(cat /proc/self/maps | head -1 | awk '{print $1}' | cut -d'-' -f1)

# 读取指定进程的内存
./example_usage <目标PID> <目标地址>
```

---

## 🔧 用户层调用方式

### 方式一：使用头文件（推荐）

这是**最简单和推荐**的方式：

```c
#include "fscan_ioctl.h"  // 包含共享头文件

int main() {
    struct mem_operation mem_op;
    unsigned char buffer[256];
    
    // 设置参数
    mem_op.target_pid = 12345;
    mem_op.addr = 0x7fff12340000;
    mem_op.buffer = buffer;
    mem_op.size = sizeof(buffer);
    
    // 获取时间戳（用于验证）
    int fd_timestamp = (int)time(NULL);
    
    // 调用 ioctl - 使用宏定义的命令码
    int result = ioctl(fd_timestamp, FSCAN_IOC_READ_MEM, &mem_op);
    
    if (result == 0) {
        printf("读取成功！\n");
        // 处理读取的数据...
    } else {
        perror("读取失败");
    }
    
    return 0;
}
```

**编译：**
```bash
gcc -o my_program my_program.c
```

### 方式二：直接使用数值（不推荐）

如果不想依赖头文件，可以直接使用计算好的命令码值：

```c
// 查看命令码的实际值
// 运行 ./show_ioctl_code 获取

#define FSCAN_IOC_READ_MEM  0xC0187699UL  // 实际值，通过 _IOWR 宏计算

struct mem_operation {
    pid_t target_pid;
    uint64_t addr;
    void *buffer;
    uint64_t size;
};

// 使用方式同上...
ioctl(fd_timestamp, FSCAN_IOC_READ_MEM, &mem_op);
```

**注意：** 这种方式维护困难，如果结构体定义改变，命令码也会改变。

---

## 🔐 安全机制详解

### 1. UID 验证

**工作原理：**
```
用户进程 → ioctl 调用 → 内核 hook
                            ↓
                    检查 current->cred->uid.val
                            ↓
                    UID 在白名单？
                    ├─ 是 → 继续
                    └─ 否 → 返回 -EPERM
```

**为什么安全：**
- UID 由内核管理，用户层无法伪造
- 即使 root 用户也需要在白名单中
- 静默拒绝，不留日志痕迹

### 2. 时间戳验证

**工作原理：**
```c
// 用户层获取当前时间戳
int fd = (int)time(NULL);

// 作为 fd 参数传递给 ioctl
ioctl(fd, FSCAN_IOC_READ_MEM, &mem_op);

// 内核侧验证时间差
if (|current_time - received_time| > 3) {
    return -EPERM;  // 超过3秒窗口，拒绝
}
```

**为什么安全：**
- 防止命令重放攻击
- 3秒窗口足够正常使用，但太短无法被扫描工具利用
- 需要系统时间同步

### 3. 伪装的命令码

**旧方式：**
```c
#define OP_READ_MEM  777  // 容易被扫描
```

暴力扫描只需要：
```c
for (cmd = 0; cmd < 1000; cmd++)
    ioctl(fd, cmd, ...);  // 1000 次就能找到
```

**新方式：**
```c
#define FSCAN_IOC_READ_MEM  _IOWR('v', 0x9F, struct mem_operation)
// 实际值: 0xC0187699 (32位整数)
```

暴力扫描需要：
```c
for (cmd = 0; cmd < 0xFFFFFFFF; cmd++)
    ioctl(fd, cmd, ...);  // 42亿次，实际不可行
```

**为什么安全：**
- 伪装成视频设备命令 (type='v')
- 包含结构体大小信息
- 看起来像合法的系统调用

---

## 📊 命令码详解

### _IOWR 宏的编码

```c
_IOWR('v', 0x9F, struct mem_operation)
```

生成的 32 位命令码：
```
┌──────────┬───────────┬──────────┬──────────┐
│ 方向(2位)│ 大小(14位)│ 类型(8位)│ 序号(8位)│
├──────────┼───────────┼──────────┼──────────┤
│    11    │   24 (0x18)│ 118 (0x76)│ 159 (0x9F)│
│ READ|WRITE│ 结构体大小│    'v'    │  命令号  │
└──────────┴───────────┴──────────┴──────────┘

最终值: 0xC0187699
```

### 各字段含义

- **方向** (2位): 3 = READ|WRITE (可读可写)
- **大小** (14位): 24 = sizeof(struct mem_operation)
- **类型** (8位): 0x76 = 'v' (ASCII码，伪装成视频设备)
- **序号** (8位): 0x9F = 159 (命令序号)

---

## ⚠️ 常见问题

### Q1: ioctl 返回 EPERM (Operation not permitted)

**可能原因：**
1. 当前进程的 UID 不在内核模块的白名单中
2. 时间戳验证失败（系统时间不同步）
3. 时间戳超过3秒窗口

**解决方法：**
```bash
# 检查你的 UID
./test_uid

# 确认内核模块中配置了正确的 UID
grep "ALLOWED_UID" fscan.c

# 检查系统时间
date
```

### Q2: ioctl 返回 EIO (Input/output error)

**可能原因：**
1. 目标进程不存在
2. 目标地址无效或未映射
3. 物理地址转换失败

**解决方法：**
```bash
# 检查目标进程是否存在
ps -p <目标PID>

# 查看目标进程的内存映射
cat /proc/<目标PID>/maps

# 使用有效的地址进行测试
```

### Q3: ioctl 返回 EFAULT (Bad address)

**可能原因：**
1. 用户空间缓冲区地址无效
2. 内核复制数据失败

**解决方法：**
```c
// 确保 buffer 是有效的内存
unsigned char buffer[256];  // 栈上分配
// 或
unsigned char *buffer = malloc(256);  // 堆上分配
```

### Q4: 如何调试内核模块？

**启用日志：**
```c
// 修改 fscan.c
#define ENABLE_DEBUG_LOG 1  // 改为 1

// 重新编译
make clean && make

// 重新加载
kpm unload fscan-syscallhook
kpm load fscan-syscallhook.kpm

// 查看内核日志
dmesg | tail -50
```

**注意：** 调试完成后记得关闭日志（改回 0）！

---

## 🎯 Android 特殊说明

### 查看 Android 应用的 UID

```bash
# 方法1：通过进程列表
adb shell ps | grep com.example.app
# 输出: u0_a123 → UID = 10123

# 方法2：查看 packages.list
adb shell cat /data/system/packages.list | grep com.example.app
# 输出: com.example.app 10123 ...

# 方法3：在应用代码中
int uid = android.os.Process.myUid();
Log.d("UID", "My UID: " + uid);
```

### Android 配置示例

```c
// fscan.c 中配置
#define ALLOWED_UID_1  0      // root (调试用)
#define ALLOWED_UID_2  2000   // shell (adb shell 用)
#define ALLOWED_UID_3  10123  // 你的应用 UID
```

### Android NDK 调用示例

```c
// native-lib.cpp
#include "fscan_ioctl.h"

extern "C" JNIEXPORT jint JNICALL
Java_com_example_app_MainActivity_readMemory(
    JNIEnv* env, jobject, jint pid, jlong addr) {
    
    struct mem_operation mem_op;
    unsigned char buffer[256];
    
    mem_op.target_pid = pid;
    mem_op.addr = addr;
    mem_op.buffer = buffer;
    mem_op.size = sizeof(buffer);
    
    int fd = (int)time(NULL);
    int result = ioctl(fd, FSCAN_IOC_READ_MEM, &mem_op);
    
    return result;
}
```

---

## 📁 文件说明

| 文件 | 说明 |
|------|------|
| `fscan.c` | 内核模块主文件 |
| `fscan_ioctl.h` | 内核/用户层共享头文件（**重要**） |
| `obfuscate.h` | 函数名混淆定义 |
| `Makefile` | 编译脚本 |
| `test_uid.c` | UID 查看工具 |
| `show_ioctl_code.c` | 命令码演示程序 |
| `example_usage.c` | 完整使用示例（**推荐参考**） |
| `build_examples.sh` | 一键编译脚本 |
| `UID_CONFIG.md` | UID 配置详细说明 |
| `USAGE_GUIDE.md` | 本文件，使用指南 |

---

## 🛡️ 安全性总结

| 防护措施 | 实现状态 | 安全等级 |
|---------|---------|---------|
| UID 白名单验证 | ✅ 已实现 | ⭐⭐⭐⭐⭐ |
| 时间戳验证（3秒） | ✅ 已实现 | ⭐⭐⭐⭐ |
| 伪装命令码 | ✅ 已实现 | ⭐⭐⭐⭐ |
| 函数名混淆 | ✅ 已实现 | ⭐⭐⭐ |
| 日志禁用 | ✅ 已实现 | ⭐⭐⭐⭐⭐ |

**检测难度：** 🔴 高

用户层检测工具很难发现此模块，需要：
1. 逆向工程找到命令码
2. 获取正确的 UID
3. 在3秒窗口内调用
4. 了解正确的数据结构

---

## 📝 完整使用流程

```bash
# 1. 编译测试工具
bash build_examples.sh

# 2. 查看 UID
./test_uid

# 3. 了解命令码
./show_ioctl_code

# 4. 修改 fscan.c 配置 UID

# 5. 编译内核模块
make clean && make

# 6. 加载模块
kpm load fscan-syscallhook.kpm

# 7. 测试
./example_usage <PID> <地址>

# 8. 卸载模块（如果需要）
kpm unload fscan-syscallhook
```

---

## 💡 进阶技巧

### 1. 动态修改允许的 UID

当前实现是编译时固定，如需动态修改，可以实现 control 接口：

```c
// 在 syscall_hook_control0 中添加
static long syscall_hook_control0(const char *args, char *__user out_msg, int outlen)
{
    // 解析命令: "add_uid:1000"
    if (strncmp(args, "add_uid:", 8) == 0) {
        uid_t new_uid = simple_strtol(args + 8, NULL, 10);
        // 添加到动态 UID 列表...
    }
    return 0;
}
```

### 2. 添加进程名验证

除了 UID，还可以检查进程名：

```c
// 在 before_ioctl 中添加
if (strcmp(current->comm, "expected_process") != 0) {
    args->ret = -EPERM;
    return;
}
```

### 3. 自定义验证协议

实现更复杂的验证机制：

```c
struct mem_operation {
    // ... 原有字段 ...
    uint32_t magic;        // 魔术数
    uint32_t checksum;     // 校验和
    uint64_t session_key;  // 会话密钥
};
```

---

## 🚀 性能优化

- ✅ UID 验证开销：< 1 微秒
- ✅ 时间戳验证开销：< 1 微秒  
- ⚠️ 内存读取开销：取决于大小，建议 < 4KB/次
- 💡 分页读取已实现，自动处理跨页问题

---

## 📞 支持

如有问题，请检查：
1. 内核日志：`dmesg | grep FastScan`
2. 模块状态：`kpm list`
3. UID 配置：`grep ALLOWED_UID fscan.c`
4. 编译错误：查看 make 输出

祝使用愉快！🎉

