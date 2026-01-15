# FastScan KPM 驱动用户态接口

这是一个完整的用户态 C++ 封装库，用于与 FastScan KPM 内核模块通信。

> 📖 **编译 KPM 模块**: 请参考 [编译指南](../../BUILD.md) 了解如何编译内核模块

## 功能特性

- ✅ **模块可用性检查** - 验证 KPM 模块是否已加载
- ✅ **进程查找** - 根据进程名获取 PID
- ✅ **内存读取** - 读取目标进程的虚拟内存
- ✅ **内存写入** - 写入目标进程的虚拟内存
- ✅ **模块基址查询** - 获取目标进程中指定模块的基址
- ✅ **类型安全** - 提供模板函数，自动处理类型转换
- ✅ **错误处理** - 完善的错误信息反馈

## 文件结构

```
ByTearGameuserCode/jni/
├── kpmdriverTearGame.h      # C++ 头文件（接口定义）
├── KPmTearGame.cpp          # C++ 实现文件（包含示例代码）
├── Android.mk               # Android NDK 构建文件
└── Application.mk          # NDK 应用配置
```

## 快速开始

### 1. 基本使用

```cpp
#include "kpmdriverTearGame.h"

// 创建驱动实例
KPMDriver::FastScanDriver driver;

// 检查模块是否可用
if (!driver.checkModuleAvailable()) {
    std::cerr << "错误: " << driver.getLastError() << std::endl;
    return -1;
}

// 获取进程 PID
pid_t pid = driver.getProcessPid("com.example.app");
if (pid <= 0) {
    std::cerr << "错误: " << driver.getLastError() << std::endl;
    return -1;
}

// 读取内存
int32_t value = 0;
if (driver.readMemory(pid, 0x70000000, value) == 0) {
    std::cout << "读取成功: " << value << std::endl;
}

// 写入内存
int32_t new_value = 0x12345678;
if (driver.writeMemory(pid, 0x70000000, new_value) == 0) {
    std::cout << "写入成功" << std::endl;
}
```

### 2. 获取模块基址

```cpp
KPMDriver::FastScanDriver driver;

pid_t pid = driver.getProcessPid("com.example.app");
uint64_t base = driver.getModuleBase(pid, "libtarget.so");

if (base > 0) {
    std::cout << "模块基址: 0x" << std::hex << base << std::dec << std::endl;
    
    // 读取模块中的偏移地址
    uint64_t offset = base + 0x1000;
    int32_t value = 0;
    driver.readMemory(pid, offset, value);
}
```

### 3. 批量读取内存

```cpp
KPMDriver::FastScanDriver driver;
pid_t pid = driver.getProcessPid("com.example.app");

// 读取大块内存
std::vector<uint8_t> buffer(1024);
if (driver.readMemory(pid, 0x70000000, buffer.data(), buffer.size()) == 0) {
    // 处理数据...
}
```

## API 参考

### FastScanDriver 类

#### 构造函数
```cpp
FastScanDriver();
```
创建驱动实例，自动检查 root 权限和模块可用性。

#### 检查模块可用性
```cpp
bool checkModuleAvailable();
```
返回 `true` 如果模块可用，`false` 否则。

#### 获取进程 PID
```cpp
pid_t getProcessPid(const std::string& processName);
```
根据进程名（支持部分匹配）查找进程 PID。成功返回 PID，失败返回 -1。

#### 读取内存
```cpp
// 通用版本
int readMemory(pid_t pid, uint64_t address, void* buffer, size_t size);

// 模板版本（自动推断类型）
template<typename T>
int readMemory(pid_t pid, uint64_t address, T& value);
```

#### 写入内存
```cpp
// 通用版本
int writeMemory(pid_t pid, uint64_t address, const void* buffer, size_t size);

// 模板版本（自动推断类型）
template<typename T>
int writeMemory(pid_t pid, uint64_t address, const T& value);
```

#### 获取模块基址
```cpp
uint64_t getModuleBase(pid_t pid, const std::string& moduleName);
```
返回模块基址，失败返回 0。

#### 错误处理
```cpp
const std::string& getLastError() const;
```
获取最后一次操作的错误信息。

#### 权限检查
```cpp
static bool isRoot();
```
检查当前进程是否以 root 权限运行。

## 编译说明

### Android NDK 编译

1. 确保已安装 Android NDK
2. 在 `jni` 目录下执行：
```bash
ndk-build
```

### 单独编译示例程序

在 `KPmTearGame.cpp` 中定义 `BUILD_EXAMPLE_PROGRAM` 宏：

```cpp
#define BUILD_EXAMPLE_PROGRAM
```

然后编译：
```bash
g++ -std=c++11 -DBUILD_EXAMPLE_PROGRAM KPmTearGame.cpp -o kpm_example
```

## 注意事项

1. **Root 权限**：所有操作都需要 root 权限
2. **模块加载**：使用前确保已加载 `fscan-prctl.kpm` 内核模块
3. **进程查找**：进程名支持部分匹配（使用 `strstr`）
4. **内存对齐**：内核模块会自动处理页对齐，但建议使用页对齐的地址以提高性能
5. **错误处理**：每次操作后检查返回值，失败时调用 `getLastError()` 获取详细信息

## 错误码说明

- **EPERM** (1): 权限不足，需要 root 权限
- **EFAULT** (14): 内存访问错误，通常是地址无效
- **EIO** (5): I/O 错误，读取/写入失败
- **ESRCH** (3): 进程不存在
- **ENOENT** (2): 模块不存在
- **EINVAL** (22): 版本不匹配或其他参数错误
- **ENOSPC** (28): 缓冲区太小

## 示例程序

代码中包含多个示例函数：

- `example_check_module()` - 检查模块可用性
- `example_get_pid()` - 获取进程 PID
- `example_read_memory()` - 读取内存示例
- `example_write_memory()` - 写入内存示例
- `example_get_module_base()` - 获取模块基址示例
- `example_complete_usage()` - 完整使用流程示例

运行示例程序：
```bash
# 需要 root 权限
sudo ./kpm_example [PID] [ADDRESS]
```

## 许可证

SPDX-License-Identifier: GPL-2.0-or-later
