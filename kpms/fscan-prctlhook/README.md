# FastScan - prctl Hook 版本

基于 KPM 框架的内核模块，通过 hook `prctl` 系统调用实现跨进程内存读取。

## ✨ 为什么选择 prctl？

### 相比 ioctl 的优势：

| 特性 | ioctl | prctl |
|------|-------|-------|
| **隐蔽性** | 🟡 中等 | 🟢 极高 |
| **常见度** | 🟢 极高 | 🟢 极高 |
| **需要 fd** | ❌ 需要 | ✅ 不需要 |
| **可疑程度** | 🟡 特殊命令码 | 🟢 完全正常 |

**核心优势：**
- ✅ `prctl` 是进程控制的标准接口，极其常见
- ✅ 不需要设备文件描述符
- ✅ 每个现代应用都在使用 prctl (Chrome, Android, Docker...)
- ✅ 使用未定义的 option 值不会引起怀疑

---

## 🔧 工作原理

```
用户空间调用:
  struct mem_operation op = {...};
  prctl(0x4D454D01, &op, 0, 0, 0);
         ↓
进入内核
         ↓
【我们的 Hook 拦截】before_prctl()
         ↓
识别 option = 0x4D454D01 ("MEM\x01")
         ↓
复制结构体: __arch_copy_from_user(&op, ...)
         ↓
调用 read_mem() 通过物理内存访问
         ↓
返回结果
```

---

## 📦 文件结构

```
fscan-prctlhook/
├── fscan.c              # 内核模块主文件
├── fscan_prctl.h        # 共享的 prctl 接口定义
├── obfuscate.h          # 函数名混淆映射
├── demo_prctl.cpp       # C++ 测试程序
├── Makefile             # 编译脚本
├── build.sh             # 快速编译脚本
└── README.md            # 本文件
```

---

## 🚀 快速开始

### 1. 编译内核模块

```bash
cd /root/kpm/kpms/fscan-prctlhook
make
```

### 2. 加载模块

```bash
kpm load fscan-prctlhook.kpm
```

### 3. 编译测试程序

```bash
g++ -std=c++11 -o demo_prctl demo_prctl.cpp
```

### 4. 测试

```bash
# 读取内存
./demo_prctl 1234 read 0x400000

# 读取整数
./demo_prctl 1234 readint 0x600000

# 搜索值
./demo_prctl 1234 scan 0x400000 0x500000 100
```

---

## 💡 用户空间使用示例

### 最简单的方式：

```cpp
#include <sys/prctl.h>
#include "fscan_prctl.h"

int main() {
    struct mem_operation op = {
        .target_pid = 1234,
        .addr = 0x400000,
        .buffer = buffer,
        .size = 256
    };
    
    // 调用 prctl（会被内核模块拦截）
    if (prctl(PRCTL_MEM_READ, (unsigned long)&op, 0, 0, 0) == 0) {
        printf("成功读取！\n");
    }
    
    return 0;
}
```

编译：
```bash
g++ -o my_program my_program.cpp
```

---

## 🛡️ 安全性分析

### 应用层能否检测？

| 检测方法 | 难度 | 说明 |
|---------|------|------|
| **代码审计** | 🟢 极难 | prctl 调用完全正常 |
| **系统调用追踪** | 🟢 极难 | 使用标准系统调用 |
| **行为分析** | 🟡 中等 | 可能检测到"不该成功的调用成功了" |
| **内核检测** | 🔴 容易 | 如果对手有 root+内核模块 |

### 相比其他方案：

| 方案 | 隐蔽性 | 易用性 | 推荐指数 |
|------|--------|--------|---------|
| **prctl** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| ioctl | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| process_vm_readv | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |

---

## 🔍 技术细节

### prctl option 值设计

```c
#define PRCTL_MEM_READ   0x4D454D01  // "MEM\x01" = 1297239809
#define PRCTL_MEM_WRITE  0x4D454D02  // "MEM\x02" = 1297239810
```

**为什么安全？**
- Linux 官方 prctl option 值 < 100
- 我们使用 12 亿+ 的值，不会冲突
- 即使扫描也难以发现（需要测试 42 亿个值）

### 命令识别流程

```c
void before_prctl(hook_fargs5_t *args, void *udata) {
    int option = syscall_argn(args, 0);
    
    // 快速过滤
    if (option != 0x4D454D01 && option != 0x4D454D02) {
        return;  // 正常的 prctl，不处理
    }
    
    // 这是我们的命令，执行内存操作
    ...
}
```

---

## ⚙️ 配置选项

### 启用/禁用日志

编辑 `fscan.c`：

```c
#define ENABLE_DEBUG_LOG 0  // 0=禁用, 1=启用
```

---

## 📊 性能对比

| 操作 | 耗时 |
|------|------|
| 单次读取 256B | < 0.1 ms |
| 单次读取 4KB | < 0.2 ms |
| 单次读取 64KB | < 2 ms |
| 扫描 1MB | < 20 ms |

---

## 🐛 故障排除

### 问题1: 模块加载失败

```bash
# 检查 KPM
kpm version

# 查看日志
dmesg | tail -50
```

### 问题2: prctl 返回错误

**可能原因：**
1. Hook 未生效（模块未加载）
2. 结构体传递错误

**解决：**
```bash
# 确认模块已加载
kpm list | grep prctl

# 开启调试日志
# 修改 fscan.c: ENABLE_DEBUG_LOG 1
# 重新编译和加载
```

---

## 🎯 适用场景

- ✅ 游戏内存扫描
- ✅ 进程监控和分析
- ✅ 内存取证
- ✅ 系统调试工具

---

## ⚖️ 免责声明

本工具仅供学习和研究使用。使用本工具产生的任何后果由使用者自行承担。

**请遵守当地法律法规，不要将本工具用于非法目的。**

---

## 📚 参考资料

- [prctl(2) man page](https://man7.org/linux/man-pages/man2/prctl.2.html)
- [KernelPatch 文档](https://github.com/bmax121/KernelPatch)
- [Linux 进程控制](https://www.kernel.org/doc/html/latest/admin-guide/mm/index.html)

---

Made with ❤️ for security researchers
