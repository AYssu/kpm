/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

 #include <compiler.h>
 #include <kpmodule.h>
 #include <linux/printk.h>
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
 #include <linux/cred.h>
 #include <linux/llist.h>
 #include "obfuscate.h"

KPM_NAME("FastScan");
#ifndef KPM_BUILD_VERSION
#define KPM_BUILD_VERSION "2.0.0"
#endif
KPM_VERSION(KPM_BUILD_VERSION);
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

// 包含共享的 prctl 接口定义
#define __KERNEL__
#include "fscan_prctl.h"

// ========================================================================
 
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

// 注意：__task_pid_nr_ns 在某些内核中未导出，需要动态查找
// 我们创建一个包装函数来使用它
pid_t kfunc_def(__task_pid_nr_ns)(struct task_struct *task, enum pid_type type, struct pid_namespace *ns);
static inline pid_t my_task_pid_nr_ns(struct task_struct *task, enum pid_type type, struct pid_namespace *ns)
{
    kfunc_call(__task_pid_nr_ns, task, type, ns);
    kfunc_not_found();
    return 0;  // 查找失败返回 0
}

// find_vma - 暂时禁用（会导致系统崩溃）
// struct vm_area_struct * kfunc_def(find_vma)(struct mm_struct * mm, unsigned long addr);
// static inline struct vm_area_struct * find_vma(struct mm_struct * mm, unsigned long addr){
//     kfunc_call(find_vma, mm, addr);
//     kfunc_not_found();
//     return NULL;
// }

// 虚拟地址转物理地址 - 优化版本（使用已获取的 mm_struct）
// 修复：避免每次都调用 get_task_mm/mmput，减少内核开销和潜在的内存泄漏
static uintptr_t _virt_to_phys_with_mm(struct mm_struct *mm, uintptr_t addr) 
{
    uint64_t* pte_ptr;
    pte_t* pte;
    uint64_t pte_value;
    phys_addr_t page_addr;
    uintptr_t page_offset;
    phys_addr_t phys_addr;
    uintptr_t pgd_base;
    
    if (mm_struct_offset.pgd_offset < 0) {
        logv("mm_struct_offset.pgd_offset not initialized\n");
        return 0;
    }
    
    if (!mm || IS_ERR(mm)) {
        logv("Invalid mm_struct\n");
        return 0;
    }
    
    // 获取页表基址
    pgd_base = *(uintptr_t *)((uintptr_t)mm + mm_struct_offset.pgd_offset);
    logv("pgd_base: 0x%lx\n", pgd_base);
    
    // 使用 KPM 提供的安全页表遍历函数
    pte_ptr = pgtable_entry(pgd_base, addr);
    if (!pte_ptr) {
        logv("Address 0x%lx not mapped (pgtable_entry returned NULL)\n", addr);
        return 0;
    }
    
    pte = (pte_t*)pte_ptr;
    pte_value = pte_val(*pte);
    logv("pte_value: 0x%llx\n", pte_value);
    
    // 检查 PTE 是否有效
    if (!pte_value) {
        logv("PTE is zero\n");
        return 0;
    }
    
    // 检查 present bit
    if (!(pte_value & 1)) {
        logv("PTE not present (bit 0 not set)\n");
        return 0;
    }
    
    // 计算物理地址
    page_addr = pte_pfn(*pte) << PAGE_SHIFT;
    page_offset = addr & (PAGE_SIZE - 1);
    phys_addr = page_addr + page_offset;
    
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
    if (compat_copy_to_user(buffer, (const void *)(mapped + offset), size) != 0) {
        logv("Failed to copy to user\n");
        return 0;
    }
    
    logv("Read successful: pa=0x%llx, offset=0x%lx, size=%zu (cached=%s)\n", 
         page_pa, offset, size, cache_idx >= 0 ? "yes" : "no");
    
    return size;
}

