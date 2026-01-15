/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 * 
 * 原作者: 阿夜 (AYssu)
 * GitHub: https://github.com/AYssu
 * 
 * 二次开发: 泪心 (TearHacker)
 * GitHub: https://github.com/tearhacker
 * Telegram: t.me/TearGame
 * QQ: 2254013571
 */

 #include <compiler.h>
 #include <kpmodule.h>
 #include <linux/printk.h>
 #include <uapi/asm-generic/unistd.h>
 #include <linux/uaccess.h>
 #include <syscall.h>
 #include <linux/string.h>
 #include <kputils.h>
 #include <asm/current.h>
 #include <ktypes.h>
 #include <pgtable.h>
 #include <linux/err.h>
 #include <linux/pid.h>
 #include <linux/sched.h>
 #include <linux/mm_types.h>
 #include <linux/errno.h>
 #include <linux/cred.h>
 #include "obfuscate.h"

KPM_NAME("FastScan");
KPM_VERSION("1.0.1");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("AYssu");
KPM_DESCRIPTION("FastScan内存读写模块");

// ======================== 日志控制开关 ========================
#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
    #define TAG "[FastScan] "
    #define logv(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)
#else
    #define logv(fmt, ...) do {} while(0)
#endif
// ============================================================

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#define __KERNEL__
#include "fscan_ioctl.h"
 
// 页表相关配置
int64_t phys_addr_size1 = (1UL << 0x20);
int64_t kvar_def(memstart_addr);
int64_t page_shift = 12;
int64_t page_size = 4096;
uint64_t page_offset1 = (1UL << 0x20);

#define PHYS_OFFSET kvar_val(memstart_addr)
#define PAGE_SHIFT page_shift
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~((1ULL << PAGE_SHIFT) - 1))
#define PHYS_MASK 0x3FFFFFF000UL
#define __phys_to_pfn(phys) ((phys) >> PAGE_SHIFT)
#define __virt_to_phys(x) (((phys_addr_t)(x) - page_offset1 + PHYS_OFFSET))
#define __phys_to_virt(x) ((unsigned long)((x) - PHYS_OFFSET + page_offset1))
#define __pa(x) __virt_to_phys((unsigned long)(x))
#define __va(x) ((void *)__phys_to_virt((phys_addr_t)(x)))

typedef u64 pteval_t;
typedef struct { pteval_t pte; } pte_t;

#define pte_val(x) ((x).pte)
#define __pte(x) ((pte_t) { (x) })
#define pte_pfn(pte) ((pte_val(pte) & PHYS_MASK) >> PAGE_SHIFT)

// 内核函数声明
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

void kfunc_def(mmput)(struct mm_struct *);
static inline void mmput(struct mm_struct *mm)
{
    kfunc_call_void(mmput, mm);
}

struct mm_struct *kfunc_def(get_task_mm)(struct task_struct *task);
static inline struct mm_struct *get_task_mm(struct task_struct *task)
{
    kfunc_call(get_task_mm, task);
    kfunc_not_found();
    return NULL;
}

int kfunc_def(pfn_valid)(unsigned long pfn);
static inline int pfn_valid(unsigned long pfn)
{
    kfunc_call(pfn_valid, pfn);
    kfunc_not_found();
    return 0;
}

void kfunc_def(__iounmap)(void __iomem *addr);
static inline void __iounmap(void __iomem *addr)
{
    kfunc_call_void(__iounmap, addr);
}

void __iomem *kfunc_def(ioremap_prot)(resource_size_t offset, unsigned long size, unsigned long prot);
void __iomem *kfunc_def(generic_ioremap_prot)(resource_size_t offset, unsigned long size, unsigned long prot);
void __iomem *kfunc_def(ioremap_cache)(resource_size_t offset, unsigned long size);

static inline void __iomem *__ioremap(resource_size_t offset, unsigned long size)
{
    if (kfunc(ioremap_prot)) {
        return kfunc(ioremap_prot)(offset, size, 0x63);
    }
    if (kfunc(generic_ioremap_prot)) {
        return kfunc(generic_ioremap_prot)(offset, size, 0x63);
    }
    if (kfunc(ioremap_cache)) {
        return kfunc(ioremap_cache)(offset, size);
    }
    kfunc_not_found();
    return NULL;
}

s64 kfunc_def(ktime_get_real_seconds)(void);
static inline s64 ktime_get_real_seconds(void)
{
    kfunc_call(ktime_get_real_seconds);
    kfunc_not_found();
    return 0;
}

