// 内核内存族 svc（Horizon 的系统调用）。派生自参考项目 d2r-mobile 的 dm/host/hle_svc.c（MIT）。
//
// 内存模型（与参考项目一致）：
//   · 预留**一整块**客体地址空间（mmap PROT_NONE，只占地址不占内存），在里面按 Horizon 分区：
//     alias 512 MB、stack 256 MB、heap 从 +1 GB 处起（4 GB 容量）
//   · 游戏启动时调 SetHeapSize 把「整块应用内存池 − 映像footprint」设成堆，之后自己切成自己的分配器
//   · MapMemory/UnmapMemory 用**内容拷贝**模拟地址别名（SDK 建线程时把堆上的栈搬进栈区；
//     映射期间源区在真机上不可访问，所以拷贝语义等价）
//   · QueryMemory 回报区间（基址、大小、状态、权限），SDK 靠它找空位、认自己的栈
//
// 平台相关的只有两个页操作（dm_as_reserve / dm_as_protect，见 loader_apple.c 与自检里的实现），
// 所以这份文件能在本机当单测跑（smo/host/tests/svc_selftest.c）。
#include "dm.h"
#include "logbuf.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>

// ---- 地址空间分区 ----
#define RESERVE   (5ull << 30)          // 预留 5 GB：前 1 GB（alias + stack）+ 4 GB 堆
#define ALIAS_SZ  (512ull << 20)
#define STACK_SZ  (256ull << 20)
#define HEAP_OFF  0x40000000ull         // 堆从预留块 +1 GB 处起
#define HEAP_CAP  (4ull << 30)

static uint8_t *g_as, *g_alias, *g_stack, *g_heap;
static size_t g_heap_cur, g_stack_cap, g_heap_cap;
static int g_heap_rejects, g_init_done;

// 报告给游戏的「应用内存池」。参考项目的实测结论值得抄下来：
//   游戏会把自己报的总量**整块**当成堆设下去（SetHeapSize），再交给自己的分配器；
//   池子报小了，主堆余量就小（他们实测 3 GB → 主堆只剩 ~18 MB，某个 128 KB 请求返回空，
//   而原版游戏很多地方**不检查空指针**就写）。所以他们报 3285 MB + 512 MB。
// SMO 的这个数还没在真机上量过 —— 在量出来之前，先用同一个数，并把它放在一个显眼的地方。
static uint64_t g_app_pool_extra = 512ull << 20;
uint64_t dm_app_pool(void) { return (3285ull << 20) + g_app_pool_extra; }

// 映像 footprint：SetHeapSize 的可用额度 = 应用内存池 − 映像
static uint64_t image_bytes(void) {
    uint64_t n = 0;
    for (int i = 0; i < g_nmod; i++) n += g_mod[i].size;
    return n;
}

void mem_init(void) {
    if (g_init_done) return;
    g_init_done = 1;
    g_as = dm_as_reserve(RESERVE);
    if (!g_as) die("预留客体地址空间失败");
    g_alias = g_as;
    g_stack = g_as + ALIAS_SZ;
    g_stack_cap = STACK_SZ;
    g_heap = g_as + HEAP_OFF;
    g_heap_cap = HEAP_CAP;
    LOG("客体地址空间 %p +%#llx：alias %p/512MB、stack %p/256MB、heap %p/%lluMB",
        g_as, (unsigned long long)RESERVE, g_alias, g_stack, g_heap,
        (unsigned long long)(HEAP_CAP >> 20));
}

// ---- 已映射区间（MapMemory/MapPhysicalMemory 用；QueryMemory 据此回报）----
#define MAX_MAP 4096
static struct { uint64_t base, size, src; uint32_t state; } g_map[MAX_MAP];
static int g_nmap;
static pthread_mutex_t g_map_lock = PTHREAD_MUTEX_INITIALIZER;