// 写入物理地址 - 使用缓存优化（与read_physical_address对应）
static size_t write_physical_address(phys_addr_t pa, const void __user *buffer, size_t size)
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
        logv("Write: Invalid PFN for pa=0x%llx\n", pa);
        return 0;
    }
    
    // 计算页对齐的物理地址
    page_pa = pa & PAGE_MASK;
    offset = pa & (PAGE_SIZE - 1);
    
    // 确保写入不跨页
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
                logv("Write cache hit! idx=%d, pa=0x%llx\n", i, page_pa);
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
                logv("Write cache expired: idx=%d\n", i);
            }
        } else if (empty_idx < 0) {
            empty_idx = i;
        }
    }
    
    // ===== 缓存未命中，创建新映射 =====
    if (!mapped) {
        logv("Write cache miss, creating new mapping for pa=0x%llx\n", page_pa);
        
        // 选择缓存槽：优先空槽，否则替换最旧的
        if (empty_idx >= 0) {
            cache_idx = empty_idx;
        } else {
            cache_idx = oldest_idx;
            // 清理旧映射
            if (mapping_cache[cache_idx].vaddr) {
                __iounmap(mapping_cache[cache_idx].vaddr);
                logv("Write evicting old cache: idx=%d\n", cache_idx);
            }
        }
        
        // 创建新映射（按页大小映射，提高缓存命中率）
        mapped = ioremap_cache(page_pa, PAGE_SIZE);
        if (!mapped) {
            logv("Write failed to ioremap_cache: pa=0x%llx\n", page_pa);
            return 0;
        }
        
        // 保存到缓存
        mapping_cache[cache_idx].pa = page_pa;
        mapping_cache[cache_idx].vaddr = mapped;
        mapping_cache[cache_idx].last_use = now;
        logv("Write new mapping cached: idx=%d, pa=0x%llx\n", cache_idx, page_pa);
    }
    
    // ===== 复制数据（关键区别：从用户空间复制到内核映射的物理内存）=====
    if (__arch_copy_from_user((void *)(mapped + offset), buffer, size) != 0) {
        logv("Failed to copy from user\n");
        return 0;
    }
    
    logv("Write successful: pa=0x%llx, offset=0x%lx, size=%zu (cached=%s)\n", 
         page_pa, offset, size, cache_idx >= 0 ? "yes" : "no");
    
    return size;
}

// 核心读取函数 - 优化版本，只获取一次 mm_struct
// 修复：避免每个页面都调用 get_task_mm/mmput，大幅减少内核开销
static int read_mem(pid_t pid, uintptr_t addr, void __user *buffer, size_t size)
{
    struct task_struct *task;
    struct mm_struct *mm;
    phys_addr_t pa;
    size_t max;
    size_t total_read = 0;
    size_t bytes_read;
    uintptr_t current_addr = addr;
    void __user *current_buffer = buffer;
    size_t remaining = size;
    
    logv("read_mem: pid=%d, addr=0x%lx, size=%zu\n", pid, addr, size);
    
    // 初始化页表配置（只需要一次）
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
    
    // 查找进程（只需要一次）
    task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("No such pid: %d\n", pid);
        return -1;
    }
    
    // 获取内存描述符（只需要一次，重要优化点！）
    mm = get_task_mm(task);
    if (!mm || IS_ERR(mm)) {
        logv("Failed to get mm for pid: %d\n", pid);
        return -1;
    }
    
    // 分页读取，避免跨页问题
    while (remaining > 0) {
        // 计算当前页最多能读取多少字节
        max = PAGE_SIZE - (current_addr & (PAGE_SIZE - 1));
        if (max > remaining) {
            max = remaining;
        }
        
        logv("Reading chunk: addr=0x%lx, size=%zu\n", current_addr, max);
        
        // 虚拟地址转物理地址（使用已获取的 mm）
        pa = _virt_to_phys_with_mm(mm, current_addr);
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
    
    // 释放 mm 引用（只需要一次，重要优化点！）
    mmput(mm);
    
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

// 核心写入函数 - 优化版本，只获取一次 mm_struct
// 修复：避免每个页面都调用 get_task_mm/mmput，大幅减少内核开销
static int write_mem(pid_t pid, uintptr_t addr, const void __user *buffer, size_t size)
{
    struct task_struct *task;
    struct mm_struct *mm;
    phys_addr_t pa;
    size_t max;
    size_t total_written = 0;
    size_t bytes_written;
    uintptr_t current_addr = addr;
    const void __user *current_buffer = buffer;
    size_t remaining = size;
    
    logv("write_mem: pid=%d, addr=0x%lx, size=%zu\n", pid, addr, size);
    
    // 初始化页表配置（只需要一次）
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
    
    // 查找进程（只需要一次）
    task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("No such pid: %d\n", pid);
        return -1;
    }
    
    // 获取内存描述符（只需要一次，重要优化点！）
    mm = get_task_mm(task);
    if (!mm || IS_ERR(mm)) {
        logv("Failed to get mm for pid: %d\n", pid);
        return -1;
    }
    
    // 分页写入，避免跨页问题
    while (remaining > 0) {
        // 计算当前页最多能写入多少字节
        max = PAGE_SIZE - (current_addr & (PAGE_SIZE - 1));
        if (max > remaining) {
            max = remaining;
        }
        
        logv("Writing chunk: addr=0x%lx, size=%zu\n", current_addr, max);
        
        // 虚拟地址转物理地址（使用已获取的 mm）
        pa = _virt_to_phys_with_mm(mm, current_addr);
        if (!pa) {
            logv("Virtual to physical translation failed for addr=0x%lx\n", current_addr);
            // 跳过这一部分，继续下一页
            goto next_chunk;
        }
        
        // 写入这一页
        bytes_written = write_physical_address(pa, current_buffer, max);
        if (bytes_written > 0) {
            total_written += bytes_written;
        } else {
            logv("Failed to write physical address 0x%llx\n", pa);
        }
        
    next_chunk:
        remaining -= max;
        current_addr += max;
        current_buffer += max;
    }
    
    // 释放 mm 引用（只需要一次，重要优化点！）
    mmput(mm);
    
    if (total_written == size) {
        logv("Successfully wrote all %zu bytes\n", size);
        return 0;
    } else if (total_written > 0) {
        logv("Partially wrote %zu/%zu bytes\n", total_written, size);
        return 0; // 返回成功，即使只写入部分
    } else {
        logv("Failed to write any data %zu/%zu\n", total_written, size);
        return -1;
    }
}

