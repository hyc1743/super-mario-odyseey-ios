// NVN 核心家族的真实现：Device / MemoryPool / Buffer / Sync / Sampler / Program。
//
// 约定：
//   * 统一签名 `uint64_t f(a0..a7)`（见 nvn_host.h）。参数按真实签名解释，这里逐个函数写明。
//   * 状态一律放宿主侧（nvn_state.c），**不写客体对象内存** —— 所以不依赖 NVN 结构的版本细节。
//   * 名字用**游戏真实索取的名字**（从 main.bin 抽出的 names.txt）；写错名字会链接不上，
//     生成器的一致性检查也会发现（见 smo/tools/gen_nvn_records.py 的 SKIP）。
//   * 这些函数一旦手写，就要把名字加进 gen_nvn_records.py 的 SKIP，否则记录桩会重复定义。
#include "nvn_host.h"

// ------------------------------------------------------------------ 状态表（nvn_state.c）
#include "nvn_state.h"

// 宿主侧睡眠：轮询等待用。mingw 有 usleep，其他平台用 nanosleep。
#if defined(_WIN32)
#  include <unistd.h>
void nvn_host_sleep_us(unsigned us) { usleep(us); }
#else
#  include <time.h>
void nvn_host_sleep_us(unsigned us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000u;
    ts.tv_nsec = (long)(us % 1000000u) * 1000L;
    (void)nanosleep(&ts, NULL);
}
#endif

#define NVN_T_DEVICE   1
#define NVN_T_DEVB     2
#define NVN_T_POOLB    3
#define NVN_T_POOL     4
#define NVN_T_BUFB     5
#define NVN_T_BUF      6
#define NVN_T_SAMPLERB 7
#define NVN_T_SAMPLER  8
#define NVN_T_SYNC     9
#define NVN_T_PROG     10

// 各类型的字段位置（v[] 下标）—— 只在本文件用，集中写在这里便于对照
#define DEVB_FLAGS      0
#define POOLB_DEV       0
#define POOLB_STORAGE   1
#define POOLB_SIZE      2
#define POOLB_FLAGS     3
#define POOL_DEV        0
#define POOL_STORAGE    1
#define POOL_SIZE       2
#define POOL_FLAGS      3
#define POOL_MAPPED     4
#define BUFB_DEV        0
#define BUFB_POOL       1
#define BUFB_ADDR       2
#define BUFB_SIZE       3
#define BUF_DEV         0
#define BUF_POOL        1
#define BUF_ADDR        2
#define BUF_SIZE        3
#define SAMP_DEV        0
#define SAMP_MINMAG     1
#define SAMP_WRAPS      2
#define SAMP_WRAPT      3
#define SAMP_WRAPR      4
#define SAMP_LODBIAS    5     // float 位模式
#define SAMP_MAXANISO   6
#define SAMP_COMPARE    7
#define SAMP_REDUCTION  8
#define SAMP_BORDER     9
#define SYNC_DEV        0
#define SYNC_SIGNALED   1
#define PROG_DEV        0
#define PROG_CTRL       1
#define PROG_CTRLSZ     2

// ------------------------------------------------------------------ 辅助
#define ARG_OBJ(a)   ((void *)(uintptr_t)(a))
#define ARG_U32(a)   ((uint32_t)(a))
#define ARG_I32(a)   ((int32_t)(a))
#define ARG_F32(a)   ((float)(uint32_t)(a))
#define F32_BITS(f)  ((uint64_t)(uint32_t)(f))

static nvn_st_ent *need(void *obj, uint32_t type, const char *who) {
    nvn_st_ent *e = nvn_st_get(obj, type);
    if (!e) NVN_LOG("【红】%s：对象 %p 不是已初始化的 %u 类（先 Initialize？）", who, obj, type);
    return e;
}

