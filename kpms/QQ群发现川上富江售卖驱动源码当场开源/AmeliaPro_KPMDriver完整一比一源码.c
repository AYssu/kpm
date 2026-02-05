/*QQ号泪心唯一号：2254013571
 * 须知：
开源源码完整可编译 1比1完整破解AmeliaPro_KPMDriver.kpm驱动源码
也不叫破解 本来就是我的源码 我自己拿自己的kpm源码没毛病吧

具体是个尚在上学的初中生  估计是被日漫洗脑了一个网名川上富江的小丑 开始卖上了 

还独家驱动源码 硬件断点  陀螺仪内核触摸  safe硬件级纯汇编读写 
确实什么牛气话术都吹天了  结果到头来是完整套壳github完整开源源码

该源码本身就属于我之前早期改写kpm驱动源码生产的github开源源码  这个倒是猎奇 直接开卖了

开卖也不能当着别人开源作者的脸上叫价  这不妥妥关公面前耍大刀

实力没有 全靠吹牛

完全一比一按照github kpm完整源码写的   其他全靠自己的话术+炒作

copy_from_user  copy_to_user access_process_vm 
解析内核符合  探嗅指针  
你是一点不带改 完全照搬  一秒钟不到 就开始当面吆喝了
“走过路过不要错过” ？



泪心开源kpm完整源码链接 https://github.com/AYssu/kpm/tree/TearGame/kpms/TearGame-prctlhookRWMemory
致谢fscan 阿夜手游外挂开发领航作者之一
fscanqq群903485530

-- 泪心只是退网了  不代表你可以当着我的面蹦跶  请不要联系泪心
	我只是退网了 不是看不到消息  你要卖也别当着我的面  问我1000要不要源码  
	你自己想想 这对吗？  要不要点脑子?
 */

#include <compiler.h>
#include <kpmodule.h>
#include <ksyms.h>
#include <syscall.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <kputils.h>
#include <ktypes.h>

/* 页大小与缓冲区 */
#define PAGE_SIZE_4K     4096
#define KERNEL_BUF_SIZE  PAGE_SIZE_4K
static uint8_t kernel_buffer[KERNEL_BUF_SIZE] __attribute__((aligned(8)));

/* 物理/硬件内存是否可用（需在模块初始化时根据 kfunc 是否解析成功设置） */
int g_phys_mem_available = 0;

/* task_struct->comm 偏移，需在模块初始化时设置（如 probe_comm_offset / offsetof） */
unsigned long g_comm_offset = 0;

#define PID_SCAN_START  1000
#define PID_SCAN_END    32769   /* 遍历 pid 1000..32768 */
#define COMM_CMP_MAX    15      /* 最多比较包名前 15 字节（与 comm 对齐） */

#define COMM_PROBE_START  1024  /* 在 task 上扫描 comm 的偏移范围 */
#define COMM_PROBE_END    3072
#define COMM_OFFSET_FALLBACK  2320

#define PRCTL_CMD_READ_MEM        0x4D454D31
#define PRCTL_CMD_WRITE_MEM       0x4D454D32
#define PRCTL_CMD_GET_PID         0x4D454D33
#define PRCTL_CMD_READ_MEM_SAFE   0x4D454D34
#define PRCTL_CMD_WRITE_MEM_SAFE 0x4D454D35

struct prctl_mem_req {
	uint32_t pid;
	uint32_t pad;
	uint64_t addr;
	void __user *buffer;
	uint64_t size;
};

/* 按包名查 PID 请求：280 字节，name 在前，name_len@256，result_pid@260 */
#define PRCTL_PID_NAME_LEN  256
#define PRCTL_PID_REQ_SIZE  280

/* kfunc 声明 */
struct task_struct *kfunc_def(find_task_by_vpid)(pid_t pid);

long kfunc_def(access_process_vm)(struct task_struct *tsk, unsigned long addr,
                                  void *buf, size_t len, int write);

/* 用户态 -> 内核缓冲，返回 0 成功，非 0 失败 */
int kfunc_def(__arch_copy_from_user)(void *to, const void __user *from, int n);