// 根据进程名获取进程PID
static pid_t get_process_pid(const char *name)
{
    pid_t ipid = -1;
    struct task_struct *task = NULL;
    int pid;
    
    if (!name) {
        logv("get_process_pid: name is NULL\n");
        return -1;
    }
    
    logv("get_process_pid: searching for process '%s'\n", name);
    
    // 遍历所有可能的 PID (0-32767)
    for (pid = 0; pid < 32768; pid++) {
        task = my_find_task_by_vpid(pid);
        if (!task) {
            continue;
        }
        
        // 获取进程名（comm字段）
        const char *comm = get_task_comm(task);
        if (!comm) {
            continue;
        }
        
        // 检查进程名是否匹配
        if (strstr(comm, name)) {
            // 尝试使用动态查找的函数获取真实的 PID
            ipid = my_task_pid_nr_ns(task, PIDTYPE_PID, NULL);
            
            // 如果动态查找的函数失败（返回0），则直接使用 pid
            if (ipid == 0) {
                ipid = pid;
                logv("get_process_pid: using fallback pid method\n");
            }
            
            logv("get_process_pid: found process '%s' with PID %d\n", comm, ipid);
            break;
        }
    }
    
    if (ipid < 0) {
        logv("get_process_pid: process '%s' not found\n", name);
    }
    
    return ipid;
}

// 获取进程中指定模块的基址（简化安全版本）
// 注意：由于内核结构体偏移不确定，VMA 遍历部分已注释，暂时只返回 0
static uintptr_t get_module_base(pid_t pid, const char* module_name)
{
    struct task_struct* task;
    struct mm_struct* mm;
    uintptr_t base_addr = 0;

    if (!module_name) {
        logv("get_module_base: module_name is NULL\n");
        return 0;
    }

    logv("get_module_base: pid=%d, module_name='%s'\n", pid, module_name);

    // 查找进程
    task = my_find_task_by_vpid(pid);
    if (!task) {
        logv("get_module_base: no such pid: %d\n", pid);
        return 0;
    }

    // 获取内存描述符
    mm = get_task_mm(task);
    if (!mm) {
        logv("get_module_base: failed to get mm\n");
        return 0;
    }
    
    logv("get_module_base: got mm=%px\n", mm);

    // ==================== 遍历 VMA 部分已注释（避免系统崩溃）====================
    // 原因：vm_file 偏移未知，d_path 可能导致死锁
    // 建议：用户空间通过读取 /proc/pid/maps 获取模块基址
    
    // TODO: 如果需要内核实现，需要：
    // 1. 确定正确的 vm_file 在 vm_area_struct 中的偏移
    // 2. 使用更安全的方式获取文件路径（不调用 d_path）
    // 3. 或者直接解析 /proc/pid/maps 文件（需要 VFS 函数支持）
    
    mmput(mm);
    
    logv("get_module_base: NOT FULLY IMPLEMENTED - returning 0\n");
    logv("get_module_base: please use userspace /proc/%d/maps parsing\n", pid);
    
    return base_addr;  // 暂时返回 0
}