// ------------------------------------------------------------------ Device
uint64_t nvn_DeviceBuilderSetDefaults(uint64_t b, uint64_t a1, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(b), NVN_T_DEVB);
    if (e) e->v[DEVB_FLAGS] = 0;
    return 0;
}

uint64_t nvn_DeviceBuilderSetFlags(uint64_t b, uint64_t flags, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_DEVB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_DEVB); if (!e) return 0; }
    e->v[DEVB_FLAGS] = ARG_U32(flags);
    return 0;
}

uint64_t nvn_DeviceInitialize(uint64_t dev, uint64_t builder, uint64_t a2, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *d = nvn_st_new(ARG_OBJ(dev), NVN_T_DEVICE);
    if (!d) return 1;                                   // NVN 失败返回非 0
    nvn_st_ent *b = nvn_st_get(ARG_OBJ(builder), NVN_T_DEVB);
    d->v[0] = b ? b->v[DEVB_FLAGS] : 0;                 // 0 号位存设备标志
    d->v[1] = 0;                                        // 1 号位：纹理句柄计数
    d->v[2] = 0;                                        // 2 号位：texel-fetch 句柄计数
    d->v[3] = 0;                                        // 3 号位：image 句柄计数
    NVN_LOG("设备已初始化（%p，标志 0x%llx）", ARG_OBJ(dev), (unsigned long long)d->v[0]);
    return 0;
}

uint64_t nvn_DeviceFinalize(uint64_t dev, uint64_t a1, uint64_t a2, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(dev), NVN_T_DEVICE);
    return 0;
}

// 设备属性查询。**出参宽度按 int（4 字节）写**：NVN 的 C 接口对 GetInteger 用 int*。
// 这条是推测（不是实测），所以每次都打日志；真机上如果发现它被调用，就要按调用点核实宽度。
uint64_t nvn_DeviceGetInteger(uint64_t dev, uint64_t pname, uint64_t out, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    if (!need(ARG_OBJ(dev), NVN_T_DEVICE, "DeviceGetInteger")) return 1;
    NVN_LOG("DeviceGetInteger(pname=%u) → 先回 0（宽度按 int 猜的，见注释）", ARG_U32(pname));
    if (out) *(uint32_t *)(uintptr_t)out = 0;
    return 0;
}

// **这是整个图形层最关键的一个函数**：游戏（AGL）先用 nvnBootstrapLoader 取到它，
// 再交给 main 里的 nvnLoadCProcs 去解析 461 个 NVN 函数。换句话说，我们返回什么，
// 游戏的每一次 NVN 调用就打到哪里。查不到时返回 0（NVN 的真实行为，游戏自己会判空）。
uint64_t nvn_DeviceGetProcAddress(uint64_t dev, uint64_t name, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)dev; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    const char *n = (const char *)(uintptr_t)name;
    if (!n) return 0;
    nvn_fn f = nvn_lookup(n);
    static unsigned seen;
    if (!f && seen < 8) { seen++; NVN_LOG("DeviceGetProcAddress 查不到：%s（返回 0）", n); }
    return (uint64_t)(uintptr_t)f;
}

// ------------------------------------------------------------------ 内存池
uint64_t nvn_MemoryPoolBuilderSetDefaults(uint64_t b, uint64_t a1, uint64_t a2, uint64_t a3,
                                          uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_new(ARG_OBJ(b), NVN_T_POOLB);
    return 0;
}

uint64_t nvn_MemoryPoolBuilderSetDevice(uint64_t b, uint64_t dev, uint64_t a2, uint64_t a3,
                                        uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_POOLB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_POOLB); if (!e) return 1; }
    e->v[POOLB_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    return 0;
}

uint64_t nvn_MemoryPoolBuilderSetStorage(uint64_t b, uint64_t mem, uint64_t size, uint64_t a3,
                                         uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_POOLB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_POOLB); if (!e) return 1; }
    e->v[POOLB_STORAGE] = (uint64_t)(uintptr_t)ARG_OBJ(mem);
    e->v[POOLB_SIZE] = size;
    return 0;
}