/* ARM64 硬件断点用 kfunc（set_real_arm64_hardware_breakpoint 等） */
struct pid *kfunc_def(find_get_pid)(pid_t nr);
struct task_struct *kfunc_def(get_pid_task)(struct pid *pid, int type);
void kfunc_def(put_pid)(struct pid *pid);
void kfunc_def(__raw_spin_lock)(void *lock);
void kfunc_def(__raw_spin_unlock)(void *lock);

/*
 * 按包名查 PID：遍历 pid 1000..32768，用 task->comm 与包名比较（最多 15 字节）
 * a1 = 包名字符串, a2 = 长度；comm[0] == ':' 的进程跳过
 * 返回: pid 或 (unsigned int)-1
 */
int get_pid_by_package(const unsigned char *package_name, size_t name_len)
{
	unsigned int pid;
	size_t cmp_len;
	struct task_struct *task;
	const unsigned char *comm;
	size_t n;

	if (!name_len || !g_comm_offset)
		return -1;

	cmp_len = name_len >= COMM_CMP_MAX ? COMM_CMP_MAX : name_len;

	for (pid = PID_SCAN_START; pid != PID_SCAN_END; pid++) {
		if (!kfunc(find_task_by_vpid)) {
			printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
			return -1;
		}

		task = kfunc(find_task_by_vpid)((pid_t)pid);
		if (!task)
			continue;

		comm = (const unsigned char *)((unsigned long)task + g_comm_offset);
		if (*comm == 58)   /* ':' 开头则跳过 */
			continue;

		{
			const unsigned char *pkg = package_name;
			for (n = cmp_len; n != 0; n--) {
				if (*comm++ != *pkg++)
					break;
			}
			if (n == 0)
				return (int)pid;
		}
	}

	return -1;
}

/*
 * 按优先级尝试: sensorservice -> android.hardware.sensors@ -> system_server
 * comm[0] == ':' 的进程跳过；最多比较 15 字节
 * 返回: pid 或 -1
 */
static int find_gyro_service_process(void)
{
	static const char *names[] = { "sensorservice", "android.hardware.sensors@", "system_server" };
	unsigned int pid;
	size_t cmp_len, name_len, n;
	struct task_struct *task;
	const unsigned char *comm;
	int i;

	if (!g_comm_offset)
		return -1;

	for (i = 0; i < 3; i++) {
		name_len = strlen(names[i]);
		if (!name_len)
			continue;
		cmp_len = name_len >= COMM_CMP_MAX ? COMM_CMP_MAX : name_len;

		for (pid = PID_SCAN_START; pid != PID_SCAN_END; pid++) {
			if (!kfunc(find_task_by_vpid)) {
				printk("[-] KP E kfunc: %s not found\n", "find_task_by_vpid");
				return -1;
			}
			task = kfunc(find_task_by_vpid)((pid_t)pid);
			if (!task)
				continue;
			comm = (const unsigned char *)((unsigned long)task + g_comm_offset);
			if (*comm == 58)
				continue;
			{
				const unsigned char *pkg = (const unsigned char *)names[i];
				for (n = cmp_len; n != 0; n--) {
					if (*comm++ != *pkg++)
						break;
				}
				if (n == 0)
					return (int)pid;
			}
		}
	}
	return -1;
}

/*
 * 普通读内存：不检查 g_phys_mem_available，每轮块长 = min(4096, remaining)
 * a1 = pid, a2 = 源地址, a3 = 用户态目标缓冲区, a4 = 要读取的字节数
 * 返回: 0 成功, -1 失败
 */
long read_mem(pid_t pid, uintptr_t src_addr, void __user *user_dst, size_t size)
{
	struct task_struct *task;
	size_t total_read = 0;
	uintptr_t cur_addr = src_addr;
	void __user *cur_dst = user_dst;
	size_t remaining = size;

	if (!kfunc(find_task_by_vpid)) {
		printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
		return -1;
	}

	task = kfunc(find_task_by_vpid)(pid);
	if (!task)
		return -1;

	while (remaining != 0) {
		size_t chunk = remaining >= PAGE_SIZE_4K ? PAGE_SIZE_4K : remaining;

		if (!kfunc(access_process_vm)) {
			printk("[-] KP E kfunc: %s not found\n", "access_process_vm_safe");
			if (total_read == 0)
				return -1;
			return 0;
		}

		{
			long n = kfunc(access_process_vm)(task, cur_addr, kernel_buffer, chunk, 0);
			if (n < 1) {
				if (total_read == 0)
					return -1;
				return 0;
			}
			if ((unsigned int)compat_copy_to_user(cur_dst, kernel_buffer, (unsigned int)n) != (unsigned int)n) {
				if (total_read == 0)
					return -1;
				return 0;
			}
			total_read += (size_t)n;
			cur_addr += (uintptr_t)n;
			cur_dst += n;
			remaining -= (size_t)n;
			if ((size_t)n < chunk)
				break;
		}
	}

	if (total_read != 0)
		return 0;
	return -1;
}

