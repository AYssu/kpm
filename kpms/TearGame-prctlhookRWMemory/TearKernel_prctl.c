/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 * TearGame KPM - 内存读写 + 短包名PID获取
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * **原作者**: 阿夜 (AYssu)
- GitHub: https://github.com/AYssu
- QQ: 2997036064
- QQ群: 903485530
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 * 
 * 注意：Linux 6.1+ 使用 Maple Tree 管理 VMA，内核层获取模块基址不可行
 *       请使用用户态 getModuleBase() 读取 /proc/<pid>/maps
 * 
 * 本项目仅供学习研究使用，禁止用于任何非法用途
 * 使用者需自行承担使用本项目产生的一切后果
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <syscall.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <kputils.h>
#include <asm/current.h>
#include <ktypes.h>
#include <pgtable.h>
#include <linux/err.h>
#include <linux/pid.h>
#include <linux/sched.h>

KPM_NAME("TearGame");
KPM_VERSION("3.2.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("泪心|阿夜");
KPM_DESCRIPTION("Telegram: t.me/TearGame - 内存读写模块");

// 日志开关：1=开启日志, 0=关闭日志
#define DEBUG_LOG 0

#define TAG "[TearGame] "
#if DEBUG_LOG
    #define logv(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)
#else
    #define logv(fmt, ...) do {} while(0)
#endif

// prctl 命令码
#define PRCTL_MEM_READ      0x4D454D01
#define PRCTL_MEM_WRITE     0x4D454D02
#define PRCTL_GET_PID       0x4D454D03
#define PRCTL_MEM_READ_SAFE  0x4D454D04  // 物理内存读取
#define PRCTL_MEM_WRITE_SAFE 0x4D454D05  // 物理内存写入

// 内存操作结构体
struct mem_operation {
    pid_t target_pid;
    uint64_t addr;
    void __user *buffer;
    uint64_t size;
};

// 获取 PID 请求结构体
#define MAX_PACKAGE_NAME_LEN 64
struct pid_request {
    const char __user *package_name;
    uint32_t name_len;
    pid_t result_pid;
};

// ======================== 内核函数声明 ========================
int __must_check kfunc_def(__arch_copy_from_user)(void *to, const void __user *from, int n);
static inline int __must_check __arch_copy_from_user(void *to, const void __user *from, int n)
{
    kfunc_call(__arch_copy_from_user, to, from, n);
    kfunc_not_found();
    return -1;
}

struct task_struct *kfunc_def(find_task_by_vpid)(pid_t nr);
static inline struct task_struct *my_find_task_by_vpid(pid_t nr)
{
    kfunc_call(find_task_by_vpid, nr);
    kfunc_not_found();
    return NULL;
}

long kfunc_def(access_process_vm)(struct task_struct *tsk, unsigned long addr, 
                                   void *buf, size_t len, int write);
static inline long access_process_vm_safe(struct task_struct *tsk, unsigned long addr, 
                                           void *buf, size_t len, int write)
{
    if (kfunc(access_process_vm)) {
        return kfunc(access_process_vm)(tsk, addr, buf, len, write);
    }
    kfunc_not_found();
    return -1;
}

// ======================== 内核缓冲区 ========================
#define MAX_BUFFER_SIZE 4096
static uint8_t kernel_buffer[MAX_BUFFER_SIZE] __attribute__((aligned(8)));

// ======================== 偏移量全局变量（前置声明） ========================
static unsigned long g_comm_offset = 0;

// ======================== 物理内存读写（使用 memremap） ========================
// 通过 kallsyms 动态解析 memremap/memunmap 实现物理内存访问
// 优点：简单、兼容性高、不依赖页表遍历

// memremap 函数指针
void *kfunc_def(memremap)(uint64_t offset, size_t size, unsigned long flags);
static inline void *my_memremap(uint64_t offset, size_t size, unsigned long flags)
{
    kfunc_call(memremap, offset, size, flags);
    kfunc_not_found();
    return NULL;
}

// memunmap 函数指针
void kfunc_def(memunmap)(void *addr);
static inline void my_memunmap(void *addr)
{
    if (kfunc(memunmap)) {
        kfunc(memunmap)(addr);
    }
}

// memremap flags
#define MEMREMAP_WB     0x1  // Write-back
#define MEMREMAP_WT     0x2  // Write-through
#define MEMREMAP_WC     0x4  // Write-combining

// 物理内存功能是否可用
static int g_phys_mem_available = 0;

// 虚拟地址转物理地址（使用 KPM 框架的 virt_to_phys）
// 注意：这只对内核线性映射区域有效
static uint64_t va_to_pa_simple(uint64_t va)
{
    // 检查是否是用户空间地址
    if (va < 0xFFFF000000000000UL) {
        // 用户空间地址，无法直接转换
        return 0;
    }
    // 内核地址，使用 KPM 框架的 virt_to_phys
    return virt_to_phys(va);
}

// 通过物理地址读取内存
static int read_mem_phys(pid_t pid, uintptr_t vaddr, void __user *buffer, size_t size)
{
    struct task_struct *task;
    void *mapped;
    size_t total_read = 0;
    uintptr_t current_vaddr = vaddr;
    u8 __user *current_buffer = (u8 __user *)buffer;
    size_t remaining = size;
    
    if (!g_phys_mem_available) {
        return -1;
    }
    
    task = my_find_task_by_vpid(pid);
    if (!task) return -1;
    
    // 先用 access_process_vm 读取一次获取数据
    // 然后尝试物理内存方式（如果需要绕过保护）
    while (remaining > 0) {
        size_t pg_offset = current_vaddr & 0xFFF;
        size_t chunk_size = 0x1000 - pg_offset;
        if (chunk_size > remaining) chunk_size = remaining;
        if (chunk_size > MAX_BUFFER_SIZE) chunk_size = MAX_BUFFER_SIZE;
        
        // 使用 access_process_vm 读取（这是最可靠的方式）
        long bytes_read = access_process_vm_safe(task, current_vaddr, kernel_buffer, chunk_size, 0);
        
        if (bytes_read > 0) {
            int copied = compat_copy_to_user(current_buffer, kernel_buffer, bytes_read);
            if (copied != (int)bytes_read) {
                if (total_read == 0) return -1;
                break;
            }
            total_read += bytes_read;
            current_vaddr += bytes_read;
            current_buffer += bytes_read;
            remaining -= bytes_read;
            if (bytes_read < (long)chunk_size) break;
        } else {
            if (total_read == 0) return -1;
            break;
        }
    }
    
    return (total_read > 0) ? 0 : -1;
}

static int write_mem_phys(pid_t pid, uintptr_t vaddr, const void __user *buffer, size_t size)
{
    struct task_struct *task;
    size_t total_written = 0;
    uintptr_t current_vaddr = vaddr;
    const u8 __user *current_buffer = (const u8 __user *)buffer;
    size_t remaining = size;
    
    logv("write_phys: pid=%d addr=0x%lx size=%zu\n", pid, (unsigned long)vaddr, size);
    
    if (!g_phys_mem_available) {
        logv("write_phys: phys_mem not available\n");
        return -1;
    }
    
    task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("write_phys: task not found\n");
        return -1;
    }
    
    while (remaining > 0) {
        size_t pg_offset = current_vaddr & 0xFFF;
        size_t chunk_size = 0x1000 - pg_offset;
        if (chunk_size > remaining) chunk_size = remaining;
        if (chunk_size > MAX_BUFFER_SIZE) chunk_size = MAX_BUFFER_SIZE;
        
        if (__arch_copy_from_user(kernel_buffer, current_buffer, chunk_size) != 0) {
            logv("write_phys: copy_from_user failed\n");
            if (total_written == 0) return -1;
            break;
        }
        
        long bytes_written = access_process_vm_safe(task, current_vaddr, kernel_buffer, chunk_size, 1);
        logv("write_phys: access_process_vm wrote %ld bytes\n", bytes_written);
        
        if (bytes_written > 0) {
            total_written += bytes_written;
            current_vaddr += bytes_written;
            current_buffer += bytes_written;
            remaining -= bytes_written;
            if (bytes_written < (long)chunk_size) break;
        } else {
            logv("write_phys: access_process_vm failed\n");
            if (total_written == 0) return -1;
            break;
        }
    }
    
    logv("write_phys: total_written=%zu\n", total_written);
    return (total_written > 0) ? 0 : -1;
}

