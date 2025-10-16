# get_module_base 函数适配说明

## ✅ 已完成的适配

### 1. 添加的内核函数声明

```c
// find_vma - 查找虚拟内存区域
struct vm_area_struct * find_vma(struct mm_struct * mm, unsigned long addr);

// d_path - 获取文件路径
char* d_path(const struct path* path, char* buf, int buflen);
```

### 2. 添加的结构体定义

```c
// 文件操作结构体
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
};

// 路径结构体
struct path {
    struct vfsmount* mnt;
    struct dentry* dentry;
};

// 文件结构体
struct file {
    union {
        struct llist_node    fu_llist;
        struct rcu_head      fu_rcuhead;
    } f_u;
    struct path     f_path;
    struct inode*   f_inode;
    const struct file_operations *f_op;
};
```

### 3. 辅助函数

```c
// 获取路径的基本文件名
static inline const char *kbasename(const char *path);
```

### 4. 核心函数

```c
// 获取进程中指定模块的基址
static uintptr_t get_module_base(pid_t pid, const char* module_name);
```

## 🔧 函数功能说明

### get_module_base

**功能**: 获取指定进程中某个模块（so库、可执行文件）的加载基址

**参数**:
- `pid`: 目标进程的 PID
- `module_name`: 模块名称（可以是部分名称）

**返回值**:
- 成功：返回模块的基址（> 0）
- 失败：返回 0

**工作原理**:
1. 通过 PID 找到进程的 task_struct
2. 获取进程的内存描述符 mm_struct
3. 遍历进程的虚拟内存区域（VMA）
4. 动态探测 vm_file 字段的偏移量
5. 对每个 VMA，检查关联的文件路径
6. 匹配目标模块名，返回 VMA 的起始地址（vm_start）

**特点**:
- ✅ **动态偏移探测**: 自动探测 vm_file_offset，兼容不同内核版本
- ✅ **部分匹配**: 支持模糊匹配模块名（使用 `strstr`）
- ✅ **首次匹配**: 返回第一个匹配的模块基址
- ✅ **详细日志**: 可通过 `ENABLE_DEBUG_LOG` 启用调试信息

## 📝 使用示例（内核代码）

```c
// 示例1: 查找 libc.so
uintptr_t libc_base = get_module_base(1234, "libc.so");
if (libc_base > 0) {
    logv("libc.so base address: 0x%lx\n", libc_base);
}

// 示例2: 查找主程序
uintptr_t app_base = get_module_base(1234, "app_process64");
if (app_base > 0) {
    logv("app_process64 base address: 0x%lx\n", app_base);
}

// 示例3: 查找 Android 系统库
uintptr_t art_base = get_module_base(1234, "libart.so");
```

## 🎯 典型应用场景

### 1. 查找共享库基址

```c
// 在 Android 中查找 libc
uintptr_t libc = get_module_base(target_pid, "libc.so");

// 查找 ART 虚拟机
uintptr_t art = get_module_base(target_pid, "libart.so");
```

### 2. 查找应用主模块

```c
// 查找应用的主可执行文件
uintptr_t app_base = get_module_base(target_pid, "com.example.app");
```

### 3. 配合地址计算

```c
// 1. 获取模块基址
uintptr_t module_base = get_module_base(pid, "libtarget.so");

// 2. 计算函数地址（基址 + 偏移）
uintptr_t target_func = module_base + 0x12345;

// 3. 读取函数代码
unsigned char code[16];
read_mem(pid, target_func, code, sizeof(code));
```

## 🔍 工作流程详解

```
1. find_task_by_vpid(pid)
   ↓
2. get_task_mm(task)
   ↓
3. find_vma(mm, 0)  // 获取第一个VMA
   ↓
4. 动态探测 vm_file_offset（仅首次）
   - 遍历偏移量 0x0 - 0xa00
   - 尝试读取文件路径
   - 找到有效的 vm_file 字段
   ↓
5. 遍历所有 VMA
   - 读取 VMA 关联的文件
   - 调用 d_path 获取路径
   - 使用 kbasename 提取文件名
   - 使用 strstr 匹配模块名
   ↓
6. 返回匹配的 VMA 起始地址
```