uint64_t nvn_MemoryPoolBuilderSetFlags(uint64_t b, uint64_t flags, uint64_t a2, uint64_t a3,
                                       uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_POOLB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_POOLB); if (!e) return 1; }
    e->v[POOLB_FLAGS] = ARG_U32(flags);
    return 0;
}

uint64_t nvn_MemoryPoolInitialize(uint64_t pool, uint64_t builder, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *b = need(ARG_OBJ(builder), NVN_T_POOLB, "MemoryPoolInitialize(builder)");
    nvn_st_ent *p = nvn_st_new(ARG_OBJ(pool), NVN_T_POOL);
    if (!b || !p) return 1;
    p->v[POOL_DEV]     = b->v[POOLB_DEV];
    p->v[POOL_STORAGE] = b->v[POOLB_STORAGE];
    p->v[POOL_SIZE]    = b->v[POOLB_SIZE];
    p->v[POOL_FLAGS]   = b->v[POOLB_FLAGS];
    p->v[POOL_MAPPED]  = 0;
    return 0;
}

uint64_t nvn_MemoryPoolFinalize(uint64_t pool, uint64_t a1, uint64_t a2, uint64_t a3,
                                uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(pool), NVN_T_POOL);
    return 0;
}

// 游戏调用最多的一个（实测 122 处调用点）：返回可直接读写的地址。
// 之所以能直接返回：**客体代码在本机原生执行，客体地址就是宿主地址** —— 不需要拷贝、
// 也不需要"映射"这一步（真实的 NVN 要把显存映射进 CPU 可见窗口，我们没有这个约束）。
uint64_t nvn_MemoryPoolMap(uint64_t pool, uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolMap");
    if (!p) return 0;
    p->v[POOL_MAPPED] = 1;
    return p->v[POOL_STORAGE];
}

uint64_t nvn_MemoryPoolGetFlags(uint64_t pool, uint64_t a1, uint64_t a2, uint64_t a3,
                                uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolGetFlags");
    return p ? p->v[POOL_FLAGS] : 0;
}

// 主机与 GPU 的一致性操作：客体内存就是宿主内存，所以这里是空操作（不是"没实现"）。
uint64_t nvn_MemoryPoolFlushMappedRange(uint64_t pool, uint64_t off, uint64_t size, uint64_t a3,
                                        uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)off; (void)size; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolFlushMappedRange");
    return p ? 0 : 1;
}

uint64_t nvn_MemoryPoolInvalidateMappedRange(uint64_t pool, uint64_t off, uint64_t size, uint64_t a3,
                                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)off; (void)size; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolInvalidateMappedRange");
    return p ? 0 : 1;
}

// ------------------------------------------------------------------ 缓冲
uint64_t nvn_BufferBuilderSetDefaults(uint64_t b, uint64_t a1, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_new(ARG_OBJ(b), NVN_T_BUFB);
    return 0;
}

uint64_t nvn_BufferBuilderSetDevice(uint64_t b, uint64_t dev, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_BUFB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_BUFB); if (!e) return 1; }
    e->v[BUFB_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    return 0;
}

// nvnBufferBuilderSetStorage(builder, pool, address, size)
uint64_t nvn_BufferBuilderSetStorage(uint64_t b, uint64_t pool, uint64_t addr, uint64_t size,
                                     uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(b), NVN_T_BUFB);
    if (!e) { e = nvn_st_new(ARG_OBJ(b), NVN_T_BUFB); if (!e) return 1; }
    e->v[BUFB_POOL] = (uint64_t)(uintptr_t)ARG_OBJ(pool);
    e->v[BUFB_ADDR] = addr;
    e->v[BUFB_SIZE] = size;
    return 0;
}