/*
// 保留：真正的物理内存直接访问（需要知道物理地址）
// 这个函数可以直接读取物理地址，绕过所有保护
static int read_phys_addr_direct(uint64_t phys_addr, void *buffer, size_t size)
{
    void *mapped;
    
    if (!g_phys_mem_available || !kfunc(memremap)) {
        return -1;
    }
    
    // 映射物理地址到内核虚拟地址
    mapped = my_memremap(phys_addr, size, MEMREMAP_WB);
    if (!mapped) {
        return -1;
    }
    
    // 复制数据
    memcpy(buffer, mapped, size);
    
    // 解除映射
    my_memunmap(mapped);
    
    return 0;
}
*/

// ======================== comm 偏移量探测 ========================
static void probe_comm_offset(void)
{
    struct task_struct *task;
    unsigned long base;
    int offset;
    char *ptr;
    
    task = my_find_task_by_vpid(1);
    if (!task) {
        g_comm_offset = 0x910;
        return;
    }
    
    base = (unsigned long)task;
    
    for (offset = 0x400; offset < 0xC00; offset++) {
        ptr = (char *)(base + offset);
        if (ptr[0] == 'i' && ptr[1] == 'n' && ptr[2] == 'i' && ptr[3] == 't' && ptr[4] == '\0') {
            g_comm_offset = offset;
            logv("probe: comm offset=0x%x\n", offset);
            return;
        }
    }
    
    g_comm_offset = 0x910;
}

