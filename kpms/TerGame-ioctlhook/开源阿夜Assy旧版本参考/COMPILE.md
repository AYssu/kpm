# FastScan 编译指南

## ✅ 编译成功！

已使用 **OLLVM (Obfuscator-LLVM)** 成功编译，启用了代码混淆。

---

## 🔧 编译命令

### 方法一：使用 make 直接编译（推荐）

```bash
cd /root/kpm/kpms/fscan-syscallhook

make USE_CLANG=1 \
     CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
     CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
```

### 方法二：使用编译脚本

```bash
cd /root/kpm/kpms/fscan-syscallhook
bash build.sh
```

---

## 📊 编译输出说明

成功编译时会看到以下信息：

```
/root/android-ndk-r27c/.../clang --target=aarch64-elf ...
[OLLVM] run.PipelineStartEPCallback    ← OLLVM 混淆已启用
/root/android-ndk-r27c/.../ld.lld -r -o fscan.kpm fscan.o
Stripping symbols for obfuscation...   ← 符号剥离以增强隐藏性
```

生成文件：`fscan.kpm`

---

## 🛡️ OLLVM 混淆选项

当前启用的混淆：

| 选项 | 说明 | 效果 |
|------|------|------|
| `-mllvm -sub` | 指令替换 | 将简单指令替换为复杂等价指令 |
| `-mllvm -split` | 基本块分割 | 将代码块分割，增加分析难度 |
| `-fvisibility=hidden` | 符号隐藏 | 隐藏符号表 |
| `--strip-unneeded` | 符号剥离 | 移除不必要的符号信息 |

**未启用的选项：**
- `-mllvm -fla`（控制流平坦化）- 会大幅增加体积
- `-mllvm -bcf`（虚假控制流）- 会大幅增加体积
- `-mllvm -sobf`（字符串混淆）- 会破坏 `kallsyms_lookup_name` 查找

---

## 🐛 常见编译错误

### 错误 1: `TARGET_COMPILE not set`

```bash
Makefile:7: *** TARGET_COMPILE not set.  Stop.
```

**原因：** 未设置编译器

**解决：** 使用 `USE_CLANG=1` 参数

### 错误 2: `clang: not found`

```bash
make: clang: No such file or directory
```

**原因：** NDK 路径不正确

**解决：** 检查 NDK 安装路径，修改 `CLANG_PATH`

### 错误 3: `incomplete definition of type 'struct task_struct'`

```c
error: incomplete definition of type 'struct task_struct'
  caller_uid = current->cred->uid.val;
```

**原因：** 直接访问结构体成员（已修复）

**解决：** 使用 KPM 偏移量方式（代码已更新）

---

## 📁 编译产物

| 文件 | 说明 | 用途 |
|------|------|------|
| `fscan.kpm` | 内核模块 | 使用 `kpm load` 加载 |
| `fscan.o` | 目标文件 | 中间文件，可删除 |
| `test_uid` | UID 测试工具 | 查看当前进程 UID |
| `show_ioctl_code` | 命令码演示 | 查看 ioctl 命令码 |
| `example_usage` | 使用示例 | 测试内核模块 |

---

## 🚀 加载模块

编译成功后，加载模块：

```bash
# 加载模块
kpm load /root/kpm/kpms/fscan-syscallhook/fscan.kpm

# 检查是否加载成功
kpm list

# 查看内核日志（如果启用了日志）
dmesg | grep FastScan
```

---

## 🔄 重新编译

修改代码后重新编译：

```bash
# 清理旧文件
make clean

# 重新编译
make USE_CLANG=1 \
     CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
     CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"

# 卸载旧模块
kpm unload fscan-syscallhook

# 加载新模块
kpm load fscan-syscallhook.kpm
```

---

## 💡 性能优化

### 当前优化等级

```makefile
CFLAGS += -O2  # 优化等级 2
```

### 可选优化等级

- `-O0` - 无优化（调试用）
- `-O1` - 基本优化
- `-O2` - 推荐优化（当前）
- `-O3` - 激进优化（可能不稳定）
- `-Os` - 大小优化
- `-Oz` - 最小大小优化

修改 `Makefile` 第60行调整优化等级。

---

## 🔐 安全性检查

编译后检查混淆效果：

```bash
# 查看符号表（应该很少）
readelf -s fscan.kpm

# 查看反汇编（应该很混乱）
objdump -d fscan.kpm | head -100

# 检查字符串（不应该有敏感信息）
strings fscan.kpm | grep -i "fastscan\|uid\|mem"
```

---

## 📝 修改配置后的编译流程

### 1. 修改 UID 白名单

```c
// 编辑 fscan.c，修改第 309 行
#define ALLOWED_UID_1  0      // 改为你的 UID
```

### 2. 修改命令码（可选）

```c
// 编辑 fscan_ioctl.h，修改第 58-59 行
#define FSCAN_IOC_READ_MEM    _IOWR('v', 0x9F, struct mem_operation)
//                                    ↑    ↑
//                              设备类型   命令号
```

### 3. 启用/禁用日志

```c
// 编辑 fscan.c，修改第 32 行
#define ENABLE_DEBUG_LOG 0  // 0=禁用, 1=启用
```

### 4. 调整混淆强度

```makefile
# 编辑 Makefile，修改第 34 行
CLANG_OBFUSCATE_FLAGS = -mllvm -sub -mllvm -split
# 可添加：
# -mllvm -fla  # 控制流平坦化（体积+200%~500%）
# -mllvm -bcf  # 虚假控制流（体积+100%~300%）
```

修改后记得重新编译！

---

## ✅ 编译完成检查清单

- [ ] `fscan.kpm` 文件已生成
- [ ] 文件大小合理（约 10-50 KB）
- [ ] 没有编译警告或错误
- [ ] OLLVM 混淆信息出现在输出中
- [ ] 符号已剥离（`llvm-strip` 成功执行）
- [ ] 配置了正确的 UID
- [ ] 日志已禁用（生产环境）

---

## 🎯 下一步

编译成功后：

1. **测试 UID**：`./test_uid`
2. **加载模块**：`kpm load fscan-syscallhook.kpm`
3. **测试功能**：`./example_usage $$ 0x400000`
4. **检查日志**：`dmesg | tail -20`（如果启用了日志）

---

## 📞 故障排除

如果遇到问题：

1. 检查 NDK 路径是否正确
2. 确认 NDK 版本（r27c）
3. 查看完整的编译输出
4. 检查 KPM 框架版本兼容性
5. 尝试清理后重新编译：`make clean && make ...`

---

祝编译顺利！🎉