## ⚠️ 注意事项

### 1. vm_file_offset 探测

`vm_file_offset` 是 `vm_area_struct` 结构中 `vm_file` 字段的偏移量。由于不同内核版本结构体布局可能不同，代码会动态探测这个偏移：

- 首次调用时会进行探测（遍历可能的偏移）
- 探测成功后会缓存偏移量
- 后续调用直接使用缓存的偏移

### 2. 模块名匹配

- 使用 `strstr` 进行部分匹配
- 例如搜索 "libc" 会匹配 "libc.so"
- 可能匹配到同名的不同版本（如 libc.so vs libc-2.31.so）
- 返回第一个匹配的模块

### 3. 性能考虑

- 首次调用会有探测开销
- 遍历所有 VMA 可能较慢（取决于进程的内存映射数量）
- 建议缓存结果，避免重复查询

### 4. 错误处理

函数返回 0 的情况：
- 进程不存在（pid 无效）
- 无法获取 mm_struct
- vm_file_offset 探测失败
- 模块未找到

## 🐛 调试

### 启用调试日志

在 `fscan.c` 开头设置：

```c
#define ENABLE_DEBUG_LOG 1
```

### 查看日志

```bash
# Android
adb shell su -c dmesg | grep FastScan

# Linux
dmesg | grep FastScan
```

### 日志示例

```
[FastScan] get_module_base: pid=1234, module_name='libc.so'
[FastScan] get_module_base: found task: ffff8881a2b4c800
[FastScan] get_module_base: got mm: ffff8881a3d5f000
[FastScan] get_module_base: probing vm_file_offset...
[FastScan] get_module_base: found vm_file_offset=0x40
[FastScan] get_module_base: found module 'libc.so' at 0x7f1234567000 (path: /system/lib64/libc.so)
[FastScan] get_module_base: success, base=0x7f1234567000
```

## 📊 常见模块名

### Android 系统

```c
// C 库
get_module_base(pid, "libc.so");

// C++ 库
get_module_base(pid, "libc++.so");

// ART 虚拟机
get_module_base(pid, "libart.so");

// OpenGL ES
get_module_base(pid, "libGLESv2.so");

// Android 运行时
get_module_base(pid, "libandroid_runtime.so");
```

### 应用模块

```c
// Unity 引擎
get_module_base(pid, "libunity.so");

// Unreal 引擎
get_module_base(pid, "libUE4.so");

// 应用主程序
get_module_base(pid, "libapp.so");
```

## 🔄 与其他功能配合使用

### 完整示例：定位并读取函数

```c
// 1. 获取进程PID（使用 get_process_pid）
pid_t pid = get_process_pid("com.example.app");

// 2. 获取模块基址
uintptr_t module_base = get_module_base(pid, "libtarget.so");

// 3. 计算目标地址
uintptr_t target_addr = module_base + 0x12345;  // 偏移从IDA/Ghidra获取

// 4. 读取内存
unsigned char buffer[256];
read_mem(pid, target_addr, buffer, sizeof(buffer));

// 5. 分析数据
// ...
```

## ❓ 常见问题

### Q: 为什么找不到模块？

**A:** 可能原因：
1. 模块名拼写错误
2. 模块未加载到进程中
3. 进程权限不足
4. vm_file_offset 探测失败

**解决方法**：
```bash
# 查看进程的内存映射
adb shell su -c cat /proc/<pid>/maps | grep <module_name>
```

### Q: vm_file_offset 探测失败怎么办？

**A:** 
- 确保进程有合法的内存映射
- 尝试用一个常见的模块名进行首次探测（如 "libc"）
- 检查内核版本是否支持 VMA 结构

### Q: 如何提高查找速度？

**A:**
- 缓存 `get_module_base` 的结果
- 使用更精确的模块名（减少误匹配）
- 首次调用后 vm_file_offset 已缓存，后续会更快

## 📈 下一步

现在 `get_module_base` 函数已经适配完成，你可以：

1. ✅ 测试函数是否正常工作
2. ⏳ 添加 prctl 接口对接（需要添加结构体和命令）
3. ⏳ 创建用户空间示例程序

如果测试正常，我可以继续帮你完成 prctl 接口对接部分。

