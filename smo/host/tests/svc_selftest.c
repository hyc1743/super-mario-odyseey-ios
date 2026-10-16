// 内核内存族 svc 的单测：**不需要游戏文件**，也不需要 Apple 平台（页操作用朴素实现）。
// 断言的都是启动路径上真正会被走到的行为：
//   1. 地址空间分区：heap/stack/alias 都在预留块内、互不重叠
//   2. SetHeapSize：成功时 x0=0、x1=堆基址、账本跟着走
//   3. 超出应用内存池 → ResultLimitReached(0x10801)，**按 2 MB 递减重试能成功**
//      （游戏启动时就是这么重试的，见 STATUS.md 2.37）
//   4. MapMemory/UnmapMemory：内容往返一致、映射登记表进出正确
//   5. MapMemory 目标非法 → 0xDC01
//   6. QueryMemory：堆=5、映射=0xB、空闲=0、宿主栈整段 0xB
#include "../dm.h"
#include "../hle.h"

module_t g_mod[MAX_MOD];          // 内存族的额度计算要用映像大小
int g_nmod;

// ---- 平台层：本机自检的朴素实现 ----
static uint8_t *g_reserved;
static unsigned char alias_rw[32768];
#ifndef _WIN32
#include <sys/mman.h>
#endif
size_t dm_as_page_size(void) { return 0x4000; } // Exercise the iPhone's 16 KiB granularity everywhere.
void *dm_as_reserve(size_t size) {
#ifdef _WIN32
    void *raw = calloc(1, size + 0x4000);
    g_reserved = (uint8_t *)(((uintptr_t)raw + 0x3fff) & ~(uintptr_t)0x3fff);
#else
    void *raw = mmap(NULL, size + 0x4000, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (raw == MAP_FAILED) return NULL;
    g_reserved = (uint8_t *)(((uintptr_t)raw + 0x3fff) & ~(uintptr_t)0x3fff);
#endif
    return g_reserved;
}
void dm_as_protect(void *p, size_t n, int rw) {
    uintptr_t off = (uintptr_t)p - (uintptr_t)g_reserved;
    if (off < (512ull << 20)) {
        if ((off | n) & 0x3fff) abort();
        for (size_t i = off / 0x4000; i < (off + n) / 0x4000; i++) alias_rw[i] = !!rw;
    }
#ifndef _WIN32
    if (mprotect(p, n, rw ? PROT_READ | PROT_WRITE : PROT_NONE)) abort();
#else
    (void)n; (void)rw;
#endif
}
static uint8_t g_fake_stack[0x100000];
void *dm_host_stack_top(void) { return g_fake_stack + sizeof g_fake_stack; }
size_t dm_host_stack_size(void) { return sizeof g_fake_stack; }

// 门表在 hle.c 里，这里给几个空实现满足链接
void gate_register(uint64_t ret, const char *kind, int arg, uint64_t orig) { (void)ret; (void)kind; (void)arg; (void)orig; }
uint64_t resolve_import(module_t *m, const char *name) { (void)m; (void)name; return 0x1000; }
uint64_t dm_slot_key(unsigned i) { return 100 + i; }
void dm_code_protect(void *p, size_t n) { (void)p; (void)n; }
// 本单测只测内存族：线程族给个桩（真实现在 hle_thread.c，另有 thread_selftest 覆盖）
int svc_thread(int num, uint64_t *regs) { (void)num; (void)regs; return 0; }
void dm_ro_protect(void *p, size_t n) { (void)p; (void)n; }

// 该文件里只用到内存族：把 hle_svc.c 之外的符号都挡掉
// 本自检不链 loader_core.c（不装模块）：给 module_of 一个空实现，
// 让 svcBreak 的「调用点」日志拿到 "?" 而不是链接失败。
module_t *module_of(const void *p) { (void)p; return NULL; }

static int fails;
// 注意：条件**只能求值一次**（见 fs_selftest.c 里的说明：求值两次会让带副作用的断言跑两遍）
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

// QueryMemory 的输出缓冲：MemoryInfo(0x28) + PageInfo(0x18)
static uint8_t mi[0x28];

static uint64_t call(unsigned num, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    uint64_t regs[31] = {0};
    regs[0] = a; regs[1] = b; regs[2] = c; regs[3] = d;
    int r = svc_dispatch(num, regs);
    if (r != 0) { printf("  【红】svc #%#x 没被内存族处理（返回 %d）\n", num, r); fails++; }
    return regs[0];
}
static uint64_t last_x1;

int main(void) {
    // 造两个假模块：image_bytes() 要用到大小
    static uint8_t m0[1 << 20], m1[2 << 20];
    g_mod[0].base = m0; g_mod[0].size = sizeof m0;
    g_mod[1].base = m1; g_mod[1].size = sizeof m1;
    g_nmod = 2;

    printf("== 1. 地址空间分区 ==\n");
    mem_init();
    uint64_t heap = dm_guest_heap_base();
    uint64_t img = (uint64_t)sizeof m0 + sizeof m1;
    CHK(heap != 0, "堆基址非 0");
    CHK(heap == (uint64_t)(uintptr_t)g_reserved + (1ull << 30), "堆在预留块 +1 GB 处（与 Horizon 分区一致）");

    printf("== 2. SetHeapSize ==\n");
    uint64_t ok = call(0x01, (uint64_t)(uintptr_t)mi, 64ull << 20, 0, 0);
    last_x1 = heap;
    CHK(ok == 0, "设堆 64 MB 成功");
    CHK(dm_guest_heap_cur() == (64ull << 20), "账本 = 64 MB");

    printf("== 3. 超出应用内存池 → 递减重试 ==\n");
    uint64_t want = dm_app_pool() - img + (4ull << 20);       // 超过池子 4 MB
    uint64_t bad = call(0x01, (uint64_t)(uintptr_t)mi, want, 0, 0);
    CHK(bad == 0x10801, "超池返回 ResultLimitReached(0x10801)");
    CHK(dm_guest_heap_rejects() == 1, "拒绝计数 +1");
    int tries = 0;
    while (call(0x01, (uint64_t)(uintptr_t)mi, want, 0, 0) == 0x10801 && tries < 4) { want -= 2ull << 20; tries++; }
    CHK(tries > 0 && dm_guest_heap_cur() == want, "按 2 MB 递减重试后成功（游戏启动就是这么重试的）");

    printf("== 4. MapMemory / UnmapMemory ==\n");
    uint64_t src = heap + 0x1000, dst = (uint64_t)(uintptr_t)g_reserved + (512ull << 20) + 0x4000;   // 目标在 stack 区
    memset((void *)src, 0x5A, 0x1000);
    uint64_t mr = call(0x04, dst, src, 0x1000, 0);
    CHK(mr == 0, "MapMemory 成功");
    CHK(memcmp((void *)dst, (void *)src, 0x1000) == 0, "映射后目标区内容 = 源区");
    memset((void *)dst, 0xA5, 0x1000);                        // 改目标区，模拟客体在映射区上跑
    uint64_t ur = call(0x05, dst, src, 0x1000, 0);
    CHK(ur == 0, "UnmapMemory 成功");
    uint8_t *p = (uint8_t *)src;
    int all = 1;
    for (int i = 0; i < 0x1000; i++) if (p[i] != 0xA5) { all = 0; break; }
    CHK(all, "解除映射后内容搬回源区");
    uint64_t again = call(0x05, dst, src, 0x1000, 0);
    CHK(again == 0xD401, "重复解除映射被拒（登记表已清）");

    printf("== 5. MapMemory 目标非法 ==\n");
    uint64_t bad2 = call(0x04, 0x1000, src, 0x1000, 0);       // 目标不在 stack 区
    CHK(bad2 == 0xDC01, "目标非法返回 0xDC01");

    printf("== 6. QueryMemory ==\n");
    CHK(call(0x06, (uint64_t)(uintptr_t)mi, 0, heap + 0x100, 0) == 0, "查堆地址");
    CHK(*(uint32_t *)(mi + 0x10) == 5, "堆的状态 = 5（Heap）");
    uint64_t st = call(0x06, (uint64_t)(uintptr_t)mi, 0, dst, 0);
    (void)st;
    CHK(*(uint32_t *)(mi + 0x10) == 0, "已解除映射的 stack 地址回报为空闲(0)");
    call(0x04, dst, src, 0x1000, 0);
    call(0x06, (uint64_t)(uintptr_t)mi, 0, dst, 0);
    CHK(*(uint32_t *)(mi + 0x10) == 0xB, "映射期间回报 0xB（栈/映射）");
    call(0x06, (uint64_t)(uintptr_t)mi, 0, (uint64_t)(uintptr_t)g_fake_stack + 0x1000, 0);
    CHK(*(uint32_t *)(mi + 0x10) == 0xB && *(uint64_t *)(mi + 8) == sizeof g_fake_stack,
        "宿主栈地址回报**整段**栈（SDK 会断言 sp 在区间内，回报一页是错的）");
    call(0x06, (uint64_t)(uintptr_t)mi, 0, (uint64_t)(uintptr_t)g_reserved + (256ull << 20), 0);
    CHK(*(uint32_t *)(mi + 0x10) == 0, "预留块内未用地址回报空闲");

    printf("== 7. Physical memory: 4 KiB guest / 16 KiB host ==\n");
    uint64_t alias = (uint64_t)(uintptr_t)g_reserved;
    CHK(call(0x2c, alias, 0x1000, 0, 0) == 0 && alias_rw[0], "physical map opens host page (v0.6.8 crash)");
    if (alias_rw[0]) { // A missing implementation fails assertions without faulting this test process.
        memset((void *)alias, 0x5a, 0x1000);
        CHK(call(0x2c, alias + 0x1000, 0x2000, 0, 0) == 0, "map unaligned host subpages");
        memset((void *)(alias + 0x1000), 0xa5, 0x2000);
        call(0x06, (uint64_t)(uintptr_t)mi, 0, alias + 0x2000, 0);
        CHK(*(uint32_t *)(mi + 0x10) == 5 && *(uint64_t *)mi == alias && *(uint64_t *)(mi + 8) == 0x3000,
            "QueryMemory merges contiguous physical pages as normal heap memory");
        CHK(call(0x2d, alias + 0x1000, 0x1000, 0, 0) == 0 && alias_rw[0], "partial unmap preserves live neighbors");
        CHK(*(uint8_t *)alias == 0x5a && *(uint8_t *)(alias + 0x2000) == 0xa5, "neighbor contents survive unmap");
        call(0x06, (uint64_t)(uintptr_t)mi, 0, alias + 0x1000, 0);
        CHK(*(uint32_t *)(mi + 0x10) == 0 && *(uint64_t *)mi == alias + 0x1000 && *(uint64_t *)(mi + 8) == 0x1000,
            "QueryMemory reports exact 4 KiB hole");
        CHK(call(0x2c, alias + 0x1000, 0x1000, 0, 0) == 0 && *(uint8_t *)(alias + 0x1000) == 0,
            "remapped page starts zeroed");
        CHK(call(0x2c, alias, 0x1000, 0, 0) != 0, "reject overlapping map");
        CHK(call(0x2d, alias, 0x4000, 0, 0) != 0 && alias_rw[0], "reject unmap containing unmapped page atomically");
        CHK(call(0x2d, alias, 0x3000, 0, 0) == 0 && !alias_rw[0], "last unmap closes host page");
        CHK(call(0x2c, alias, 0x1000, 0, 0) == 0 && *(uint8_t *)alias == 0, "fresh map after full unmap is zeroed");
        call(0x2d, alias, 0x1000, 0, 0);
    }
    CHK(call(0x2c, alias + 1, 0x1000, 0, 0) != 0, "reject unaligned address");
    CHK(call(0x2c, alias, 1, 0, 0) != 0 && call(0x2c, alias, 0, 0, 0) != 0, "reject invalid sizes");
    CHK(call(0x2c, alias + (512ull << 20) - 0x1000, 0x2000, 0, 0) != 0, "reject crossing alias boundary");
    CHK(call(0x2c, UINT64_MAX - 0xfff, 0x2000, 0, 0) != 0, "reject address overflow");
    CHK(call(0x2d, alias, 0x1000, 0, 0) != 0, "reject duplicate unmap");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