/*
 * 普通写内存：不检查 g_phys_mem_available，每轮块长 = min(4096, remaining)
 * a1 = pid, a2 = 目标地址, a3 = 用户态源缓冲区, a4 = 要写入的字节数
 * 返回: 0 成功, -1 失败
 */
long write_mem(pid_t pid, uintptr_t dst_addr, const void __user *user_src, size_t size)
{
	struct task_struct *task;
	size_t total_written = 0;
	uintptr_t cur_addr = dst_addr;
	const void __user *cur_src = user_src;
	size_t remaining = size;

	if (!kfunc(find_task_by_vpid)) {
		printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
		return -1;
	}

	task = kfunc(find_task_by_vpid)(pid);
	if (!task || !size)
		return -1;

	do {
		size_t chunk = remaining >= PAGE_SIZE_4K ? PAGE_SIZE_4K : remaining;

		if (!kfunc(__arch_copy_from_user)) {
			printk("[-] KP E kfunc: %s not found\n", "__arch_copy_from_user");
			if (total_written == 0)
				return -1;
			return 0;
		}
		if (kfunc(__arch_copy_from_user)(kernel_buffer, cur_src, (int)chunk) != 0) {
			if (total_written == 0)
				return -1;
			return 0;
		}

		if (!kfunc(access_process_vm)) {
			printk("[-] KP E kfunc: %s not found\n", "access_process_vm_safe");
			if (total_written == 0)
				return -1;
			return 0;
		}

		{
			long n = kfunc(access_process_vm)(task, cur_addr, kernel_buffer, chunk, 1);
			if (n < 1) {
				if (total_written == 0)
					return -1;
				return 0;
			}
			total_written += (size_t)n;
			if ((size_t)n < chunk)
				break;
			remaining -= (size_t)n;
			cur_addr += (uintptr_t)n;
			cur_src += n;
		}
	} while (remaining);

	if (total_written != 0)
		return 0;
	return -1;
}

/*
 * 硬件安全读内存：通过 find_task_by_vpid + access_process_vm 按页读入内核缓冲再 copy_to_user
 * a1 = pid, a2 = 源地址, a3 = 用户态目标缓冲区, a4 = 要读取的字节数
 * 返回: 0 成功, -1 失败
 */
long read_mem_hardware_safe(pid_t pid, uintptr_t src_addr, void __user *user_dst, size_t size)
{
	struct task_struct *task;
	size_t total_read = 0;
	uintptr_t cur_addr = src_addr;
	void __user *cur_dst = user_dst;
	size_t remaining = size;

	if (!g_phys_mem_available)
		return -1;

	if (!kfunc(find_task_by_vpid)) {
		printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
		return -1;
	}

	task = kfunc(find_task_by_vpid)(pid);
	if (!task)
		return -1;

	while (remaining != 0) {
		size_t page_off = cur_addr & 0xFFF;
		size_t chunk = (PAGE_SIZE_4K - page_off) >= remaining
				  ? remaining
				  : (size_t)(PAGE_SIZE_4K - (cur_addr & 0xFFF));

		if (!kfunc(access_process_vm)) {
			printk("[-] KP E kfunc: %s not found\n", "access_process_vm_safe");
			if (total_read == 0)
				return -1;
			return 0;
		}

		{
			long n = kfunc(access_process_vm)(task, cur_addr, kernel_buffer, chunk, 0);
			if (n < 1) {
				if (total_read == 0)
					return -1;
				return 0;
			}
			if ((unsigned int)compat_copy_to_user(cur_dst, kernel_buffer, (unsigned int)n) != (unsigned int)n) {
				if (total_read == 0)
					return -1;
				return 0;
			}
			total_read += (size_t)n;
			cur_addr += (uintptr_t)n;
			cur_dst += n;
			remaining -= (size_t)n;
			if ((size_t)n < chunk)
				break;
		}
	}

	if (total_read != 0)
		return 0;
	return -1;
}

