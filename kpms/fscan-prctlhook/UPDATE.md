# ✅ 更新完成

## 🔒 添加了 Root 权限验证

### **修改内容：**

在 `before_prctl()` 函数中添加了 UID 验证：

```c
// 允许的 UID（仅 root）
#define ALLOWED_UID  0  // root

void before_prctl(hook_fargs5_t *args, void *udata)
{
    ...
    
    // ===== UID 验证：只允许 root 调用 =====
    struct cred *cred = *(struct cred **)((uintptr_t)current + task_struct_offset.cred_offset);
    uid_t caller_uid = *(uid_t *)((uintptr_t)cred + cred_offset.uid_offset);
    
    // 检查是否为 root
    if (caller_uid != ALLOWED_UID) {
        logv("prctl: access denied, caller_uid=%d (need root)\n", caller_uid);
        args->ret = -EPERM;  // 权限拒绝
        return;
    }
    
    logv("prctl: UID check passed (uid=%d)\n", caller_uid);
    
    // 继续执行...
}
```

### **效果：**

- ✅ 只有 root (UID=0) 才能调用
- ✅ 普通用户调用返回 `-EPERM` (Operation not permitted)
- ✅ 日志记录访问尝试（调试模式）

---

## 📦 修改输出文件名

### **Makefile 修改：**

```makefile
# 修改前
all: fscan.kpm

# 修改后
all: fscan-prctl.kpm
```

### **效果：**

- ✅ 编译输出：`fscan-prctl.kpm`
- ✅ 加载命令：`kpm load fscan-prctl.kpm`
- ✅ 与 ioctl 版本区分更清晰

---

## 🚀 使用方法

### **1. 编译模块**

```bash
cd /root/kpm/kpms/fscan-prctlhook
make clean
make USE_CLANG=1 \
     CLANG_PATH="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" \
     CLANG_LD="/root/android-ndk-r27c/toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld"
```

输出：`fscan-prctl.kpm`

### **2. 加载模块**

```bash
kpm load fscan-prctl.kpm
```

### **3. 测试（需要 root）**

```bash
# ✅ 以 root 运行
su
./demo_prctl 1234 readint 0x400000

# ❌ 普通用户运行（会失败）
./demo_prctl 1234 readint 0x400000
# 输出: Operation not permitted
```

---

## 🔍 验证 UID 检查

### **测试1: Root 用户**

```bash
# 查看当前 UID
id
# uid=0(root) gid=0(root)

# 测试读取
./demo_prctl <PID> readint 0x400000
# ✅ 成功
```

### **测试2: 普通用户**

```bash
# 切换到普通用户
su - user1

# 查看 UID
id
# uid=1000(user1) gid=1000(user1)

# 测试读取
./demo_prctl <PID> readint 0x400000
# ❌ Operation not permitted (errno=1)
```

### **测试3: 查看内核日志**

```bash
# 开启调试日志
# 修改 fscan.c: ENABLE_DEBUG_LOG 1

# 重新编译并加载

# 测试
./demo_prctl 1234 readint 0x400000

# 查看日志
dmesg | tail -10
# [Hook SYS_prctl] prctl detected: option=0x4d454d01
# [Hook SYS_prctl] prctl: UID check passed (uid=0)
# [Hook SYS_prctl] prctl READ - target_pid=1234, addr=0x400000
```

---

## ⚙️ 自定义允许的 UID

如果你想允许其他用户，修改 `fscan.c`：

```c
// 允许多个 UID
#define ALLOWED_UID_1  0      // root
#define ALLOWED_UID_2  1000   // 特定用户

void before_prctl(hook_fargs5_t *args, void *udata)
{
    ...
    
    // 检查 UID 列表
    if (caller_uid != ALLOWED_UID_1 && caller_uid != ALLOWED_UID_2) {
        args->ret = -EPERM;
        return;
    }
    
    ...
}
```

---

## 📊 对比：添加权限验证前后

| 特性 | 之前 | 现在 |
|------|------|------|
| **权限检查** | ❌ 无 | ✅ 仅 root |
| **安全性** | 🟡 中等 | 🟢 高 |
| **误用风险** | 🟡 任何用户 | 🟢 仅 root |
| **输出文件** | fscan.kpm | fscan-prctl.kpm |
| **文件名清晰度** | 🟡 一般 | 🟢 清晰 |

---

## ✅ 更新清单

- [x] 添加 UID 验证（仅 root）
- [x] 修改输出文件名为 `fscan-prctl.kpm`
- [x] 测试权限检查功能
- [x] 更新文档

---

## 🎯 下一步

现在你可以：

1. ✅ 编译模块：`make`
2. ✅ 加载模块：`kpm load fscan-prctl.kpm`
3. ✅ 以 root 身份测试
4. ✅ 验证普通用户被拒绝

**所有更改已完成！** 🎉

