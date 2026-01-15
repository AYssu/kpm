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
 #include <linux/cred.h>
//  #include <limits.h>
 #include "obfuscate.h"
KPM_NAME("Hook SYS_process_vm_readv");
KPM_VERSION("1.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("AYssu");
KPM_DESCRIPTION("SYS_process_vm_readv 消失?");

// ======================== 日志控制开关 ========================
// 定义 ENABLE_DEBUG_LOG 为 1 启用日志，为 0 禁用日志
#define ENABLE_DEBUG_LOG 1

#if ENABLE_DEBUG_LOG
    #define TAG "[FastScan] "
    #define logv(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)
#else
    #define logv(fmt, ...) do {} while(0)  // 日志关闭时不输出
#endif
// ============================================================

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

// ===== 映射缓存机制（减少 ioremap 调用，提升隐蔽性）=====
#define MAPPING_CACHE_SIZE 4
static struct {
    phys_addr_t pa;
    void __iomem *vaddr;
    s64 last_use;  // 秒级时间戳
} mapping_cache[MAPPING_CACHE_SIZE] = {{0}};

// 清理映射缓存
static void cleanup_mapping_cache(void)
{
    int i;
    for (i = 0; i < MAPPING_CACHE_SIZE; i++) {
        if (mapping_cache[i].vaddr) {
            __iounmap(mapping_cache[i].vaddr);
            mapping_cache[i].vaddr = NULL;
            mapping_cache[i].pa = 0;
        }
    }
    logv("Mapping cache cleaned up\n");
}

// 读取物理地址 - 使用缓存优化（减少90%+ ioremap调用）
static size_t read_physical_address(phys_addr_t pa, void __user *buffer, size_t size)
{
    void __iomem *mapped = NULL;
    int cache_idx = -1;
    int empty_idx = -1;
    int oldest_idx = 0;
    s64 now = ktime_get_real_seconds();  // 秒级时间戳
    s64 oldest_time = now;
    phys_addr_t page_pa;
    unsigned long offset;
    int i;
    
    // 验证物理地址
    if (!pfn_valid(__phys_to_pfn(pa))) {
        logv("Invalid PFN for pa=0x%llx\n", pa);
        return 0;
    }
    
    // 计算页对齐的物理地址
    page_pa = pa & PAGE_MASK;
    offset = pa & (PAGE_SIZE - 1);
    
    // 确保读取不跨页
    if (offset + size > PAGE_SIZE) {
        size = PAGE_SIZE - offset;
    }
    
    // ===== 查找缓存 =====
    for (i = 0; i < MAPPING_CACHE_SIZE; i++) {
        if (mapping_cache[i].vaddr) {
            // 检查是否命中（同一页）
            if (mapping_cache[i].pa == page_pa) {
                cache_idx = i;
                mapping_cache[i].last_use = now;
                mapped = mapping_cache[i].vaddr;
                logv("Cache hit! idx=%d, pa=0x%llx\n", i, page_pa);
                break;
            }
            // 记录最旧的缓存
            if (mapping_cache[i].last_use < oldest_time) {
                oldest_time = mapping_cache[i].last_use;
                oldest_idx = i;
            }
            // 清理过期缓存（超过2秒未使用）
            if (now - mapping_cache[i].last_use > 2) {  // 2秒
                __iounmap(mapping_cache[i].vaddr);
                mapping_cache[i].vaddr = NULL;
                mapping_cache[i].pa = 0;
                empty_idx = i;
                logv("Cache expired: idx=%d\n", i);
            }
        } else if (empty_idx < 0) {
            empty_idx = i;
        }
    }
    
    // ===== 缓存未命中，创建新映射 =====
    if (!mapped) {
        logv("Cache miss, creating new mapping for pa=0x%llx\n", page_pa);
        
        // 选择缓存槽：优先空槽，否则替换最旧的
        if (empty_idx >= 0) {
            cache_idx = empty_idx;
        } else {
            cache_idx = oldest_idx;
            // 清理旧映射
            if (mapping_cache[cache_idx].vaddr) {
                __iounmap(mapping_cache[cache_idx].vaddr);
                logv("Evicting old cache: idx=%d\n", cache_idx);
            }
        }
        
        // 创建新映射（按页大小映射，提高缓存命中率）
        mapped = ioremap_cache(page_pa, PAGE_SIZE);
        if (!mapped) {
            logv("Failed to ioremap_cache: pa=0x%llx\n", page_pa);
            return 0;
        }
        
        // 保存到缓存
        mapping_cache[cache_idx].pa = page_pa;
        mapping_cache[cache_idx].vaddr = mapped;
        mapping_cache[cache_idx].last_use = now;
        logv("New mapping cached: idx=%d, pa=0x%llx\n", cache_idx, page_pa);
    }
    
    // ===== 复制数据 =====
    // 注意：ioremap 返回的是 IO 内存，需要先复制到内核缓冲区，再复制到用户空间
    unsigned char kernel_buffer[4096];  // 临时内核缓冲区
    
    // 从 IO 内存复制到内核缓冲区
    memcpy(kernel_buffer, (const void *)(mapped + offset), size);
    
    // 从内核缓冲区复制到用户空间
   compat_copy_to_user(buffer, kernel_buffer, size);
      
    
    logv("Read successful: pa=0x%llx, offset=0x%lx, size=%zu (cached=%s)\n", 
         page_pa, offset, size, cache_idx >= 0 ? "yes" : "no");
    
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

// ===== process_vm_readv Hook 实现 =====
// iovec 结构体定义（用户空间和内核空间都需要）
struct iovec_compat {
    void __user *iov_base;  // 缓冲区基址
    size_t iov_len;         // 缓冲区长度
};

// process_vm_readv syscall signature:
// ssize_t process_vm_readv(pid_t pid,
//                          const struct iovec *local_iov, unsigned long liovcnt,
//                          const struct iovec *remote_iov, unsigned long riovcnt,
//                          unsigned long flags);
void before_process_vm_readv(hook_fargs6_t *args, void *udata)
{
    pid_t target_pid = (pid_t)syscall_argn(args, 0);
    const struct iovec_compat __user *local_iov = (const struct iovec_compat __user *)syscall_argn(args, 1);
    unsigned long liovcnt = (unsigned long)syscall_argn(args, 2);
    const struct iovec_compat __user *remote_iov = (const struct iovec_compat __user *)syscall_argn(args, 3);
    unsigned long riovcnt = (unsigned long)syscall_argn(args, 4);
    
    struct iovec_compat local_vec, remote_vec;
    ssize_t total_read = 0;
    unsigned long i, j;
    int result;
    
    logv("process_vm_readv: pid=%d, liovcnt=%lu, riovcnt=%lu\n", 
         target_pid, liovcnt, riovcnt);
    
    // 基本参数验证
    if (liovcnt == 0 || riovcnt == 0 || liovcnt > 1024 || riovcnt > 1024) {
        args->ret = -EINVAL;
        return;
    }
    
    // 遍历所有的 iovec 对
    for (i = 0; i < liovcnt && total_read >= 0; i++) {
        // 复制本地 iovec
        if (__arch_copy_from_user(&local_vec, &local_iov[i], sizeof(local_vec)) != 0) {
            args->ret = -EFAULT;
            return;
        }
        
        if (!local_vec.iov_base || local_vec.iov_len == 0) {
            continue;
        }
        
        // 遍历远程 iovec
        for (j = 0; j < riovcnt && total_read >= 0; j++) {
            // 复制远程 iovec
            if (__arch_copy_from_user(&remote_vec, &remote_iov[j], sizeof(remote_vec)) != 0) {
                args->ret = -EFAULT;
                return;
            }
            
            if (!remote_vec.iov_base || remote_vec.iov_len == 0) {
                continue;
            }
            
            // 计算实际要读取的大小（取较小值）
            size_t read_size = local_vec.iov_len < remote_vec.iov_len ? 
                              local_vec.iov_len : remote_vec.iov_len;
            
            logv("Reading: local=%p, remote=%p, size=%zu\n", 
                 local_vec.iov_base, remote_vec.iov_base, read_size);
            
            // 使用我们的物理内存读取函数
            result = read_mem(target_pid, 
                            (uintptr_t)remote_vec.iov_base,
                            local_vec.iov_base, 
                            read_size);
            
            if (result == 0) {
                total_read += read_size;
                logv("Read successful: %zu bytes, total=%zd\n", read_size, total_read);
            } else {
                logv("Read failed: result=%d\n", result);
                // 部分读取也算成功
                if (total_read > 0) {
                    break;
                } else {
                    args->ret = -EIO;
                    return;
                }
            }
            
            // 如果读取完成，跳出内层循环
            if (read_size >= local_vec.iov_len) {
                break;
            }
        }
    }
    
    // 设置返回值（读取的总字节数）
    args->ret = total_read;
    logv("process_vm_readv completed: total_read=%zd\n", total_read);
}

static long syscall_hook_demo_init(const char *args, const char *event, void *__user reserved)
{
    logv("FastScan module init, args: %s\n", args);
    logv("Platform: ARM64/aarch64\n");

    // 查找所有需要的内核函数
    logv("Looking up kernel functions...\n");
    kvar_lookup_name(memstart_addr);
    kfunc_lookup_name(__arch_copy_from_user);
    kfunc_lookup_name(find_task_by_vpid);
    kfunc_lookup_name(mmput);
    kfunc_lookup_name(get_task_mm);
    kfunc_lookup_name(pfn_valid);
    kfunc_lookup_name(ioremap_cache);
    kfunc_lookup_name(__iounmap);
    kfunc_lookup_name(ktime_get_real_seconds);
    logv("All kernel functions resolved\n");

    hook_err_t err = HOOK_NO_ERR;

    // Hook process_vm_readv (推荐使用，更隐蔽)
    logv("hooking process_vm_readv syscall (nr=%d)...\n", __NR_process_vm_readv);
    err = fp_hook_syscalln(__NR_process_vm_readv, 6, before_process_vm_readv, 0, 0);
    
    if (err) {
        logv("hook process_vm_readv error: %d\n", err);
    } else {
        logv("hook process_vm_readv success\n");
    }

    // 可选：同时保留 ioctl hook 作为备用方案
    // logv("hooking ioctl syscall (nr=%d)...\n", __NR_ioctl);
    // err = fp_hook_syscalln(__NR_ioctl, 3, before_ioctl, 0, 0);
    // 
    // if (err) {
    //     logv("hook ioctl error: %d\n", err);
    // } else {
    //     logv("hook ioctl success\n");
    // }

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

     // 清理映射缓存（重要：避免内存泄漏）
     cleanup_mapping_cache();

     // Unhook process_vm_readv
     fp_unhook_syscalln(__NR_process_vm_readv, before_process_vm_readv, 0);
     
     // 如果同时hook了ioctl，也需要unhook
     // fp_unhook_syscalln(__NR_ioctl, before_ioctl, 0);
     
     return 0;
 }
 
 KPM_INIT(syscall_hook_demo_init);
 KPM_CTL0(syscall_hook_control0);
 KPM_EXIT(syscall_hook_demo_exit);