// 虚拟地址转物理地址
static uintptr_t _pid_virt_to_phys(pid_t pid, uintptr_t addr) 
{
    if (mm_struct_offset.pgd_offset < 0) {
        logv("mm_struct_offset.pgd_offset not initialized\n");
        return 0;
    }
    
    uint64_t tcr_el1;
    asm volatile("mrs %0, tcr_el1" : "=r"(tcr_el1));
    uint64_t tg1 = tcr_el1 << 32 >> 62;
    uint64_t t1sz = (tcr_el1 >> 16) & 0x1F;
    uint64_t ips = (tcr_el1 >> 30) & 0x3;
    
    page_offset1 = (1ULL << (64 - t1sz));
    phys_addr_size1 = 32 + (8 * ips);
    page_shift = 12;
    
    if (tg1 == 1) {
        page_shift = 14;
        phys_addr_size1 = 40;
    } else if (tg1 == 3) {
        page_shift = 16;
        phys_addr_size1 = 42;
    }
    
    page_size = 1 << page_shift;
    
    logv("virt_to_phys: pid=%d, addr=0x%lx\n", pid, addr);
    
    struct task_struct *task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("No such pid: %d\n", pid);
        return 0;
    }

    struct mm_struct *mm = get_task_mm(task);
    if (!mm || IS_ERR(mm)) {
        logv("Failed to get mm for pid: %d\n", pid);
        return 0;
    }
    
    uintptr_t pgd_base = *(uintptr_t *)((uintptr_t)mm + mm_struct_offset.pgd_offset);
    logv("pgd_base: 0x%lx\n", pgd_base);
    
    uint64_t* pte_ptr = pgtable_entry(pgd_base, addr);
    if (!pte_ptr) {
        mmput(mm);
        logv("Address 0x%lx not mapped\n", addr);
        return 0;
    }
    
    pte_t* pte = (pte_t*)pte_ptr;
    uint64_t pte_value = pte_val(*pte);
    
    if (!pte_value || !(pte_value & 1)) {
        mmput(mm);
        logv("PTE invalid\n");
        return 0;
    }
    
    phys_addr_t page_addr = pte_pfn(*pte) << PAGE_SHIFT;
    uintptr_t page_offset = addr & (PAGE_SIZE - 1);
    phys_addr_t phys_addr = page_addr + page_offset;
    
    mmput(mm);
    
    logv("virt: 0x%lx -> phys: 0x%lx\n", addr, phys_addr);
    return phys_addr;
}

static void cleanup_mapping_cache(void)
{
    logv("Module cleanup\n");
}

static uint8_t kernel_buffer[4096] __attribute__((aligned(8)));

// 读取物理地址
static size_t read_physical_address(phys_addr_t pa, void __user *buffer, size_t size)
{
    void __iomem *mapped = NULL;
    int n;
    
    if (size > sizeof(kernel_buffer)) {
        size = sizeof(kernel_buffer);
    }
    
    if (!pfn_valid(__phys_to_pfn(pa))) {
        logv("Invalid PFN for pa=0x%llx\n", pa);
        return 0;
    }
    
    mapped = __ioremap(pa, size);
    if (!mapped) {
        logv("Failed to __ioremap: pa=0x%llx\n", pa);
        return 0;
    }
    
    n = compat_copy_to_user(buffer, mapped, size);
    __iounmap(mapped);
    
    if (n != (int)size) {
        return 0;
    }
    
    return size;
}

// 核心读取函数
static int read_mem(pid_t pid, uintptr_t addr, void __user *buffer, size_t size)
{
    phys_addr_t pa;
    size_t max;
    size_t total_read = 0;
    size_t bytes_read;
    uintptr_t current_addr = addr;
    void __user *current_buffer = buffer;
    size_t remaining = size;
    
    while (remaining > 0) {
        max = PAGE_SIZE - (current_addr & (PAGE_SIZE - 1));
        if (max > remaining) max = remaining;
        
        pa = _pid_virt_to_phys(pid, current_addr);
        if (pa) {
            bytes_read = read_physical_address(pa, current_buffer, max);
            if (bytes_read > 0) total_read += bytes_read;
        }
        
        remaining -= max;
        current_addr += max;
        current_buffer += max;
    }
    
    return (total_read > 0) ? 0 : -1;
}

// 写入物理地址
static size_t write_physical_address(phys_addr_t pa, const void __user *buffer, size_t size)
{
    void __iomem *mapped = NULL;
    int n;
    
    if (size > sizeof(kernel_buffer)) {
        size = sizeof(kernel_buffer);
    }
    
    if (!pfn_valid(__phys_to_pfn(pa))) {
        return 0;
    }
    
    n = __arch_copy_from_user(kernel_buffer, buffer, size);
    if (n != 0) {
        return 0;
    }
    
    mapped = __ioremap(pa, size);
    if (!mapped) {
        return 0;
    }
    
    memcpy((void *)mapped, kernel_buffer, size);
    __iounmap(mapped);
    
    return size;
}

// 核心写入函数
static int write_mem(pid_t pid, uintptr_t addr, const void __user *buffer, size_t size)
{
    phys_addr_t pa;
    size_t max;
    size_t total_written = 0;
    size_t bytes_written;
    uintptr_t current_addr = addr;
    const void __user *current_buffer = buffer;
    size_t remaining = size;
    
    while (remaining > 0) {
        max = PAGE_SIZE - (current_addr & (PAGE_SIZE - 1));
        if (max > remaining) max = remaining;
        
        pa = _pid_virt_to_phys(pid, current_addr);
        if (pa) {
            bytes_written = write_physical_address(pa, current_buffer, max);
            if (bytes_written > 0) total_written += bytes_written;
        }
        
        remaining -= max;
        current_addr += max;
        current_buffer += max;
    }
    
    return (total_written > 0) ? 0 : -1;
}