/*
 * 硬件安全写内存：用户态数据 __arch_copy_from_user 到内核缓冲，再 access_process_vm(..., 1) 写入目标进程
 * a1 = pid, a2 = 目标进程虚拟地址, a3 = 用户态源缓冲区, a4 = 要写入的字节数
 * 返回: 0 成功, -1 失败
 */
long write_mem_hardware_safe(pid_t pid, uintptr_t dst_addr, const void __user *user_src, size_t size)
{
	struct task_struct *task;
	size_t total_written = 0;
	uintptr_t cur_addr = dst_addr;
	const void __user *cur_src = user_src;
	size_t remaining = size;

	if (!g_phys_mem_available)
		return -1;

	if (!kfunc(find_task_by_vpid)) {
		printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
		return -1;
	}

	task = kfunc(find_task_by_vpid)(pid);
	if (!task)
		return -1;

	while (remaining != 0) {
		size_t page_off = cur_addr & 0xFFF;
		size_t chunk = (PAGE_SIZE_4K - page_off) >= remaining
				  ? remaining
				  : (size_t)(PAGE_SIZE_4K - (cur_addr & 0xFFF));

		if (!kfunc(__arch_copy_from_user)) {
			printk("[-] KP E kfunc: %s not found\n", "__arch_copy_from_user");
			if (total_written == 0)
				return -1;
			return 0;
		}
		if (kfunc(__arch_copy_from_user)(kernel_buffer, cur_src, (int)chunk) != 0) {
			if (total_written == 0)
				return -1;
			return 0;
		}

		if (!kfunc(access_process_vm)) {
			printk("[-] KP E kfunc: %s not found\n", "access_process_vm_safe");
			if (total_written == 0)
				return -1;
			return 0;
		}

		{
			long n = kfunc(access_process_vm)(task, cur_addr, kernel_buffer, chunk, 1);
			if (n < 1) {
				if (total_written == 0)
					return -1;
				return 0;
			}
			total_written += (size_t)n;
			cur_addr += (uintptr_t)n;
			cur_src += n;
			remaining -= (size_t)n;
			if ((size_t)n < chunk)
				break;
		}
	}

	if (total_written != 0)
		return 0;
	return -1;
}

/* ==================== 触摸注入（ inject_touch_down + inject_touch_event_to_system） ==================== */
#define TOUCH_SLOTS_NUM    10
#define TOUCH_SLOT_STRIDE  4   /* 每槽 4 个 uint64_t */
#define TOUCH_TRACKING_MIN 1000
#define TOUCH_TRACKING_MAX 10999

#define TOUCH_EVENT_BUF_SIZE  48
#define TOUCH_SHM_BASE        0x70000000ULL
#define TOUCH_MAGIC_HEAD      1414485315U  /* 0x544F5543 "TOUC" */
#define TOUCH_MAGIC_TAIL      1414678855U  /* 0x544F5547 "TOUG" */

static uint64_t touch_slots[TOUCH_SLOTS_NUM * TOUCH_SLOT_STRIDE];
static int next_tracking_id = TOUCH_TRACKING_MIN;
static uint64_t get_current_timestamp_counter;

/* 屏幕尺寸，需在 init 或 prctl 中设置（如 /proc/cmdline video= 或 display_service） */
int screen_width;
int screen_height;

/* 系统服务进程 PID（display_server 等）；触摸注入写入该进程 0x70000000 共享区，需在 init/prctl 中设置 */
int display_service_pid = -1;

/*
 * 获取系统服务进程 PID；未实现时返回 display_service_pid（需 init 里先设）
 */
static int find_system_service_process(void)
{
	return display_service_pid;
}