static int map_find(uint64_t a, uint64_t n) {           // 1 基下标；0 = 无重叠
    for (int i = 0; i < g_nmap; i++)
        if (a < g_map[i].base + g_map[i].size && g_map[i].base < a + n) return i + 1;
    return 0;
}
static void map_add(uint64_t b, uint64_t n, uint64_t src, uint32_t st) {
    pthread_mutex_lock(&g_map_lock);
    if (g_nmap >= MAX_MAP) die("映射表满");
    g_map[g_nmap].base = b; g_map[g_nmap].size = n; g_map[g_nmap].src = src; g_map[g_nmap].state = st;
    g_nmap++;
    pthread_mutex_unlock(&g_map_lock);
}
static void map_del(int i) { pthread_mutex_lock(&g_map_lock); g_map[i] = g_map[--g_nmap]; pthread_mutex_unlock(&g_map_lock); }

// 供诊断/自检用
uint64_t dm_guest_heap_base(void) { return (uint64_t)(uintptr_t)g_heap; }
uint64_t dm_guest_heap_cur(void)  { return g_heap_cur; }
int dm_guest_heap_rejects(void)   { return g_heap_rejects; }

// 线程族在 hle_thread.c
#include "hle_thread.h"

// Horizon 的内存状态码（QueryMemory 用；取自参考项目的实测值）
enum { MS_FREE = 0, MS_CODE = 3, MS_HEAP = 5, MS_STACK = 0xB };

// Horizon's physical mappings use 4 KiB pages, independently of the host page size.
// Keep a guest-page ledger: removing a 4 KiB mapping must not protect a still-live
// neighbor in the same 16 KiB iPhone page. QueryMemory also uses this ledger.
#define GUEST_PAGE 0x1000ull
#define ALIAS_PAGES (ALIAS_SZ / GUEST_PAGE)
static uint8_t g_physical[ALIAS_PAGES];
static uint64_t g_physical_used;

static uint64_t physical_map(unsigned num, uint64_t addr, uint64_t size) {
    uint64_t base = (uint64_t)(uintptr_t)g_alias;
    if (!size || ((addr | size) & (GUEST_PAGE - 1)) || size > ALIAS_SZ ||
        addr < base || addr - base > ALIAS_SZ - size) return 0xD401;
    uint64_t first = (addr - base) / GUEST_PAGE, count = size / GUEST_PAGE;
    int mapping = num == 0x2c;
    size_t host_page = dm_as_page_size();
    if (host_page < GUEST_PAGE || (host_page & (host_page - 1)) ||
        (base & (host_page - 1)) || ALIAS_SZ % host_page)
        die("unsupported physical mapping host page size: %#zx", host_page);
    uint64_t per_host = host_page / GUEST_PAGE;
    uint64_t lo = first / per_host * per_host;
    uint64_t hi = (first + count + per_host - 1) / per_host * per_host;
    pthread_mutex_lock(&g_map_lock);
    for (uint64_t i = first; i < first + count; i++) {
        if (!!g_physical[i] == mapping) {
            pthread_mutex_unlock(&g_map_lock);
            return 0xD401;
        }
    }
    if (mapping) {
        dm_as_protect(g_alias + lo * GUEST_PAGE, (hi - lo) * GUEST_PAGE, 1);
        memset((void *)(uintptr_t)addr, 0, size);
        memset(g_physical + first, 1, count);
        g_physical_used += size;
    } else {
        // Clear while the mapping is writable; future mappings cannot expose old data.
        memset((void *)(uintptr_t)addr, 0, size);
        memset(g_physical + first, 0, count);
        g_physical_used -= size;
        for (uint64_t p = lo; p < hi; p += per_host) {
            int live = 0;
            for (uint64_t i = p; i < p + per_host; i++) live |= g_physical[i];
            if (!live) dm_as_protect(g_alias + p * GUEST_PAGE, host_page, 0);
        }
    }
    pthread_mutex_unlock(&g_map_lock);
    LOG("%sPhysicalMemory %#llx +%#llx", mapping ? "Map" : "Unmap",
        (unsigned long long)addr, (unsigned long long)size);
    return 0;
}