// ======================== mm->pgd 偏移量探测（暂时禁用） ========================
/*
static void probe_mm_pgd_offset(void)
{
    struct task_struct *task;
    struct mm_struct *mm;
    unsigned long base;
    int offset;
    uint64_t val;
    
    g_phys_mem_available = 0;  // 默认不可用
    
    // 检查必要的内核函数
    if (!kfunc(get_task_mm) || !kfunc(mmput)) {
        logv("probe_pgd: get_task_mm/mmput not available\n");
        return;
    }
    
    // 检查 linear_voffset 是否有效（KPM 框架变量）
    if (linear_voffset == 0) {
        logv("probe_pgd: linear_voffset=0, phys disabled\n");
        return;
    }
    
    // 使用 init 进程 (pid=1) 探测
    task = my_find_task_by_vpid(1);
    if (!task) {
        logv("probe_pgd: no init task\n");
        return;
    }
    
    mm = my_get_task_mm(task);
    if (!mm) {
        logv("probe_pgd: no mm\n");
        return;
    }
    
    base = (unsigned long)mm;
    
    // 扫描 mm_struct 寻找 pgd 指针
    // pgd 通常在 mm_struct 的前 0x100 字节内
    for (offset = 0x00; offset < 0x100; offset += 8) {
        val = *(uint64_t *)(base + offset);
        
        // pgd 必须是内核地址且页对齐
        if (val >= 0xFFFF000000000000UL && (val & 0xFFF) == 0) {
            // 进一步验证：pgd 指向的内容应该是有效的页表条目
            uint64_t *pgd_ptr = (uint64_t *)val;
            uint64_t first_entry = *pgd_ptr;
            
            // 有效的 PGD 条目通常非零，且低位有标志
            if (first_entry != 0 && (first_entry & 0x3) != 0) {
                g_mm_pgd_offset = offset;
                g_phys_mem_available = 1;  // 启用物理内存功能
                my_mmput(mm);
                logv("probe_pgd: offset=0x%x OK\n", offset);
                return;
            }
        }
    }
    
    my_mmput(mm);
    logv("probe_pgd: not found, phys disabled\n");
}
*/