/*
 * 将触摸事件注入系统：向 display_service 进程 0x70000000 写入协议块
 * 协议: magic_head(4) | size(4) | event(48) | checksum(4) | magic_tail(4)
 * event 为 48 字节；传入的 event 为 6 个 int（24 字节），前 24 字节拷贝，后 24 字节填 0
 * 返回: 0 成功，-1 失败
 */
static int inject_touch_event_to_system(int *event)
{
	uint8_t buf[TOUCH_EVENT_BUF_SIZE];
	uint32_t checksum;
	size_t i;
	int pid;
	struct task_struct *task;
	uint32_t magic_head = TOUCH_MAGIC_HEAD;
	uint32_t size_val = TOUCH_EVENT_BUF_SIZE;
	uint32_t magic_tail = TOUCH_MAGIC_TAIL;
	long n;

	pid = find_system_service_process();
	if (pid < 1)
		return -1;

	if (!kfunc(find_task_by_vpid)) {
		printk("[-] KP E kfunc: %s not found\n", "my_find_task_by_vpid");
		return -1;
	}
	task = kfunc(find_task_by_vpid)((pid_t)pid);
	if (!task)
		return -1;

	/* 拷贝 event 前 24 字节到 48 字节缓冲，余量填 0 */
	for (i = 0; i < sizeof(buf); i++)
		buf[i] = (i < 6 * sizeof(int)) ? ((uint8_t *)event)[i] : 0;

	/* 校验和：48 字节逐字节相加 */
	checksum = 0;
	for (i = 0; i < sizeof(buf); i++)
		checksum += buf[i];

	if (!kfunc(access_process_vm))
		goto err_print;
	n = kfunc(access_process_vm)(task, TOUCH_SHM_BASE, &magic_head, 4, 1);
	if (n < 1) return -1;
	n = kfunc(access_process_vm)(task, TOUCH_SHM_BASE + 4, &size_val, 4, 1);
	if (n < 1) return -1;
	n = kfunc(access_process_vm)(task, TOUCH_SHM_BASE + 8, buf, TOUCH_EVENT_BUF_SIZE, 1);
	if (n < 1) return -1;
	n = kfunc(access_process_vm)(task, TOUCH_SHM_BASE + 0x38, &checksum, 4, 1);
	if (n < 1) return -1;
	n = kfunc(access_process_vm)(task, TOUCH_SHM_BASE + 0x3C, &magic_tail, 4, 1);
	if (n < 1) return -1;
	return 0;

err_print:
	printk("[-] KP E kfunc: %s not found\n", "access_process_vm_safe");
	return -1;
}

/*
 * 按下触摸：取一空闲槽，写入 x,y 并调用 inject_touch_event_to_system
 * 返回: 槽号 0..9 成功，-1 失败（坐标非法或无空闲槽或注入失败）
 */
static int inject_touch_down(int x, int y)
{
	unsigned int slot;
	uint64_t *v3;
	int tracking_id;
	int event[6];

	if ((x & 0x80000000) != 0 || (y & 0x80000000) != 0)
		return -1;
	if (x >= screen_width || y >= screen_height)
		return -1;

	slot = 0;
	for (;;) {
		if (slot >= TOUCH_SLOTS_NUM)
			return -1;
		if ((uint32_t)touch_slots[slot * TOUCH_SLOT_STRIDE] == 0)
			break;
		slot++;
	}

	v3 = &touch_slots[slot * TOUCH_SLOT_STRIDE];
	tracking_id = next_tracking_id;

	((uint32_t *)v3)[0] = 1;
	((uint32_t *)v3)[1] = tracking_id;
	((uint32_t *)v3)[3] = x;
	((uint32_t *)v3)[4] = y;
	((uint32_t *)v3)[5] = 50;

	next_tracking_id++;
	if (next_tracking_id > TOUCH_TRACKING_MAX)
		next_tracking_id = TOUCH_TRACKING_MIN;

	event[0] = 0;
	event[1] = (int)slot;
	event[2] = tracking_id;
	event[3] = x;
	event[4] = y;
	event[5] = 50;

	get_current_timestamp_counter++;

	if (inject_touch_event_to_system(event) != 0) {
		*v3 = 0xFFFFFFFF00000000ULL;
		return -1;
	}
	return (int)slot;
}