uint64_t nvn_BufferInitialize(uint64_t buf, uint64_t builder, uint64_t a2, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *b = need(ARG_OBJ(builder), NVN_T_BUFB, "BufferInitialize(builder)");
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(buf), NVN_T_BUF);
    if (!b || !e) return 1;
    e->v[BUF_DEV]  = b->v[BUFB_DEV];
    e->v[BUF_POOL] = b->v[BUFB_POOL];
    e->v[BUF_ADDR] = b->v[BUFB_ADDR];
    e->v[BUF_SIZE] = b->v[BUFB_SIZE];
    return 0;
}

uint64_t nvn_BufferFinalize(uint64_t buf, uint64_t a1, uint64_t a2, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(buf), NVN_T_BUF);
    return 0;
}

uint64_t nvn_BufferGetAddress(uint64_t buf, uint64_t a1, uint64_t a2, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(buf), NVN_T_BUF, "BufferGetAddress");
    return e ? e->v[BUF_ADDR] : 0;
}

// ------------------------------------------------------------------ 同步对象
uint64_t nvn_SyncInitialize(uint64_t sync, uint64_t dev, uint64_t a2, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(sync), NVN_T_SYNC);
    if (!e) return 1;
    e->v[SYNC_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    e->v[SYNC_SIGNALED] = 0;
    return 0;
}

uint64_t nvn_SyncFinalize(uint64_t sync, uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(sync), NVN_T_SYNC);
    return 0;
}

// 宿主侧信号（队列/呈现路径调；真机上由 Metal 的完成回调触发）
void nvn_sync_signal(void *sync) {
    nvn_st_ent *e = nvn_st_get(sync, NVN_T_SYNC);
    if (e) e->v[SYNC_SIGNALED] = 1;
}

// nvnSyncWait(sync, timeout_ns)：0 = 等到，非 0 = 超时。
// 现在按 200 微秒轮询。真机上应换成条件变量（这里不换是为了不引入平台相关的等待原语，
// 等到 Metal 重放层接上时一起做 —— 那时它会被 GPU 完成回调唤醒，轮询就不合适了）。
uint64_t nvn_SyncWait(uint64_t sync, uint64_t timeout_ns, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(sync), NVN_T_SYNC, "SyncWait");
    if (!e) return 1;
    uint64_t waited_us = 0;
    while (!e->v[SYNC_SIGNALED]) {
        if (timeout_ns && waited_us * 1000ULL >= timeout_ns) return 1;   // 超时
        nvn_host_sleep_us(200);
        waited_us += 200;
    }
    e->v[SYNC_SIGNALED] = 0;              // NVN：等到之后同步对象自动复位
    return 0;
}

// ------------------------------------------------------------------ 采样器
#define SAMP_FN(name, field, conv)                                                        \
uint64_t name(uint64_t s, uint64_t val, uint64_t a2, uint64_t a3,                         \
              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {                       \
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;                            \
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(s), NVN_T_SAMPLERB);                                \
    if (!e) { e = nvn_st_new(ARG_OBJ(s), NVN_T_SAMPLERB); if (!e) return 1; }              \
    e->v[field] = (conv);                                                                 \
    return 0;                                                                             \
}

SAMP_FN(nvn_SamplerBuilderSetMinMagFilter, SAMP_MINMAG, ARG_U32(val))
SAMP_FN(nvn_SamplerBuilderSetWrapMode,     SAMP_WRAPS,  ARG_U32(val))
SAMP_FN(nvn_SamplerBuilderSetMaxAnisotropy, SAMP_MAXANISO, ARG_I32(val))
SAMP_FN(nvn_SamplerBuilderSetCompare,      SAMP_COMPARE, ARG_U32(val))
SAMP_FN(nvn_SamplerBuilderSetReductionFilter, SAMP_REDUCTION, ARG_U32(val))