int svc_dispatch(unsigned num, uint64_t *regs) {
    mem_init();
    // 每条 svc 都记进日志（前 120 条）：崩在某条 svc 上时，这是唯一能指出「哪一条」的线索。
    // 真机第七崩（SIGABRT 在系统库里）就是靠这个下一轮定位的。
    {
        static unsigned logged;
        if (logged < 120) {
            logged++;
            LOG("svc #%u(%#llx, %#llx, %#llx, %#llx, %#llx)", num,
                (unsigned long long)regs[0], (unsigned long long)regs[1],
                (unsigned long long)regs[2], (unsigned long long)regs[3],
                (unsigned long long)regs[4]);
        }
    }
    if (svc_thread((int)num, regs)) return 0;
    switch (num) {
    case 0x01: {                                        // SetHeapSize(out_addr*, size)
        size_t size = (size_t)regs[1];
        if (size > g_heap_cap) { LOG("SetHeapSize 超容量：%#zx > %#zx", size, g_heap_cap); regs[0] = 0xD401; return 0; }
        if (size + image_bytes() > dm_app_pool()) {     // 超出应用内存池：ResultLimitReached
            g_heap_rejects++;
            regs[0] = 0x10801;
            return 0;                                   // 游戏会按 2 MB 递减重试直到放得下
        }
        if (size > g_heap_cur) dm_as_protect(g_heap + g_heap_cur, size - g_heap_cur, 1);
        else if (size < g_heap_cur) dm_as_protect(g_heap + size, g_heap_cur - size, 0);
        g_heap_cur = size;
        regs[0] = 0;
        regs[1] = (uint64_t)(uintptr_t)g_heap;
        return 0;
    }
    case 0x02: regs[0] = 0; return 0;                   // SetMemoryPermission
    case 0x03: regs[0] = 0; return 0;                   // SetMemoryAttribute
    case 0x04: {                                        // MapMemory(dst, src, size)
        uint64_t dst = regs[0], src = regs[1], size = regs[2];
        if ((uint8_t *)dst < g_stack || (uint8_t *)dst + size > g_stack + g_stack_cap || map_find(dst, size)) {
            LOG("MapMemory 目标非法或已占用 %#llx +%#llx", (unsigned long long)dst, (unsigned long long)size);
            regs[0] = 0xDC01;
            return 0;
        }
        dm_as_protect((void *)dst, size, 1);
        memcpy((void *)dst, (void *)src, size);          // 搬移语义
        map_add(dst, size, src, MS_STACK);
        regs[0] = 0;
        return 0;
    }
    case 0x05: {                                        // UnmapMemory(dst, src, size)
        uint64_t dst = regs[0], src = regs[1], size = regs[2];
        int k = map_find(dst, size);
        if (!k || g_map[k - 1].base != dst || g_map[k - 1].size != size || g_map[k - 1].src != src) {
            LOG("UnmapMemory 与映射不符 %#llx +%#llx", (unsigned long long)dst, (unsigned long long)size);
            regs[0] = 0xD401;
            return 0;
        }
        memcpy((void *)src, (void *)dst, size);           // 内容搬回源区
        map_del(k - 1);
        regs[0] = 0;
        return 0;
    }
    case 0x06: {                                        // QueryMemory(MemoryInfo* out, PageInfo* pout, addr)
        uint64_t mi = regs[0], addr = regs[2];
        uint8_t *a = (uint8_t *)addr;
        uint64_t base, size, state, perm;
        int k;
        if (addr >= (uint64_t)(uintptr_t)g_alias && addr - (uint64_t)(uintptr_t)g_alias < ALIAS_SZ) {
            pthread_mutex_lock(&g_map_lock);
            uint64_t i = (addr - (uint64_t)(uintptr_t)g_alias) / GUEST_PAGE;
            uint64_t lo = i, hi = i + 1;
            int mapped = g_physical[i];
            while (lo && g_physical[lo - 1] == mapped) lo--;
            while (hi < ALIAS_PAGES && g_physical[hi] == mapped) hi++;
            base = (uint64_t)(uintptr_t)g_alias + lo * GUEST_PAGE;
            size = (hi - lo) * GUEST_PAGE;
            state = mapped ? MS_HEAP : MS_FREE; perm = mapped ? 3 : 0;
            pthread_mutex_unlock(&g_map_lock);
        }
        else if (a >= g_heap && a < g_heap + g_heap_cur) { base = (uint64_t)(uintptr_t)g_heap; size = g_heap_cur; state = MS_HEAP; perm = 3; }
        else if ((k = map_find(addr, 1))) { base = g_map[k - 1].base; size = g_map[k - 1].size; state = g_map[k - 1].state; perm = 3; }
        else if (a >= g_as && a < g_as + RESERVE) {       // 空闲：到相邻映射/堆边界为止
            uint64_t lo = (uint64_t)(uintptr_t)g_as, hi = lo + RESERVE;
            if (a >= g_heap) lo = (uint64_t)(uintptr_t)g_heap + g_heap_cur;
            else if (hi > (uint64_t)(uintptr_t)g_heap) hi = (uint64_t)(uintptr_t)g_heap;
            for (int i = 0; i < g_nmap; i++) {
                uint64_t e = g_map[i].base + g_map[i].size;
                if (e <= addr && e > lo) lo = e;
                if (g_map[i].base > addr && g_map[i].base < hi) hi = g_map[i].base;
            }
            base = lo; size = hi - lo; state = MS_FREE; perm = 0;
        }
        else {                                            // 落在宿主线程栈里：回报整段栈（SDK 会断言 sp 在其区间内）
            uint64_t top = (uint64_t)(uintptr_t)dm_host_stack_top();
            uint64_t ssz = dm_host_stack_size();
            if (top && addr < top && addr >= top - ssz) { base = top - ssz; size = ssz; state = MS_STACK; perm = 3; }
            else { base = addr & ~0xFFFull; size = 0x1000; state = MS_CODE; perm = 5; }
        }
        if (mi) {                                         // MemoryInfo: base(0) size(8) state(0x10) attr(0x14) perm(0x18)
            uint64_t *m = (uint64_t *)mi;
            m[0] = base; m[1] = size;
            ((uint32_t *)mi)[4] = (uint32_t)state; ((uint32_t *)mi)[5] = 0;
            ((uint32_t *)mi)[6] = (uint32_t)perm;  ((uint32_t *)mi)[7] = 0;
            m[4] = 0;
        }
        regs[0] = 0;
        regs[1] = 0;
        return 0;
    }
    case 0x2c: case 0x2d:
        regs[0] = physical_map(num, regs[0], regs[1]);
        return 0;
    // ---- 杂项族（照参考项目 dm/host/hle_svc.c 的实现抄；真机第八轮卡住就是缺这几个）----
    // 为什么必须实现：未实现的 svc 以前既不写 regs[0] 也不报错，SDK 读到的是**栈上的垃圾** ——
    // 真机上 GetInfo(RandomEntropy) 拿到垃圾后 SDK 直接 svcBreak(Panic)（日志停在 svc #38）。
    case 0x1E: {                                          // GetSystemTick：19.2 MHz 计数器
        static uint64_t t0;
        struct timespec ts; timespec_get(&ts, TIME_UTC);
        uint64_t now = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        if (!t0) t0 = now;
        regs[0] = (now - t0) * 192 / 10;
        return 0;
    }
    case 0x26: {                                          // Break(reason, ptr, size)：SDK 致命中止
        uint32_t res = (regs[1] && regs[2] >= 4) ? *(const uint32_t *)(uintptr_t)regs[1] : 0;
        // 调用点 = x30（LR）：门入口保存了 x0..x30，所以能报出「是哪个模块的哪个偏移在 abort」
        module_t *mm = module_of((void *)(uintptr_t)regs[30]);
        LOG("【中止】svcBreak reason=%#llx Result=%#x ptr=%#llx size=%#llx 调用点 %s+%#llx",
            (unsigned long long)regs[0], res, (unsigned long long)regs[1], (unsigned long long)regs[2],
            mm ? mm->name : "?",
            mm ? (unsigned long long)(regs[30] - (uint64_t)(uintptr_t)mm->base) : 0);
        if (regs[0] & 0x80000000ull) { regs[0] = 0; return 0; }   // 高位 = 仅通知，可继续
        // 真机上没有调试器时内核会终止进程；这里是宿主，等价做法是明确停下并留下现场
        dm_progress_finish(DM_RUN_FAIL, "游戏运行库主动中止（svcBreak）—— 见日志最后一行");
        dm_log_raw("【中止】进程结束（svcBreak 致命）\n");
        _exit(42);
    }
    case 0x27: {                                          // OutputDebugString(str, len)
        if (regs[0] && regs[1]) LOG("[客体] %.*s", (int)(regs[1] > 512 ? 512 : regs[1]), (const char *)(uintptr_t)regs[0]);
        regs[0] = 0;
        return 0;
    }
    case 0x29: {                                          // GetInfo(out*, id, handle, subid) → x1 = 值
        uint64_t id = regs[1], v = 0;
        switch (id) {
        case 0:  v = 7; break;                                  // CoreMask（与参考项目同值）
        case 1:  v = 0; break;                                  // PriorityMask
        case 2:  v = (uint64_t)(uintptr_t)g_alias; break;       // AliasAddr
        case 3:  v = g_alias ? ALIAS_SZ : 0; break;             // AliasSize
        case 4:  v = (uint64_t)(uintptr_t)g_heap; break;        // HeapAddr
        case 5:  v = g_heap_cap; break;                         // HeapSize
        case 6: case 21: v = dm_app_pool(); break;              // TotalMem / TotalNonSystem
        case 7: case 22:
            pthread_mutex_lock(&g_map_lock);
            v = g_heap_cur + image_bytes() + g_physical_used;
            pthread_mutex_unlock(&g_map_lock);
            break; // UsedMem / UsedNonSystem
        case 8:  v = 0; break;                                  // DebuggerAttached
        case 9:  v = 0; break;                                  // ResourceLimit
        case 10: v = 0; break;                                  // IdleTick
        case 11: {                                              // RandomEntropy：每次给新值
            static uint64_t n;
            struct timespec ts; timespec_get(&ts, TIME_UTC);
            v = ((uint64_t)ts.tv_sec << 32) ^ ((uint64_t)ts.tv_nsec << 8) ^ (++n) ^ (uint64_t)(uintptr_t)&ts;
            break;
        }
        case 12: v = (uint64_t)(uintptr_t)g_as; break;          // AslrAddr
        case 13: v = g_as ? RESERVE : 0; break;                 // AslrSize
        case 14: v = (uint64_t)(uintptr_t)g_stack; break;       // StackAddr
        case 15: v = g_stack_cap; break;                        // StackSize
        case 16: v = 0x2000000; break;                          // SystemResourceTotal
        case 17: v = 0x400000; break;                           // SystemResourceUsed
        case 18: v = 0x0100000000010000ull; break;              // ProgramId（SMO 的 title id）
        case 20: v = 0; break;                                  // UserExceptionContextAddr
        case 23: v = 1; break;                                  // IsApplication
        default: v = 0; break;
        }
        regs[0] = 0;
        regs[1] = v;
        return 0;
    }
    default: {
        // 记录一次（每个号只报一次）并**返回成功**：以前这里是「什么都不做」，SDK 会读到
        // 未初始化的返回寄存器，然后在别处崩/卡，排查成本极高。
        static uint8_t seen[256];
        if (!seen[num & 0xFF]) {
            seen[num & 0xFF] = 1;
            LOG("SVC %#x 未实现（x0=%#llx x1=%#llx x2=%#llx）—— 占位返回成功", num,
                (unsigned long long)regs[0], (unsigned long long)regs[1], (unsigned long long)regs[2]);
        }
        regs[0] = 0;
        return 0;
    }
    }
}