/* ==================== ARM64 硬件断点（set_real_arm64_hardware_breakpoint） ==================== */
#define ARM64_HWBP_STATE_SIZE  128
#define ARM64_HWBP_MAX_INDEX   15
#define ARM64_DBGVCR_BASE      0x80000000ULL
#define ARM64_DBGBCR_OFF       0x800
#define ARM64_DBGWCR_BASE      0x80010000ULL
#define ARM64_DBGWCR_OFF       0x800
#define ARM64_MDSCR_EL1_ADDR   0x80003000ULL
#define ARM64_MDSCR_EL1_MDE    (1ULL << 15)
#define ARM64_MDSCR_EL1_MDA    (1ULL << 14)

static int arm64_hwbp_state[ARM64_HWBP_STATE_SIZE];
static int hwbp_lock;
static uintptr_t qword_32B8;

static void init_arm64_hardware_breakpoint_system(void)
{
	arm64_hwbp_state[0] = 1;
}

static inline void arm64_dsb_isb(void)
{
	__asm__ __volatile__("dsb sy" ::: "memory");
	__asm__ __volatile__("isb" ::: "memory");
}

static long set_real_arm64_hardware_breakpoint(uint32_t *a1)
{
	uint32_t idx = a1[7];
	uint64_t addr = *(uint64_t *)(a1 + 2);
	uint32_t len_bits = a1[5];
	uint32_t type = a1[4];
	uint32_t ctrl_enc = a1[6];
	int *v14;
	int *v13;
	long ret = -22;

	if (!arm64_hwbp_state[0])
		init_arm64_hardware_breakpoint_system();

	if (idx > ARM64_HWBP_MAX_INDEX)
		return -22;
	if (len_bits > 63 || (addr & ((1ULL << len_bits) - 1)) != 0)
		return -22;

	if (kfunc(__raw_spin_lock))
		kfunc(__raw_spin_lock)(&hwbp_lock);

	if (arm64_hwbp_state[idx + 1]) {
		ret = -16;
		goto out_unlock;
	}

	if (kfunc(find_get_pid)) {
		struct pid *pid = kfunc(find_get_pid)((pid_t)a1[0]);
		if (pid && kfunc(get_pid_task)) {
			if (kfunc(get_pid_task)(pid, 0) && kfunc(put_pid))
				kfunc(put_pid)(pid);
		}
	}

	if (qword_32B8) {
		uint64_t ctrl = (2ULL * ctrl_enc) | (32ULL * len_bits) | ((uint64_t)type << 20) | 1;
		if (type == 1) {
			*(volatile uint64_t *)(ARM64_DBGVCR_BASE + (uintptr_t)(8 * idx)) = addr;
			arm64_dsb_isb();
			*(volatile uint64_t *)(ARM64_DBGVCR_BASE + ARM64_DBGBCR_OFF + (uintptr_t)(8 * idx)) = ctrl;
			arm64_dsb_isb();
		} else if (type == 2 || type == 3) {
			*(volatile uint64_t *)(ARM64_DBGWCR_BASE + (uintptr_t)(8 * idx)) = addr;
			arm64_dsb_isb();
			*(volatile uint64_t *)(ARM64_DBGWCR_BASE + ARM64_DBGWCR_OFF + (uintptr_t)(8 * idx)) = ctrl;
			arm64_dsb_isb();
		} else {
			ret = -22;
			goto out_unlock;
		}
		{
			uint64_t v = *(volatile uint64_t *)ARM64_MDSCR_EL1_ADDR;
			arm64_dsb_isb();
			*(volatile uint64_t *)ARM64_MDSCR_EL1_ADDR = v | ARM64_MDSCR_EL1_MDE | ARM64_MDSCR_EL1_MDA;
			arm64_dsb_isb();
		}
	}

	v14 = &arm64_hwbp_state[idx];
	v13 = &arm64_hwbp_state[2 * idx];
	v14[1] = 1;
	*(uint64_t *)(v13 + 17) = addr;
	v14[17] = a1[0];
	v14[66] = a1[4];
	v14[82] = a1[5];
	v14[98] = a1[6];
	ret = 0;

out_unlock:
	if (kfunc(__raw_spin_unlock))
		kfunc(__raw_spin_unlock)(&hwbp_lock);
	return ret;
}