// 注意：这个函数的第二参数是 **float**，按 ABI 走 s0 而**不是** x1。
// 所以统一 u64 签名对它不成立 —— 必须用真正的 float 形参（表里仍然按统一函数指针存，
// 因为调用方（客体）按它自己的约定传参）。同理适用于 CommandBufferSetAlphaRef 等带 float 的接口。
uint64_t nvn_SamplerBuilderSetLodBias(uint64_t s, float bias, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(s), NVN_T_SAMPLERB);
    if (!e) { e = nvn_st_new(ARG_OBJ(s), NVN_T_SAMPLERB); if (!e) return 1; }
    uint32_t bits;
    __builtin_memcpy(&bits, &bias, 4);
    e->v[SAMP_LODBIAS] = bits;
    return 0;
}

uint64_t nvn_SamplerBuilderSetDefaults(uint64_t s, uint64_t a1, uint64_t a2, uint64_t a3,
                                       uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_new(ARG_OBJ(s), NVN_T_SAMPLERB);
    return 0;
}

uint64_t nvn_SamplerBuilderSetDevice(uint64_t s, uint64_t dev, uint64_t a2, uint64_t a3,
                                     uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(s), NVN_T_SAMPLERB);
    if (!e) { e = nvn_st_new(ARG_OBJ(s), NVN_T_SAMPLERB); if (!e) return 1; }
    e->v[SAMP_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    return 0;
}

uint64_t nvn_SamplerInitialize(uint64_t s, uint64_t builder, uint64_t a2, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *b = nvn_st_get(ARG_OBJ(builder), NVN_T_SAMPLERB);
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(s), NVN_T_SAMPLER);
    if (!b || !e) return 1;
    for (unsigned i = 0; i < 12; i++) e->v[i] = b->v[i];
    return 0;
}

uint64_t nvn_SamplerFinalize(uint64_t s, uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(s), NVN_T_SAMPLER);
    nvn_st_del(ARG_OBJ(s), NVN_T_SAMPLERB);
    return 0;
}

// ------------------------------------------------------------------ 程序（着色器）
uint64_t nvn_ProgramInitialize(uint64_t prog, uint64_t dev, uint64_t a2, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(prog), NVN_T_PROG);
    if (!e) return 1;
    e->v[PROG_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    return 0;
}

uint64_t nvn_ProgramFinalize(uint64_t prog, uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(prog), NVN_T_PROG);
    return 0;
}

// nvnProgramSetShaders(program, controlSize, control)：
// 控制段就是 Maxwell 着色器 blob（我们的离线翻译链吃的东西，见 STATUS.md 第七轮）。
// 这里先把控制段记下来并打印键信息（大小 + 头 16 字节），Metal 重放层接上后按它查 18470 个已译着色器。
static unsigned g_shader_sets;
uint64_t nvn_ProgramSetShaders(uint64_t prog, uint64_t size, uint64_t ctrl, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(prog), NVN_T_PROG, "ProgramSetShaders");
    if (!e) return 1;
    e->v[PROG_CTRL]   = ctrl;
    e->v[PROG_CTRLSZ] = size;
    g_shader_sets++;
    if (g_shader_sets <= 3) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)ctrl;
        NVN_LOG("ProgramSetShaders：控制段 %llu 字节，头 16 字节 %02x%02x%02x%02x %02x%02x%02x%02x "
                "%02x%02x%02x%02x %02x%02x%02x%02x",
                (unsigned long long)size,
                p ? p[0] : 0, p ? p[1] : 0, p ? p[2] : 0, p ? p[3] : 0,
                p ? p[4] : 0, p ? p[5] : 0, p ? p[6] : 0, p ? p[7] : 0,
                p ? p[8] : 0, p ? p[9] : 0, p ? p[10] : 0, p ? p[11] : 0,
                p ? p[12] : 0, p ? p[13] : 0, p ? p[14] : 0, p ? p[15] : 0);
    }
    return 0;
}

unsigned nvn_shader_sets(void) { return g_shader_sets; }
