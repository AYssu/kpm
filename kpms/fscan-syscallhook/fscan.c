/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
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
//  #include <limits.h>
 #include "obfuscate.h"
 // ioctl 宏定义（直接定义，避免头文件依赖）
 #ifndef _IOC_NRBITS
 #define _IOC_NRBITS     8
 #define _IOC_TYPEBITS   8
 #define _IOC_SIZEBITS   14
 #define _IOC_DIRBITS    2
 
 #define _IOC_NRSHIFT    0
 #define _IOC_TYPESHIFT  (_IOC_NRSHIFT+_IOC_NRBITS)
 #define _IOC_SIZESHIFT  (_IOC_TYPESHIFT+_IOC_TYPEBITS)
 #define _IOC_DIRSHIFT   (_IOC_SIZESHIFT+_IOC_SIZEBITS)
 
 #define _IOC_NONE       0U
 #define _IOC_WRITE      1U
 #define _IOC_READ       2U
 
 #define _IOC(dir,type,nr,size) \
     (((dir)  << _IOC_DIRSHIFT) | \
      ((type) << _IOC_TYPESHIFT) | \
      ((nr)   << _IOC_NRSHIFT) | \
      ((size) << _IOC_SIZESHIFT))
 
 #define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
 #endif
 
KPM_NAME("FastScan");
KPM_VERSION("1.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("AYssu");
KPM_DESCRIPTION("FastScan专用内存读取模块?");

// ======================== 日志控制开关 ========================
// 定义 ENABLE_DEBUG_LOG 为 1 启用日志，为 0 禁用日志
#define ENABLE_DEBUG_LOG 0

#if ENABLE_DEBUG_LOG
    #define TAG "[FastScan] "
    #define logv(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)
#else
    #define logv(fmt, ...) do {} while(0)  // 日志关闭时不输出
#endif
// ============================================================
 
 // ioctl command definitions
 #define OP_READ_MEM    777
 #define OP_WRITE_MEM   778
 
 // Memory operation structure
 struct mem_operation {
     pid_t target_pid;      // 目标进程 PID
     uint64_t addr;         // 目标地址
     void __user *buffer;   // 用户空间缓冲区
     uint64_t size;         // 读写大小
 };
 
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

// 虽然 find_task_by_vpid 在 sched.h 中有声明，但 KPM 需要动态查找实现
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

int kfunc_def(valid_phys_addr_range)(phys_addr_t addr, size_t size);
static inline int valid_phys_addr_range(phys_addr_t addr, size_t size)
{
    kfunc_call(valid_phys_addr_range, addr, size);
    kfunc_not_found();
    return 0;
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

void __iomem *kfunc_def(ioremap_cache)(resource_size_t offset, unsigned long size);
static inline void __iomem *ioremap_cache(resource_size_t offset, unsigned long size)
{
    kfunc_call(ioremap_cache, offset, size);
    kfunc_not_found();
    return NULL;
}

// 获取当前时间戳（秒）- 使用 s64 代替 time64_t
s64 kfunc_def(ktime_get_real_seconds)(void);
static inline s64 ktime_get_real_seconds(void)
{
    kfunc_call(ktime_get_real_seconds);
    kfunc_not_found();
    return 0;
}

// 虚拟地址转物理地址 - 使用 KPM 提供的安全函数
static uintptr_t _pid_virt_to_phys(pid_t pid, uintptr_t addr) 
{
    if (mm_struct_offset.pgd_offset < 0) {
        logv("mm_struct_offset.pgd_offset not initialized\n");
        return 0;
    }
    
    // 获取页表配置
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
    
    // 查找进程
    struct task_struct *task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("No such pid: %d\n", pid);
        return 0;
    }

    // 获取内存描述符
    struct mm_struct *mm = get_task_mm(task);
    if (!mm || IS_ERR(mm)) {
        logv("Failed to get mm for pid: %d\n", pid);
        return 0;
    }
    
    // 获取页表基址
    uintptr_t pgd_base = *(uintptr_t *)((uintptr_t)mm + mm_struct_offset.pgd_offset);
    logv("pgd_base: 0x%lx\n", pgd_base);
    
    // 使用 KPM 提供的安全页表遍历函数
    uint64_t* pte_ptr = pgtable_entry(pgd_base, addr);
    if (!pte_ptr) {
        mmput(mm);
        logv("Address 0x%lx not mapped (pgtable_entry returned NULL)\n", addr);
        return 0;
    }
    
    pte_t* pte = (pte_t*)pte_ptr;
    uint64_t pte_value = pte_val(*pte);
    logv("pte_value: 0x%llx\n", pte_value);
    
    // 检查 PTE 是否有效
    if (!pte_value) {
        mmput(mm);
        logv("PTE is zero\n");
        return 0;
    }
    
    // 检查 present bit
    if (!(pte_value & 1)) {
        mmput(mm);
        logv("PTE not present (bit 0 not set)\n");
        return 0;
    }
    
    // 计算物理地址
    phys_addr_t page_addr = pte_pfn(*pte) << PAGE_SHIFT;
    uintptr_t page_offset = addr & (PAGE_SIZE - 1);
    phys_addr_t phys_addr = page_addr + page_offset;
    
    mmput(mm);
    
    logv("virt: 0x%lx -> phys: 0x%lx (page: 0x%llx, offset: 0x%lx)\n", 
         addr, phys_addr, page_addr, page_offset);
    
    return phys_addr;
}

// 读取物理地址 - 分页读取
static size_t read_physical_address(phys_addr_t pa, void __user *buffer, size_t size)
{
    void __iomem *mapped;
    // 验证物理地址
    if (!pfn_valid(__phys_to_pfn(pa))) {
        logv("Invalid PFN for pa=0x%llx\n", pa);
        return 0;
    }
    if (!valid_phys_addr_range(pa, size)) {
        logv("Invalid physical address range: pa=0x%llx, size=%zu\n", pa, size);
        return 0;
    }
    // 映射物理内存 - 使用 ioremap_cache
    mapped = ioremap_cache(pa, size);
    if (!mapped) {
        logv("Failed to ioremap_cache: pa=0x%llx, size=%zu\n", pa, size);
        return 0;
    }
    // 复制到用户空间
     compat_copy_to_user(buffer, (const void *)mapped, size);
    // 解除映射
    __iounmap(mapped);
   
    return size;
}

// 核心读取函数 - 分页读取，避免跨页问题
static int read_mem(pid_t pid, uintptr_t addr, void __user *buffer, size_t size)
{
    phys_addr_t pa;
    size_t max;
    size_t total_read = 0;
    size_t bytes_read;
    uintptr_t current_addr = addr;
    void __user *current_buffer = buffer;
    size_t remaining = size;
    
    logv("read_mem: pid=%d, addr=0x%lx, size=%zu\n", pid, addr, size);
    
    // 分页读取，避免跨页问题
    while (remaining > 0) {
        // 计算当前页最多能读取多少字节
        max = PAGE_SIZE - (current_addr & (PAGE_SIZE - 1));
        if (max > remaining) {
            max = remaining;
        }
        
        logv("Reading chunk: addr=0x%lx, size=%zu\n", current_addr, max);
        
        // 虚拟地址转物理地址
        pa = _pid_virt_to_phys(pid, current_addr);
        if (!pa) {
            logv("Virtual to physical translation failed for addr=0x%lx\n", current_addr);
            // 跳过这一部分，继续下一页
            goto next_chunk;
        }
        
        // 读取这一页
        bytes_read = read_physical_address(pa, current_buffer, max);
        if (bytes_read > 0) {
            total_read += bytes_read;
        } else {
            logv("Failed to read physical address 0x%llx\n", pa);
        }
        
    next_chunk:
        remaining -= max;
        current_addr += max;
        current_buffer += max;
    }
    
    if (total_read == size) {
        logv("Successfully read all %zu bytes\n", size);
        return 0;
    } else if (total_read > 0) {
        logv("Partially read %zu/%zu bytes\n", total_read, size);
        return 0; // 返回成功，即使只读取部分
    } else {
        logv("Failed to read any data %zu/%zu\n", total_read, size);
        return -1;
    }
}

// ioctl syscall signature:
 // long ioctl(int fd, unsigned long cmd, unsigned long arg);
 void before_ioctl(hook_fargs3_t *args, void *udata)
 {
     int fd = (int)syscall_argn(args, 0);
     unsigned long cmd = (unsigned long)syscall_argn(args, 1);
     unsigned long arg = (unsigned long)syscall_argn(args, 2);
     s64 current_timestamp;
     s64 received_timestamp;
     s64 time_diff;
     struct mem_operation cm;
     int result;
 
    // 时间戳验证：fd作为时间戳传入，验证是否在10秒时间窗口内
    current_timestamp = ktime_get_real_seconds();
    received_timestamp = (s64)fd;
    
    // 计算时间差的绝对值
    if (current_timestamp > received_timestamp) {
        time_diff = current_timestamp - received_timestamp;
    } else {
        time_diff = received_timestamp - current_timestamp;
    }
    
    // 检查时间窗口（10秒）
    if (time_diff > 10) {

        args->ret = -EPERM;  // Permission denied
        return;
    }
    

    // 只处理我们关心的 ioctl 命令
    if (cmd == OP_READ_MEM) {
         logv("Timestamp validation passed: diff=%lld seconds\n", time_diff);

        // 从用户空间复制参数
        if (__arch_copy_from_user(&cm, (void __user *)arg, sizeof(cm)) != 0) {
            logv("ioctl: failed to copy mem_operation from user\n");
            args->ret = -EFAULT;
            return;
        }

        logv("ioctl READ - fd=%d, target_pid=%d, addr=0x%llx, size=%llu\n", 
             fd, cm.target_pid, cm.addr, cm.size);
        
        // 调用读取函数
        result = read_mem(cm.target_pid, cm.addr, cm.buffer, cm.size);
        
        if (result == 0) {
            logv("Read operation successful\n");
            args->ret = 0;
        } else {
            logv("Read operation failed with result: %d\n", result);
            args->ret = -EIO;
        }
        
        return;
    }
}
 
static long syscall_hook_demo_init(const char *args, const char *event, void *__user reserved)
{
    logv("FastScan module init, args: %s\n", args);
    logv("Platform: ARM64/aarch64\n");
    logv("Syscall number __NR_ioctl: %d\n", __NR_ioctl);
    logv("OP_READ_MEM cmd: 0x%lx\n", (unsigned long)OP_READ_MEM);
    logv("OP_WRITE_MEM cmd: 0x%lx\n", (unsigned long)OP_WRITE_MEM);

    // 查找所有需要的内核函数
    logv("Looking up kernel functions...\n");
    kvar_lookup_name(memstart_addr);
    kfunc_lookup_name(__arch_copy_from_user);
    kfunc_lookup_name(find_task_by_vpid);
    kfunc_lookup_name(mmput);
    kfunc_lookup_name(get_task_mm);
    kfunc_lookup_name(valid_phys_addr_range);
    kfunc_lookup_name(pfn_valid);
    kfunc_lookup_name(ioremap_cache);
    kfunc_lookup_name(__iounmap);
    kfunc_lookup_name(ktime_get_real_seconds);
    logv("All kernel functions resolved\n");

    hook_err_t err = HOOK_NO_ERR;

    logv("hooking ioctl syscall (nr=%d)...\n", __NR_ioctl);
    err = fp_hook_syscalln(__NR_ioctl, 3, before_ioctl, 0, 0);
    
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
 
     fp_unhook_syscalln(__NR_ioctl, before_ioctl, 0);
     return 0;
 }
 
 KPM_INIT(syscall_hook_demo_init);
 KPM_CTL0(syscall_hook_control0);
 KPM_EXIT(syscall_hook_demo_exit);