/*
 * 移除 ARM64 硬件断点；a1 为请求，*(uint32_t*)(a1+28) = 槽位索引
 * 返回: 0 成功, -ENOENT(-2) 槽未使用, -EINVAL(-22) 索引非法
 */
static long remove_real_arm64_hardware_breakpoint(uint32_t *a1)
{
	uint32_t idx = *(uint32_t *)((char *)a1 + 28);
	int *v6;
	long ret = -22;

	if (idx > ARM64_HWBP_MAX_INDEX)
		return -22;

	if (kfunc(__raw_spin_lock))
		kfunc(__raw_spin_lock)(&hwbp_lock);

	if (!arm64_hwbp_state[idx + 1]) {
		ret = -2; /* -ENOENT */
		goto out_unlock;
	}

	if (qword_32B8) {
		uintptr_t v4 = (uintptr_t)(8 * idx);
		if (idx > ARM64_HWBP_MAX_INDEX) {
			ret = -22;
			goto out_unlock;
		}
		*(volatile uint64_t *)(ARM64_DBGVCR_BASE + v4) = 0;
		arm64_dsb_isb();
		*(volatile uint64_t *)(ARM64_DBGVCR_BASE + ARM64_DBGBCR_OFF + v4) = 0;
		arm64_dsb_isb();
		/* v4 = (8*idx - 2147481600) & 0xFFFFFFFF => (v4+2048)=0x80008800+8*idx, (v4+4096)=0x80009000+8*idx */
		*(volatile uint64_t *)(0x80008800ULL + v4) = 0;
		arm64_dsb_isb();
		*(volatile uint64_t *)(0x80009000ULL + v4) = 0;
		arm64_dsb_isb();
	}

	v6 = &arm64_hwbp_state[idx];
	v6[1] = 0;
	v6[17] = 0;
	*(uint64_t *)&arm64_hwbp_state[2 * idx + 34] = 0;
	v6[66] = 0;
	v6[82] = 0;
	v6[98] = 0;
	ret = 0;

out_unlock:
	if (kfunc(__raw_spin_unlock))
		kfunc(__raw_spin_unlock)(&hwbp_lock);
	return ret;
}

/*
 * prctl 勾子：只处理读/写内存与按包名查 PID（0x4D454D31~35）
 * 触摸、陀螺仪、硬件断点等未实现，不处理则交给原 prctl
 */
static void before_prctl(hook_fargs5_t *args, void *udata)
{
	int option = (int)syscall_argn(args, 0);
	unsigned long arg2 = (unsigned long)syscall_argn(args, 1);
	struct prctl_mem_req mem;
	long ret = -22; /* -EINVAL */

	(void)udata;

	if (option != PRCTL_CMD_READ_MEM && option != PRCTL_CMD_WRITE_MEM &&
	    option != PRCTL_CMD_GET_PID &&
	    option != PRCTL_CMD_READ_MEM_SAFE && option != PRCTL_CMD_WRITE_MEM_SAFE)
		return;

	args->skip_origin = 1;

	/* ==================== 按包名查 PID ==================== */
	if (option == PRCTL_CMD_GET_PID) {
		uint8_t buf[PRCTL_PID_REQ_SIZE];
		int name_len;
		int result_pid;
		int copied;

		if (!kfunc(__arch_copy_from_user)) {
			args->ret = -EFAULT;
			return;
		}
		if (kfunc(__arch_copy_from_user)(buf, (const void __user *)arg2, PRCTL_PID_REQ_SIZE) != 0) {
			args->ret = -EFAULT;
			return;
		}
		name_len = *(int *)(buf + 256);
		if (name_len < 1 || name_len > PRCTL_PID_NAME_LEN) {
			args->ret = -EINVAL;
			return;
		}
		buf[name_len] = '\0';
		result_pid = get_pid_by_package(buf, (size_t)name_len);
		if (result_pid < 1) {
			args->ret = -3; /* 未找到 */
			return;
		}
		*(int *)(buf + 260) = result_pid;
		copied = compat_copy_to_user((void __user *)arg2, buf, PRCTL_PID_REQ_SIZE);
		if (copied != PRCTL_PID_REQ_SIZE) {
			args->ret = -EFAULT;
			return;
		}
		args->ret = 0;
		return;
	}

	/* ==================== 内存读写 ==================== */
	if (!kfunc(__arch_copy_from_user)) {
		args->ret = -EFAULT;
		return;
	}
	if (kfunc(__arch_copy_from_user)(&mem, (const void __user *)arg2, sizeof(mem)) != 0) {
		args->ret = -EFAULT;
		return;
	}
	if (!mem.buffer) {
		args->ret = -EFAULT;
		return;
	}

	switch (option) {
	case PRCTL_CMD_READ_MEM:
		ret = read_mem((pid_t)mem.pid, mem.addr, mem.buffer, mem.size);
		break;
	case PRCTL_CMD_WRITE_MEM:
		ret = write_mem((pid_t)mem.pid, mem.addr, mem.buffer, mem.size);
		break;
	case PRCTL_CMD_READ_MEM_SAFE:
		ret = read_mem_hardware_safe((pid_t)mem.pid, mem.addr, mem.buffer, mem.size);
		break;
	case PRCTL_CMD_WRITE_MEM_SAFE:
		ret = write_mem_hardware_safe((pid_t)mem.pid, mem.addr, mem.buffer, mem.size);
		break;
	default:
		ret = -22;
		break;
	}

	args->ret = (ret == 0) ? 0 : -5; /* 0 ok, -5  */
}