// 允许的 UID（仅 root）
#define ALLOWED_UID  0  // root

// prctl syscall signature:
// long prctl(int option, unsigned long arg2, unsigned long arg3,
//            unsigned long arg4, unsigned long arg5);
void before_prctl(hook_fargs5_t *args, void *udata)
{
    int option = (int)syscall_argn(args, 0);
    unsigned long arg2 = (unsigned long)syscall_argn(args, 1);
    struct mem_operation op;
    struct process_pid pp;
    struct module_base mb;
    int result;
    uid_t caller_uid;
    
    // ===== 快速过滤：非目标命令直接放行 =====
    if (option != PRCTL_MEM_READ && option != PRCTL_MEM_WRITE && 
        option != PRCTL_PROCESS_PID && option != PRCTL_MODULE_BASE) {
        return;  // 不处理，让系统正常执行原 prctl
    }
    
    logv("prctl detected: option=0x%x, arg2=0x%lx\n", option, arg2);
    
    // ===== UID 验证：只允许 root 调用 =====
    // 获取当前调用进程的 UID（使用 KPM 偏移量方式）
    struct cred *cred = *(struct cred **)((uintptr_t)current + task_struct_offset.cred_offset);
    caller_uid = *(uid_t *)((uintptr_t)cred + cred_offset.uid_offset);
    
    // 检查是否为 root
    if (caller_uid != ALLOWED_UID) {
        logv("prctl: access denied, caller_uid=%d (need root)\n", caller_uid);
        args->ret = -EPERM;  // 权限拒绝
        return;
    }
    
    logv("prctl: UID check passed (uid=%d)\n", caller_uid);
    
    // ===== 执行对应操作 =====
    if (option == PRCTL_MEM_READ || option == PRCTL_MEM_WRITE) {
        // ===== 从用户空间复制内存操作结构体 =====
        if (__arch_copy_from_user(&op, (void __user *)arg2, sizeof(op)) != 0) {
            logv("prctl: failed to copy mem_operation from user\n");
            args->ret = -EFAULT;
            return;
        }
        
        logv("prctl %s - target_pid=%d, addr=0x%llx, size=%llu\n",
             option == PRCTL_MEM_READ ? "READ" : "WRITE",
             op.target_pid, op.addr, op.size);
        
        if (option == PRCTL_MEM_READ) {
            // 读取内存
            result = read_mem(op.target_pid, op.addr, op.buffer, op.size);
            
            if (result == 0) {
                logv("prctl read operation successful\n");
                args->ret = 0;  // 成功
            } else {
                logv("prctl read operation failed: %d\n", result);
                args->ret = -EIO;  // 失败
            }
        } 
        else if (option == PRCTL_MEM_WRITE) {
            // 写入内存
            result = write_mem(op.target_pid, op.addr, op.buffer, op.size);
            
            if (result == 0) {
                logv("prctl write operation successful\n");
                args->ret = 0;  // 成功
            } else {
                logv("prctl write operation failed: %d\n", result);
                args->ret = -EIO;  // 失败
            }
        }
    }
    else if (option == PRCTL_PROCESS_PID) {
        // ===== 获取进程PID =====
        char taskname[256] = {0};
        pid_t found_pid;
        
        // 从用户空间复制 process_pid 结构体
        if (__arch_copy_from_user(&pp, (void __user *)arg2, sizeof(pp)) != 0) {
            logv("prctl: failed to copy process_pid from user\n");
            args->ret = -EFAULT;
            return;
        }
        
        // 从用户空间复制进程名字符串
        if (__arch_copy_from_user(taskname, pp.taskname, sizeof(taskname) - 1) != 0) {
            logv("prctl: failed to copy taskname from user\n");
            args->ret = -EFAULT;
            return;
        }
        
        logv("prctl GET_PID - taskname='%s'\n", taskname);
        
        // 调用 get_process_pid 查找进程
        found_pid = get_process_pid(taskname);
        
        if (found_pid > 0) {
            // 找到进程，回写 PID 到用户空间
            pp.pid = found_pid;
            compat_copy_to_user((void __user *)arg2, &pp, sizeof(pp));
            
            logv("prctl get_pid successful: '%s' -> PID %d\n", taskname, found_pid);
            args->ret = 0;  // 成功
        } else {
            logv("prctl get_pid failed: process '%s' not found\n", taskname);
            args->ret = -ESRCH;  // 进程不存在
        }
    }
    else if (option == PRCTL_MODULE_BASE) {
        // ===== 获取模块基址 =====
        char module_name[256] = {0};
        uintptr_t base_addr;
        
        // 从用户空间复制 module_base 结构体
        if (__arch_copy_from_user(&mb, (void __user *)arg2, sizeof(mb)) != 0) {
            logv("prctl: failed to copy module_base from user\n");
            args->ret = -EFAULT;
            return;
        }
        
        // 从用户空间复制模块名字符串
        if (__arch_copy_from_user(module_name, mb.module_name, sizeof(module_name) - 1) != 0) {
            logv("prctl: failed to copy module_name from user\n");
            args->ret = -EFAULT;
            return;
        }
        
        logv("prctl MODULE_BASE - pid=%d, module_name='%s'\n", mb.pid, module_name);
        
        // 调用 get_module_base 查找模块基址
        base_addr = get_module_base(mb.pid, module_name);
        
        if (base_addr > 0) {
            // 找到模块，回写基址到用户空间
            mb.base_address = base_addr;
            if (compat_copy_to_user((void __user *)arg2, &mb, sizeof(mb)) != 0) {
                logv("prctl: failed to write back module_base\n");
                args->ret = -EFAULT;
                return;
            }
            
            logv("prctl get_module_base successful: '%s' in pid %d -> 0x%lx\n", 
                 module_name, mb.pid, base_addr);
            args->ret = 0;  // 成功
        } else {
            logv("prctl get_module_base failed: module '%s' not found in pid %d\n", 
                 module_name, mb.pid);
            args->ret = -ENOENT;  // 模块不存在
        }
    }
}
 