// ======================== 短包名 PID 获取 ========================
static pid_t get_pid_by_package(const char *name, size_t len)
{
    struct task_struct *task;
    pid_t pid;
    const char *comm;
    size_t cmp_len;
    
    if (g_comm_offset == 0 || len == 0) return -1;
    
    cmp_len = (len > 15) ? 15 : len;
    
    for (pid = 1000; pid <= 32768; pid++) {
        task = my_find_task_by_vpid(pid);
        if (!task) continue;
        
        comm = (const char *)((unsigned long)task + g_comm_offset);
        if (comm[0] == ':') continue;
        
        int match = 1;
        for (size_t i = 0; i < cmp_len; i++) {
            if (comm[i] != name[i]) { match = 0; break; }
        }
        
        if (match) {
            logv("get_pid: found pid=%d\n", pid);
            return pid;
        }
    }
    return -1;
}


// ======================== 核心读取函数 ========================
static int read_mem(pid_t pid, uintptr_t addr, void __user *buffer, size_t size)
{
    struct task_struct *task;
    long bytes_read;
    size_t chunk_size;
    size_t total_read = 0;
    uintptr_t current_addr = addr;
    u8 __user *current_buffer = (u8 __user *)buffer;
    size_t remaining = size;
    
    task = my_find_task_by_vpid(pid);
    if (!task) return -1;
    
    while (remaining > 0) {
        chunk_size = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
        bytes_read = access_process_vm_safe(task, current_addr, kernel_buffer, chunk_size, 0);
        
        if (bytes_read > 0) {
            int copied = compat_copy_to_user(current_buffer, kernel_buffer, bytes_read);
            if (copied != (int)bytes_read) {
                if (total_read > 0) break;
                return -1;
            }
            total_read += bytes_read;
            current_addr += bytes_read;
            current_buffer += bytes_read;
            remaining -= bytes_read;
            if (bytes_read < (long)chunk_size) break;
        } else {
            if (total_read == 0) return -1;
            break;
        }
    }
    return (total_read > 0) ? 0 : -1;
}

// ======================== 核心写入函数 ========================
static int write_mem(pid_t pid, uintptr_t addr, const void __user *buffer, size_t size)
{
    struct task_struct *task;
    long bytes_written;
    size_t chunk_size;
    size_t total_written = 0;
    uintptr_t current_addr = addr;
    const u8 __user *current_buffer = (const u8 __user *)buffer;
    size_t remaining = size;
    
    logv("write_mem: pid=%d addr=0x%lx size=%zu\n", pid, (unsigned long)addr, size);
    
    task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("write_mem: task not found\n");
        return -1;
    }
    
    while (remaining > 0) {
        chunk_size = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
        
        if (__arch_copy_from_user(kernel_buffer, current_buffer, chunk_size) != 0) {
            logv("write_mem: copy_from_user failed\n");
            if (total_written == 0) return -1;
            break;
        }
        
        bytes_written = access_process_vm_safe(task, current_addr, kernel_buffer, chunk_size, 1);
        logv("write_mem: access_process_vm wrote %ld bytes\n", bytes_written);
        
        if (bytes_written > 0) {
            total_written += bytes_written;
            current_addr += bytes_written;
            current_buffer += bytes_written;
            remaining -= bytes_written;
            if (bytes_written < (long)chunk_size) break;
        } else {
            logv("write_mem: access_process_vm failed\n");
            if (total_written == 0) return -1;
            break;
        }
    }
    
    logv("write_mem: total_written=%zu\n", total_written);
    return (total_written > 0) ? 0 : -1;
}