/*
 * 探测 task_struct->comm 偏移：用 pid 1 的 task，在 [COMM_PROBE_START, COMM_PROBE_END) 内
 * 查找 "init\0" (105,110,105,116,0)，找到则 g_comm_offset = 该偏移，否则用 COMM_OFFSET_FALLBACK
 */
static void probe_comm_offset(void)
{
	unsigned long off;
	struct task_struct *task;

	g_comm_offset = COMM_OFFSET_FALLBACK;

	if (!kfunc(find_task_by_vpid))
		return;

	task = kfunc(find_task_by_vpid)(1);
	if (!task)
		return;

	for (off = COMM_PROBE_START; off < COMM_PROBE_END; off++) {
		unsigned char *p = (unsigned char *)((unsigned long)task + off);
		if (p[0] == 105 && p[1] == 110 && p[2] == 105 && p[3] == 116 && !p[4])
			break;
	}
	if (off < COMM_PROBE_END)
		g_comm_offset = off;
}

/*
 * 模块初始化：解析 kfunc、探测 g_comm_offset、设置 g_phys_mem_available
 *  syscall_hook_demo_init 中与读写/包名相关的部分；触摸/屏幕/陀螺仪/prctl 等需另接
 */
static long ameliapro_init(const char *args, const char *event, void *reserved)
{
	kfunc_lookup_name(__arch_copy_from_user);
	kfunc_lookup_name(find_task_by_vpid);
	kfunc_lookup_name(access_process_vm);

	probe_comm_offset();

	g_phys_mem_available = (kfunc(access_process_vm) != NULL);

	if (fp_hook_syscalln(__NR_prctl, 5, before_prctl, 0, 0) != 0)
		; /* 勾子失败仅跳过，不阻止加载 */

	return 0;
}
KPM_INIT(ameliapro_init);

/* syscall_hook_control0：ctl0 入口，直接返回 0 */
static long ameliapro_ctl0(const char *ctl_args, char *__user out_msg, int outlen)
{
	(void)ctl_args;
	(void)out_msg;
	(void)outlen;
	return 0;
}
KPM_CTL0(ameliapro_ctl0);

/*
 * 模块退出：syscall_hook_demo_exit 还包含
 * 1) fp_unwrap_syscalln(167, 0, before_prctl, 0) 解除 prctl 勾子
 * 2) 遍历 10 个 touch_slots 调用 inject_touch_event_to_system 并清空槽
 * 3) 拿 hwbp_lock，清空硬件断点寄存器、arm64_hwbp_state / qword_32B8，放锁
 * 
 */
static long ameliapro_exit(void *reserved)
{
	(void)reserved;
	fp_unhook_syscalln(__NR_prctl, before_prctl, 0);
	return 0;
}
KPM_EXIT(ameliapro_exit);
