# FastScan - 内核内存读取模块

基于 KPM 框架的内核模块，通过 hook `ioctl` 系统调用实现跨进程内存读取。

[![License](https://img.shields.io/badge/license-GPL%20v2-blue.svg)](LICENSE)
[![KPM](https://img.shields.io/badge/KPM-Framework-green.svg)](https://github.com/bmax121/KernelPatch)
[![OLLVM](https://img.shields.io/badge/OLLVM-Obfuscated-red.svg)](https://github.com/obfuscator-llvm/obfuscator)

---

## ✨ 特性

- ✅ **UID 白名单验证** - 只允许特定用户调用
- ✅ **时间戳验证** - 3秒时间窗口，防止重放攻击
- ✅ **伪装命令码** - 使用 `_IOWR` 宏伪装成视频设备命令
- ✅ **OLLVM 混淆** - 代码混淆，增加逆向难度
- ✅ **直接物理内存访问** - 绕过常规权限检查
- ✅ **自动分页读取** - 处理跨页内存访问
- ✅ **符号混淆** - 函数名混淆 + 符号剥离
- ✅ **零日志模式** - 生产环境无日志输出

---

## 🔐 安全性

### 三重验证机制

```
用户进程调用
    ↓
【验证1】UID 白名单
    ↓
【验证2】时间戳（3秒窗口）
    ↓
【验证3】命令码（0xC0187699）
    ↓
执行内存读取
```

### 隐藏性分析

| 检测方法 | 检测难度 | 说明 |
|---------|---------|------|
| 命令码扫描 | 🔴 极难 | 需要扫描 42 亿个可能值 |
| 符号分析 | 🔴 极难 | 符号已混淆+剥离 |
| 代码逆向 | 🟠 困难 | OLLVM 混淆增加分析难度 |
| 行为检测 | 🟡 中等 | hook ioctl 可能被检测 |
| 权限验证 | 🟢 容易 | 需要特定 UID |

---

## 📦 文件结构

```
fscan-syscallhook/
├── fscan.c              # 内核模块主文件
├── fscan_ioctl.h        # 共享的 ioctl 接口定义
├── obfuscate.h          # 函数名混淆映射
├── Makefile             # 编译脚本（支持 OLLVM）
├── build.sh             # 快速编译脚本
├── build_examples.sh    # 编译测试工具脚本
│
├── test_uid.c           # UID 查看工具
├── show_ioctl_code.c    # 命令码演示程序
├── example_usage.c      # 完整的使用示例
│
├── README.md            # 本文件
├── QUICK_START.md       # 快速开始指南 ⚡
├── USAGE_GUIDE.md       # 详细使用指南（17页）
├── UID_CONFIG.md        # UID 配置专题
└── COMPILE.md           # 编译指南
```

---

## 🚀 快速开始（5分钟）

### 1. 编译测试工具

```bash
cd /root/kpm/kpms/fscan-syscallhook
bash build_examples.sh
```

### 2. 查看你的 UID

```bash
./test_uid
```

输出示例：
```
===== 当前进程的 UID 信息 =====
Real UID:      0
Effective UID: 0  ← 记住这个数字
```

### 3. 配置内核模块

编辑 `fscan.c`，修改第 309 行：

```c
#define ALLOWED_UID_1  0  // 改为你的 UID
```

### 4. 编译内核模块

```bash
make USE_CLANG=1 \
     CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
     CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
```

### 5. 加载模块

```bash
kpm load fscan-syscallhook.kpm
```

### 6. 测试

```bash
./example_usage <目标PID> <目标地址>
```

---

## 📖 用户层使用

### 最简单的方式

```c
#include "fscan_ioctl.h"

int main() {
    struct mem_operation mem_op;
    unsigned char buffer[256];
    
    // 设置参数
    mem_op.target_pid = 12345;
    mem_op.addr = 0x7fff12340000;
    mem_op.buffer = buffer;
    mem_op.size = sizeof(buffer);
    
    // 调用（fd 是时间戳）
    if (ioctl((int)time(NULL), FSCAN_IOC_READ_MEM, &mem_op) == 0) {
        printf("成功读取 %lu 字节\n", mem_op.size);
    }
    
    return 0;
}
```

**编译：** `gcc -o my_program my_program.c`

---

## 🛠️ 技术细节

### UID 验证原理

```c
// 使用 KPM 偏移量方式获取 UID
struct cred *cred = *(struct cred **)((uintptr_t)current + task_struct_offset.cred_offset);
uid_t caller_uid = *(uid_t *)((uintptr_t)cred + cred_offset.uid_offset);

// 验证
if (caller_uid != ALLOWED_UID) {
    return -EPERM;  // 拒绝
}
```

**为什么安全：**
- UID 由内核管理，用户层无法伪造
- 使用 KPM 框架的偏移量机制，避免结构体不完整定义的问题

### 命令码生成

```c
#define FSCAN_IOC_READ_MEM  _IOWR('v', 0x9F, struct mem_operation)
```

展开后的 32 位命令码结构：
```
┌──────────┬───────────┬──────────┬──────────┐
│ 方向(2位)│ 大小(14位)│ 类型(8位)│ 序号(8位)│
├──────────┼───────────┼──────────┼──────────┤
│    11    │   24      │ 'v'(0x76)│  0x9F    │
│ RD|WR    │ sizeof()  │ video    │  159     │
└──────────┴───────────┴──────────┴──────────┘
          ↓
     0xC0187699
```

### 物理内存访问流程

```
虚拟地址
    ↓
页表遍历（pgtable_entry）
    ↓
获取 PTE
    ↓
提取物理地址
    ↓
ioremap_cache 映射
    ↓
读取数据
    ↓
copy_to_user
    ↓
__iounmap 解除映射
```

---

## 🎯 适用场景

- ✅ 游戏内存扫描（需要高权限）
- ✅ 进程监控和分析
- ✅ 内存取证
- ✅ 系统调试工具
- ❌ 恶意软件（请勿用于非法目的）

---

## ⚠️ 安全警告

1. **需要 root 权限**：模块加载需要 root
2. **配置 UID**：必须配置正确的 UID 白名单
3. **禁用日志**：生产环境务必禁用日志（`ENABLE_DEBUG_LOG = 0`）
4. **合法使用**：仅用于合法的调试和分析目的
5. **风险自负**：不当使用可能导致系统不稳定

---

## 📚 文档导航

| 文档 | 内容 | 适合 |
|------|------|------|
| [QUICK_START.md](QUICK_START.md) | 快速开始指南 | 新手 ⚡ |
| [USAGE_GUIDE.md](USAGE_GUIDE.md) | 完整使用指南 | 详细了解 |
| [UID_CONFIG.md](UID_CONFIG.md) | UID 配置专题 | 配置帮助 |
| [COMPILE.md](COMPILE.md) | 编译指南 | 编译问题 |

---

## 🔧 系统要求

- **KPM 框架**：已安装并运行
- **编译器**：Clang (NDK r27c 或更高)
- **架构**：ARM64/aarch64
- **内核**：支持 KPM 的 Linux 内核
- **权限**：root

---

## 🐛 故障排除

### 问题 1: 编译失败

```
error: incomplete definition of type 'struct task_struct'
```

**解决：** 已修复，使用 KPM 偏移量方式访问结构体成员

### 问题 2: ioctl 返回 EPERM

**原因：** UID 不在白名单或时间戳过期

**解决：**
1. 运行 `./test_uid` 查看 UID
2. 修改 `fscan.c` 中的 `ALLOWED_UID_1`
3. 重新编译和加载模块

### 问题 3: 模块加载失败

**原因：** 符号未找到或 KPM 版本不兼容

**解决：**
1. 检查 KPM 是否正常运行：`kpm version`
2. 查看内核日志：`dmesg | tail -50`
3. 确认所有内核函数都能找到

---

## 📊 性能指标

- **UID 验证开销**：< 1 微秒
- **时间戳验证开销**：< 1 微秒
- **单次读取延迟**：< 100 微秒（4KB 以内）
- **吞吐量**：约 100 MB/s（连续读取）
- **模块大小**：10-50 KB（取决于混淆强度）

---

## 🔄 版本历史

### v1.0.0 (Current)
- ✅ UID 白名单验证
- ✅ 时间戳验证（3秒窗口）
- ✅ 伪装命令码
- ✅ OLLVM 混淆支持
- ✅ 自动分页读取
- ✅ 符号混淆和剥离
- ✅ 完整文档

### 计划中的功能
- 🔲 动态 UID 管理
- 🔲 加密通信
- 🔲 会话密钥机制
- 🔲 写内存功能
- 🔲 批量读取优化

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 开发指南

1. Fork 本项目
2. 创建特性分支：`git checkout -b feature/AmazingFeature`
3. 提交更改：`git commit -m 'Add some AmazingFeature'`
4. 推送到分支：`git push origin feature/AmazingFeature`
5. 提交 Pull Request

---

## 📄 许可证

GPL v2 - 详见 [LICENSE](LICENSE) 文件

---

## 👨‍💻 作者

- **AYssu** - FastScan 模块开发
- **bmax121** - KPM 框架作者

---

## 🙏 致谢

- [KernelPatch](https://github.com/bmax121/KernelPatch) - KPM 框架
- [OLLVM](https://github.com/obfuscator-llvm/obfuscator) - 代码混淆
- Android NDK - 编译工具链

---

## 📞 联系方式

如有问题或建议，请提交 Issue。

---

## ⚖️ 免责声明

本工具仅供学习和研究使用。使用本工具产生的任何后果由使用者自行承担。

**请遵守当地法律法规，不要将本工具用于非法目的。**

---

<p align="center">
  Made with ❤️ for security researchers and developers
</p>

<p align="center">
  ⭐ 如果这个项目对你有帮助，请给个 Star！
</p>