// ======================== prctl Hook ========================
void before_prctl(hook_fargs5_t *args, void *udata)
{
    int option = (int)syscall_argn(args, 0);
    unsigned long arg2 = (unsigned long)syscall_argn(args, 1);
    
    if (option != PRCTL_MEM_READ && option != PRCTL_MEM_WRITE && 
        option != PRCTL_GET_PID &&
        option != PRCTL_MEM_READ_SAFE && option != PRCTL_MEM_WRITE_SAFE) {
        return;
    }
    
    // ==================== 获取 PID ====================
    if (option == PRCTL_GET_PID) {
        struct pid_request req;
        char name[MAX_PACKAGE_NAME_LEN];
        
        if (__arch_copy_from_user(&req, (void __user *)arg2, sizeof(req)) != 0) {
            args->ret = -EFAULT;
            return;
        }
        
        if (req.name_len == 0 || req.name_len >= MAX_PACKAGE_NAME_LEN) {
            args->ret = -EINVAL;
            return;
        }
        
        if (__arch_copy_from_user(name, req.package_name, req.name_len) != 0) {
            args->ret = -EFAULT;
            return;
        }
        name[req.name_len] = '\0';
        
        pid_t found = get_pid_by_package(name, req.name_len);
        
        if (found > 0) {
            req.result_pid = found;
            compat_copy_to_user((void __user *)arg2, &req, sizeof(req));
            args->ret = 0;
        } else {
            args->ret = -ESRCH;
        }
        return;
    }
    
    // ==================== 内存读写 ====================
    struct mem_operation op;
    int result;
    
    if (__arch_copy_from_user(&op, (void __user *)arg2, sizeof(op)) != 0) {
        args->ret = -EFAULT;
        return;
    }
    
    if (!op.buffer) {
        args->ret = -EFAULT;
        return;
    }
    
    // 根据命令选择读写方式
    switch (option) {
        case PRCTL_MEM_READ:
            result = read_mem(op.target_pid, op.addr, op.buffer, op.size);
            break;
        case PRCTL_MEM_WRITE:
            result = write_mem(op.target_pid, op.addr, op.buffer, op.size);
            break;
        case PRCTL_MEM_READ_SAFE:
            result = read_mem_phys(op.target_pid, op.addr, op.buffer, op.size);
            break;
        case PRCTL_MEM_WRITE_SAFE:
            result = write_mem_phys(op.target_pid, op.addr, op.buffer, op.size);
            break;
        default:
            result = -1;
    }
    
    args->ret = (result == 0) ? 0 : -EIO;
}

// ======================== 模块初始化/退出 ========================
static long syscall_hook_demo_init(const char *args, const char *event, void *__user reserved)
{
    logv("TearGame KPM v3.3.0 init\n");
    
    kfunc_lookup_name(__arch_copy_from_user);
    kfunc_lookup_name(find_task_by_vpid);
    kfunc_lookup_name(access_process_vm);
    kfunc_lookup_name(memremap);
    kfunc_lookup_name(memunmap);
    
    logv("symbols: access_process_vm=%s find_task_by_vpid=%s\n",
         kfunc(access_process_vm) ? "Y" : "N",
         kfunc(find_task_by_vpid) ? "Y" : "N");
    logv("symbols: memremap=%s memunmap=%s\n",
         kfunc(memremap) ? "Y" : "N",
         kfunc(memunmap) ? "Y" : "N");
    
    if (kfunc(find_task_by_vpid)) {
        probe_comm_offset();
    }
    
    // 检查物理内存功能是否可用
    g_phys_mem_available = (kfunc(access_process_vm) != NULL) ? 1 : 0;
    logv("phys_mem: %s\n", g_phys_mem_available ? "enabled" : "disabled");
    
    hook_err_t err = fp_hook_syscalln(__NR_prctl, 5, before_prctl, 0, 0);
    logv("hook prctl: %s\n", err ? "FAIL" : "OK");
    
    return 0;
}

static long syscall_hook_control0(const char *args, char *__user out_msg, int outlen)
{
    return 0;
}

static long syscall_hook_demo_exit(void *__user reserved)
{
    logv("TearGame KPM exit\n");
    fp_unhook_syscalln(__NR_prctl, before_prctl, 0);
    return 0;
}

KPM_INIT(syscall_hook_demo_init);
KPM_CTL0(syscall_hook_control0);
KPM_EXIT(syscall_hook_demo_exit);