static long syscall_hook_demo_init(const char *args, const char *event, void *__user reserved)
{
    logv("prctl hook module init, args: %s\n", args);
    logv("Platform: ARM64/aarch64\n");
    logv("Syscall number __NR_prctl: %d\n", __NR_prctl);
    logv("PRCTL_MEM_READ: 0x%x\n", PRCTL_MEM_READ);
    logv("PRCTL_MEM_WRITE: 0x%x\n", PRCTL_MEM_WRITE);
    logv("PRCTL_PROCESS_PID: 0x%x\n", PRCTL_PROCESS_PID);
    logv("PRCTL_MODULE_BASE: 0x%x\n", PRCTL_MODULE_BASE);

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
    
    // 尝试查找 __task_pid_nr_ns（可能失败，有降级方案）
    // 注意：这个函数在某些内核中未导出，如果查找失败会自动使用备用方案
    kfunc_lookup_name(__task_pid_nr_ns);
    
    // find_vma 和 d_path 暂时不使用（会导致系统崩溃）
    // kfunc_lookup_name(find_vma);
    // kfunc_lookup_name(d_path);
    logv("All kernel functions resolved\n");

    hook_err_t err = HOOK_NO_ERR;

    logv("hooking prctl syscall (nr=%d)...\n", __NR_prctl);
    err = fp_hook_syscalln(__NR_prctl, 5, before_prctl, 0, 0);
    
    if (err) {
        logv("hook prctl error: %d\n", err);
    } else {
        logv("hook prctl success\n");
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
     logv("prctl hook module exit\n");

     // 清理映射缓存（重要：避免内存泄漏）
     cleanup_mapping_cache();

     fp_unhook_syscalln(__NR_prctl, before_prctl, 0);
     return 0;
 }
 
 KPM_INIT(syscall_hook_demo_init);
 KPM_CTL0(syscall_hook_control0);
 KPM_EXIT(syscall_hook_demo_exit);