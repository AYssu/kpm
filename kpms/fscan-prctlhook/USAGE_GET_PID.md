# PRCTL_PROCESS_PID 使用说明

## 功能概述

`PRCTL_PROCESS_PID` 是一个通过 prctl 系统调用根据进程名获取进程 PID 的功能。

## 接口定义

### 结构体

```c
struct process_pid {
    char *taskname;    // 输入：要查找的进程名
    pid_t pid;         // 输出：内核回写的进程 PID
};
```

### 命令码

```c
#define PRCTL_PROCESS_PID   0x4D454D03  // "MEM\x03"
```

## 使用方法

### C/C++ 代码示例

```c
#include <sys/prctl.h>
#include "fscan_prctl.h"

int get_pid_by_name(const char *process_name) {
    struct process_pid pp;
    int ret;
    
    // 准备结构体
    pp.taskname = (char *)process_name;
    pp.pid = 0;
    
    // 调用 prctl
    ret = prctl(PRCTL_PROCESS_PID, &pp, 0, 0, 0);
    
    if (ret == 0) {
        printf("进程 '%s' 的 PID 是: %d\n", process_name, pp.pid);
        return pp.pid;
    } else {
        printf("进程 '%s' 未找到\n", process_name);
        return -1;
    }
}
```

### 查找进程示例

```c
// 查找 system_server (Android)
int pid1 = get_pid_by_name("system_server");

// 查找 com.android.systemui
int pid2 = get_pid_by_name("com.android.systemui");

// 查找任意进程
int pid3 = get_pid_by_name("zygote64");
```

## 编译和运行

### 编译示例程序

```bash
# 编译
gcc -o example_get_pid example_get_pid.c

# 运行（需要 root 权限）
su -c ./example_get_pid system_server
# 或
sudo ./example_get_pid system_server
```

### 在 Android 上使用

```bash
# 1. 推送到设备
adb push example_get_pid /data/local/tmp/
adb shell chmod +x /data/local/tmp/example_get_pid

# 2. 以 root 权限运行
adb shell su -c /data/local/tmp/example_get_pid system_server
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功，`pp.pid` 包含找到的进程 PID |
| -EPERM | 权限不足（需要 root） |
| -ESRCH | 进程不存在 |
| -EFAULT | 内存访问错误 |

## 注意事项

1. **需要 root 权限** - 必须以 root 用户身份运行
2. **进程名匹配** - 使用部分匹配（`strstr`），例如搜索 "system" 会匹配 "system_server"
3. **首次匹配** - 如果有多个进程匹配，返回第一个找到的
4. **PID 范围** - 搜索 PID 范围 0-32767

## 常见用途

### 1. 查找 Android 系统进程

```c
get_pid_by_name("system_server");
get_pid_by_name("surfaceflinger");
get_pid_by_name("zygote");
```

### 2. 查找应用进程

```c
get_pid_by_name("com.android.phone");
get_pid_by_name("com.tencent.mm");  // 微信
```

### 3. 配合内存读写

```c
// 1. 先获取进程 PID
struct process_pid pp;
pp.taskname = "target_app";
pp.pid = 0;
prctl(PRCTL_PROCESS_PID, &pp, 0, 0, 0);

// 2. 使用获取的 PID 进行内存读写
struct mem_operation op;
op.target_pid = pp.pid;
op.addr = 0x12345678;
op.buffer = my_buffer;
op.size = 1024;

prctl(PRCTL_MEM_READ, &op, 0, 0, 0);
```

## 工作原理

1. 用户空间传入 `process_pid` 结构体，包含要查找的进程名
2. 内核模块遍历所有进程（PID 0-32767）
3. 使用 `find_task_by_vpid` 获取 task_struct
4. 比对进程名（comm 字段）
5. 找到匹配的进程后，将 PID 回写到结构体
6. 返回成功或失败状态

## 调试

### 启用日志

在 `fscan.c` 中设置：

```c
#define ENABLE_DEBUG_LOG 1
```

然后查看内核日志：

```bash
# Android
adb shell su -c dmesg | grep FastScan

# Linux
dmesg | grep FastScan
```

### 日志输出示例

```
[FastScan] get_process_pid: searching for process 'system_server'
[FastScan] get_process_pid: found process 'system_server' with PID 1234
[FastScan] prctl get_pid successful: 'system_server' -> PID 1234
```

## 完整流程图

```
用户空间                   内核空间
   |                         |
   | prctl(PRCTL_PROCESS_PID)|
   |------------------------>|
   |                         | 验证权限 (root)
   |                         | 复制 process_pid 结构体
   |                         | 复制进程名字符串
   |                         | 调用 get_process_pid()
   |                         |   遍历所有进程
   |                         |   匹配进程名
   |                         |   找到 PID
   |                         | 回写 PID 到用户空间
   |<------------------------|
   | pp.pid 已更新           |
```

## 错误排查

### 问题：返回 -EPERM

**原因**：权限不足  
**解决**：使用 `su -c` 或 `sudo` 以 root 身份运行

### 问题：返回 -ESRCH

**原因**：进程不存在  
**解决**：
1. 检查进程名是否正确
2. 使用 `ps -A | grep 进程名` 确认进程存在

### 问题：prctl 返回 -1，errno 是 EINVAL

**原因**：内核模块未加载  
**解决**：
```bash
# 加载模块
kpm load fscan-prctl.kpm
# 或
insmod fscan.ko
```