// 允许的 UID
#define ALLOWED_UID_1  0

// ioctl hook
void before_ioctl(hook_fargs3_t *args, void *udata)
{
    int fd = (int)syscall_argn(args, 0);
    unsigned long cmd = (unsigned long)syscall_argn(args, 1);
    unsigned long arg = (unsigned long)syscall_argn(args, 2);
    s64 current_timestamp;
    s64 received_timestamp;
    s64 time_diff;
    int result;
    uid_t caller_uid;
    
    // 快速过滤：只处理内存读写命令
    if (cmd != OP_READ_MEM && cmd != OP_WRITE_MEM) {
        return;
    }
 
    // UID 验证
    struct cred *cred = *(struct cred **)((uintptr_t)current + task_struct_offset.cred_offset);
    caller_uid = *(uid_t *)((uintptr_t)cred + cred_offset.uid_offset);
    
    if (caller_uid != ALLOWED_UID_1) {
        args->ret = -EINVAL;
        args->skip_origin = 1;
        return;
    }

    // 时间戳验证
    current_timestamp = ktime_get_real_seconds();
    received_timestamp = (s64)fd;
    
    if (current_timestamp > received_timestamp) {
        time_diff = current_timestamp - received_timestamp;
    } else {
        time_diff = received_timestamp - current_timestamp;
    }
    
    if (time_diff > 3) {
        args->ret = -EBUSY;
        args->skip_origin = 1;
        return;
    }

    // 内存读写操作
    struct mem_operation cm;
    if (__arch_copy_from_user(&cm, (void __user *)arg, sizeof(cm)) != 0) {
        args->ret = -EFAULT;
        args->skip_origin = 1;
        return;
    }

    logv("ioctl %s - pid=%d, addr=0x%llx, size=%llu\n", 
         cmd == OP_READ_MEM ? "READ" : "WRITE", cm.target_pid, cm.addr, cm.size);
    
    if (cmd == OP_READ_MEM) {
        result = read_mem(cm.target_pid, cm.addr, cm.buffer, cm.size);
    } else {
        result = write_mem(cm.target_pid, cm.addr, cm.buffer, cm.size);
    }
    
    args->ret = (result == 0) ? 0 : -EIO;
    args->skip_origin = 1;
}
 
static long syscall_hook_demo_init(const char *args, const char *event, void *__user reserved)
{
    logv("FastScan module init, args: %s\n", args);
    logv("Platform: ARM64/aarch64\n");
    logv("Syscall number __NR_ioctl: %d\n", __NR_ioctl);
    logv("OP_READ_MEM cmd: 0x%lx\n", (unsigned long)OP_READ_MEM);
    logv("OP_WRITE_MEM cmd: 0x%lx\n", (unsigned long)OP_WRITE_MEM);

    logv("Looking up kernel functions...\n");
    kvar_lookup_name(memstart_addr);
    kfunc_lookup_name(__arch_copy_from_user);
    kfunc_lookup_name(find_task_by_vpid);
    kfunc_lookup_name(mmput);
    kfunc_lookup_name(get_task_mm);
    kfunc_lookup_name(pfn_valid);
    kfunc_lookup_name(ioremap_prot);
    kfunc_lookup_name(generic_ioremap_prot);
    kfunc_lookup_name(ioremap_cache);
    kfunc_lookup_name(__iounmap);
    kfunc_lookup_name(ktime_get_real_seconds);
    logv("All kernel functions resolved\n");
    
    logv("task_struct_offset: pid=%d, comm=%d, mm=%d\n",
         task_struct_offset.pid_offset, 
         task_struct_offset.comm_offset,
         task_struct_offset.mm_offset);
    logv("mm_struct_offset: pgd=%d\n", mm_struct_offset.pgd_offset);

    hook_err_t err = fp_hook_syscalln(__NR_ioctl, 3, before_ioctl, 0, 0);
    
    if (err) {
        logv("hook ioctl error: %d\n", err);
    } else {
        logv("hook ioctl success\n");
    }
    return 0;
}
 
static long syscall_hook_control0(const char *args, char *__user out_msg, int outlen)
{
    logv("syscall_hook control, args: %s\n", args);
    return 0;
}
 
static long syscall_hook_demo_exit(void *__user reserved)
{
    logv("FastScan module exit\n");
    cleanup_mapping_cache();
    fp_unhook_syscalln(__NR_ioctl, before_ioctl, 0);
    return 0;
}
 
KPM_INIT(syscall_hook_demo_init);
KPM_CTL0(syscall_hook_control0);
KPM_EXIT(syscall_hook_demo_exit);
