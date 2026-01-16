# TearGame KPM 用户态驱动接口
禁止收费   收费后果自负 自行承担法律责任！
基于 prctl 系统调用的跨进程内存读写库。

> **原作者**: 阿夜 (AYssu) - https://github.com/AYssu
> 
> **二次开发**: 泪心 (TearHacker)
> - GitHub: https://github.com/tearhacker
> - Telegram: t.me/TearGame
> - QQ: 2254013571

## 功能特性

- ✅ **内存读取** - 读取目标进程的虚拟内存
- ✅ **内存写入** - 写入目标进程的虚拟内存
- ✅ **进程查找** - 根据包名获取 PID（用户态/内核态）
- ✅ **模块基址** - 获取目标进程中指定模块的基址
- ✅ **类型安全** - 提供 C++ 模板函数，自动处理类型转换
- ✅ **物理内存** - 备用物理内存读写接口

## 文件结构

```
prctlUser/jni/
├── kpmdriverTearGame.h      # C++ 头文件（API 接口）
├── KPmTearGame.cpp          # 测试程序
├── Android.mk               # Android NDK 构建文件
└── Application.mk           # NDK 应用配置
```

## 快速开始

### 1. 基本使用

```cpp
#include "kpmdriverTearGame.h"

// 检查 root 权限
if (!isRoot()) {
    printf("需要 root 权限\n");
    return -1;
}

// 获取进程 PID
pid_t pid = getPidByName("com.tencent.tmgp.sgame");
if (pid <= 0) {
    printf("进程未找到\n");
    return -1;
}

// 获取模块基址
uint64_t base = getModuleBase(pid, "libil2cpp.so");

// 读取内存
int hp = read<int>(pid, base + 0x1234);
printf("HP = %d\n", hp);

// 写入内存
write<int>(pid, base + 0x1234, 9999);
```

### 2. 模板接口（推荐）

```cpp
// 读取任意类型
int val = read<int>(pid, addr);
float speed = read<float>(pid, addr + 0x10);
uint64_t ptr = read<uint64_t>(pid, addr + 0x20);

// 写入任意类型
write<int>(pid, addr, 9999);
write<float>(pid, addr + 0x10, 100.0f);
```

### 3. 原始接口

```cpp
// 读取字节数组
uint8_t buffer[64];
readMem(pid, addr, buffer, sizeof(buffer));

// 写入字节数组
uint8_t data[] = {0x90, 0x90, 0x90};
writeMem(pid, addr, data, sizeof(data));
```

### 4. 物理内存接口（备用）

```cpp
// 物理内存读取
int val = readSafe<int>(pid, addr);

// 物理内存写入
writeSafe<int>(pid, addr, 9999);
```

## API 参考

### 内存读写

| 函数 | 说明 |
|------|------|
| `readMem(pid, addr, buf, len)` | 读取内存（原始接口） |
| `writeMem(pid, addr, buf, len)` | 写入内存（原始接口） |
| `read<T>(pid, addr)` | 读取任意类型（模板） |
| `write<T>(pid, addr, val)` | 写入任意类型（模板） |
| `readMemSafe(pid, addr, buf, len)` | 物理内存读取 |
| `writeMemSafe(pid, addr, buf, len)` | 物理内存写入 |
| `readSafe<T>(pid, addr)` | 物理内存读取（模板） |
| `writeSafe<T>(pid, addr, val)` | 物理内存写入（模板） |

### 进程/模块

| 函数 | 说明 |
|------|------|
| `getPidByName(name)` | 用户态获取 PID（读取 /proc） |
| `getPidByNameKernel(name)` | 内核态获取 PID（仅短包名） |
| `getModuleBase(pid, name)` | 获取模块基址 |
| `isRoot()` | 检查 root 权限 |

## 编译说明

### 交叉编译（推荐）

```bash
aarch64-linux-gnu-g++ -o test KPmTearGame.cpp -static
```

### Android NDK 编译

```bash
cd jni
ndk-build
```

## prctl 命令码

| 命令 | 值 | 说明 |
|------|-----|------|
| `PRCTL_MEM_READ` | 0x4D454D01 | 内存读取 |
| `PRCTL_MEM_WRITE` | 0x4D454D02 | 内存写入 |
| `PRCTL_GET_PID` | 0x4D454D03 | 获取 PID |
| `PRCTL_MEM_READ_SAFE` | 0x4D454D04 | 物理内存读取 |
| `PRCTL_MEM_WRITE_SAFE` | 0x4D454D05 | 物理内存写入 |

## 注意事项

1. **Root 权限**：所有操作都需要 root 权限
2. **模块加载**：使用前确保已加载 TearGame.kpm 内核模块
3. **只读区域**：代码段（.text）是只读的，写入会失败
4. **短包名限制**：内核态 `getPidByNameKernel` 只支持 ≤15 字符的包名
5. **Linux 6.1+**：内核层无法获取模块基址，必须使用用户态 `getModuleBase`

## 错误码

| 错误码 | 说明 |
|--------|------|
| 0 | 成功 |
| -EFAULT | 内存访问错误 |
| -EIO | I/O 错误 |
| -ESRCH | 进程不存在 |
| -EINVAL | 参数无效 |

## 许可证

SPDX-License-Identifier: GPL-2.0-or-later
