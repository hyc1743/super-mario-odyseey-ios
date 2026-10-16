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
#include "nvn_tex.h"
#include "nvn_gpu.h"
#include "nvn_shader.h"
#include <time.h>

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
#define NVN_T_TEXB     11
#define NVN_T_TEX      12

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
    if (!d) return NVN_FALSE;                                   // NVNboolean: false on failure
    nvn_st_ent *b = nvn_st_get(ARG_OBJ(builder), NVN_T_DEVB);
    d->v[0] = b ? b->v[DEVB_FLAGS] : 0;                 // 0 号位存设备标志
    d->v[1] = 0;                                        // 1 号位：纹理句柄计数
    d->v[2] = 0;                                        // 2 号位：texel-fetch 句柄计数
    d->v[3] = 0;                                        // 3 号位：image 句柄计数
    NVN_LOG("设备已初始化（%p，标志 0x%llx）", ARG_OBJ(dev), (unsigned long long)d->v[0]);
    return NVN_TRUE;
}

uint64_t nvn_DeviceFinalize(uint64_t dev, uint64_t a1, uint64_t a2, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(dev), NVN_T_DEVICE);
    return 0;
}

// Device properties adapted from d2r-mobile (MIT; third_party/d2r-mobile.LICENSE).
// SMO main+0x731358 requires major 53, minor >=13. These are API compatibility
// values, independent of the host GPU. All outputs are signed 32-bit integers.
#include "nvn_device_info.inc"
uint64_t nvn_DeviceGetInteger(uint64_t dev, uint64_t pname, uint64_t out, uint64_t a3,
                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)dev; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    uint32_t index = ARG_U32(pname);
    int value = index < sizeof g_devinfo / sizeof g_devinfo[0] ? g_devinfo[index] : 0;
    if (out) *(int32_t *)(uintptr_t)out = value;
    static uint8_t logged[256];
    if (index >= sizeof logged || !logged[index]) {
        if (index < sizeof logged) logged[index] = 1;
        NVN_LOG("DeviceGetInteger(%u) = %d", index, value);
    }
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
    if (!b || !p) return NVN_FALSE;
    p->v[POOL_DEV]     = b->v[POOLB_DEV];
    p->v[POOL_STORAGE] = b->v[POOLB_STORAGE];
    p->v[POOL_SIZE]    = b->v[POOLB_SIZE];
    p->v[POOL_FLAGS]   = b->v[POOLB_FLAGS];
    p->v[POOL_MAPPED]  = 0;
    NVN_LOG("MemoryPoolInitialize pool=%p storage=%p size=%#llx flags=%#llx", ARG_OBJ(pool),
            ARG_OBJ(p->v[POOL_STORAGE]), (unsigned long long)p->v[POOL_SIZE], (unsigned long long)p->v[POOL_FLAGS]);
    return NVN_TRUE;
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
    if (!p->v[POOL_STORAGE]) NVN_LOG("【红】MemoryPoolMap pool=%p has no CPU storage size=%#llx flags=%#llx",
        ARG_OBJ(pool), (unsigned long long)p->v[POOL_SIZE], (unsigned long long)p->v[POOL_FLAGS]);
    return p->v[POOL_STORAGE];
}

uint64_t nvn_MemoryPoolGetFlags(uint64_t pool, uint64_t a1, uint64_t a2, uint64_t a3,
                                uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolGetFlags");
    return p ? p->v[POOL_FLAGS] : 0;
}

uint64_t nvn_MemoryPoolGetBufferAddress(uint64_t pool, uint64_t a1, uint64_t a2, uint64_t a3,
                                       uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *p = need(ARG_OBJ(pool), NVN_T_POOL, "MemoryPoolGetBufferAddress");
    return p ? p->v[POOL_STORAGE] : 0;
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

// nvnBufferBuilderSetStorage(builder, pool, byte_offset, size): the third
// argument is an offset inside the pool, never an absolute CPU address.
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
    if (!b) return NVN_FALSE;
    nvn_st_ent *p = need(ARG_OBJ(b->v[BUFB_POOL]), NVN_T_POOL, "BufferInitialize(pool)");
    if (!p || b->v[BUFB_ADDR] > p->v[POOL_SIZE] ||
        b->v[BUFB_SIZE] > p->v[POOL_SIZE] - b->v[BUFB_ADDR]) return NVN_FALSE;
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(buf), NVN_T_BUF);
    if (!e) return NVN_FALSE;
    e->v[BUF_DEV]  = b->v[BUFB_DEV];
    e->v[BUF_POOL] = b->v[BUFB_POOL];
    e->v[BUF_ADDR] = b->v[BUFB_ADDR];
    e->v[BUF_SIZE] = b->v[BUFB_SIZE];
    return NVN_TRUE;
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
    if (!e) return 0;
    nvn_st_ent *p = need(ARG_OBJ(e->v[BUF_POOL]), NVN_T_POOL, "BufferGetAddress(pool)");
    return p && p->v[POOL_STORAGE] ? p->v[POOL_STORAGE] + e->v[BUF_ADDR] : 0;
}

uint64_t nvn_BufferMap(uint64_t buf, uint64_t a1, uint64_t a2, uint64_t a3,
                       uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    return nvn_BufferGetAddress(buf, a1, a2, a3, a4, a5, a6, a7);
}

// ------------------------------------------------------------------ Texture storage (same block layout as the detiler).
// v[]: device, width, height, depth, levels, format, flags, target,
// samples, pool, pool byte offset, linear stride. Objects remain opaque.
#include "nvn_format_info.inc"
uint64_t nvn_TextureBuilderSetDefaults(uint64_t obj, uint64_t a1, uint64_t a2, uint64_t a3,
                                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(obj), NVN_T_TEXB);
    uint64_t dev = e ? e->v[0] : 0;
    e = nvn_st_new(ARG_OBJ(obj), NVN_T_TEXB);
    if (e) { e->v[0] = dev; e->v[1] = e->v[2] = e->v[3] = e->v[4] = e->v[7] = 1; }
    return 0;
}
#define TEX_SET(name, index) \
uint64_t nvn_TextureBuilderSet##name(uint64_t obj, uint64_t value, uint64_t a2, uint64_t a3, \
                                    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) { \
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; \
    nvn_st_ent *e = nvn_st_get(ARG_OBJ(obj), NVN_T_TEXB); \
    if (!e) e = nvn_st_new(ARG_OBJ(obj), NVN_T_TEXB); \
    if (e) { e->v[index] = value; } return 0; \
}
TEX_SET(Device, 0)
TEX_SET(Width, 1)
TEX_SET(Height, 2)
TEX_SET(Depth, 3)
TEX_SET(Levels, 4)
TEX_SET(Format, 5)
TEX_SET(Flags, 6)
TEX_SET(Target, 7)
TEX_SET(Samples, 8)
TEX_SET(Stride, 11)
#undef TEX_SET
uint64_t nvn_TextureBuilderSetSize3D(uint64_t obj, uint64_t w, uint64_t h, uint64_t d,
                                    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(obj), NVN_T_TEXB, "TextureBuilderSetSize");
    if (e) { e->v[1] = w; e->v[2] = h; e->v[3] = d; }
    return 0;
}
uint64_t nvn_TextureBuilderSetSize2D(uint64_t obj, uint64_t w, uint64_t h, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; return nvn_TextureBuilderSetSize3D(obj, w, h, 1, a4, a5, a6, a7);
}
uint64_t nvn_TextureBuilderSetSize1D(uint64_t obj, uint64_t w, uint64_t a2, uint64_t a3,
                                    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; return nvn_TextureBuilderSetSize3D(obj, w, 1, 1, a4, a5, a6, a7);
}
uint64_t nvn_TextureBuilderSetStorage(uint64_t obj, uint64_t pool, uint64_t off, uint64_t a3,
                                     uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(obj), NVN_T_TEXB, "TextureBuilderSetStorage");
    if (e) { e->v[9] = pool; e->v[10] = off; }
    return 0;
}
static uint64_t texture_size(const nvn_st_ent *e) {
    if (!e) return 0;
    texture_fmt fmt = texture_format((int)e->v[5]);
    if (!fmt.bpb) { NVN_LOG("【红】unsupported texture format %#llx", (unsigned long long)e->v[5]); return 0; }
    nvn_tex_layout l = {0};
    l.width = (uint32_t)e->v[1]; l.height = (uint32_t)e->v[2]; l.depth = (uint32_t)e->v[3];
    l.mip_levels = (uint32_t)e->v[4];
    if (!l.width || !l.height || !l.depth || l.mip_levels > 32) return 0;
    l.block_w = fmt.bw; l.block_h = fmt.bh; l.bytes_per_block = fmt.bpb;
    l.samples_x = l.samples_y = 1; l.layers = 1; l.dims = 2;
    switch (e->v[7]) {
    case 0: l.dims = 1; l.height = l.depth = 1; break;
    case 2: l.dims = 3; break;
    case 3: l.dims = 1; l.layers = l.height; l.height = l.depth = 1; break;
    case 4: case 6: l.layers = l.depth; l.depth = 1; break;
    case 8: l.layers = 6; l.depth = 1; break;
    case 9: l.layers = l.depth * 6; l.depth = 1; break;
    case 10: return (uint64_t)l.width * fmt.bpb;
    default: l.depth = 1; break;
    }
    if (e->v[8] == 2) l.samples_x = 2;
    if (e->v[8] == 4) l.samples_x = l.samples_y = 2;
    if (e->v[8] == 8) { l.samples_x = 4; l.samples_y = 2; }
    l.linear = !!(e->v[6] & 0x10);
    l.stride = (uint32_t)e->v[11];
    if (l.linear && !l.stride) {
        uint32_t align = e->v[6] & 0x40 ? 128 : 32;
        l.stride = (tex_div_ceil(l.width, fmt.bw) * fmt.bpb + align - 1) & ~(align - 1);
        l.height = tex_div_ceil(l.height, fmt.bh);
    }
    if (l.dims == 2) l.tile_h_log2 = nvn_tex_default_tile_h(l.height, l.samples_y, fmt.bh);
    if (l.dims == 3) l.tile_d_log2 = tex_pick_tile_size(l.depth + l.depth / 2);
    return nvn_tex_storage_size(&l);
}
uint64_t nvn_TextureBuilderGetStorageSize(uint64_t obj, uint64_t a1, uint64_t a2, uint64_t a3,
                                         uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    return texture_size(need(ARG_OBJ(obj), NVN_T_TEXB, "TextureBuilderGetStorageSize"));
}
uint64_t nvn_TextureBuilderGetStorageAlignment(uint64_t obj, uint64_t a1, uint64_t a2, uint64_t a3,
                                              uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(obj), NVN_T_TEXB, "TextureBuilderGetStorageAlignment");
    if (!e) return 0;
    texture_fmt fmt = texture_format((int)e->v[5]);
    if (e->v[7] == 10) return fmt.bpb == 12 ? 4 : fmt.bpb;
    return e->v[6] & 0x10 ? (e->v[6] & 0x40 ? 128 : 32) : 512;
}
uint64_t nvn_TextureInitialize(uint64_t obj, uint64_t builder, uint64_t a2, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *b = need(ARG_OBJ(builder), NVN_T_TEXB, "TextureInitialize(builder)");
    uint64_t size = texture_size(b);
    if (!size) {
        NVN_LOG("NVN_TEXTURE_INIT_FAILED object=%p reason=layout format=%llu target=%llu size=%llux%llux%llu levels=%llu",
            ARG_OBJ(obj),(unsigned long long)b->v[5],(unsigned long long)b->v[7],
            (unsigned long long)b->v[1],(unsigned long long)b->v[2],(unsigned long long)b->v[3],(unsigned long long)b->v[4]);
        return NVN_FALSE;
    }
    nvn_st_ent *p = need(ARG_OBJ(b->v[9]), NVN_T_POOL, "TextureInitialize(pool)");
    if (!p || b->v[10] > p->v[POOL_SIZE] || size > p->v[POOL_SIZE] - b->v[10]) {
        NVN_LOG("NVN_TEXTURE_INIT_FAILED object=%p reason=storage format=%llu target=%llu size=%llux%llux%llu levels=%llu bytes=%llu offset=%llu pool_bytes=%llu",
            ARG_OBJ(obj),(unsigned long long)b->v[5],(unsigned long long)b->v[7],
            (unsigned long long)b->v[1],(unsigned long long)b->v[2],(unsigned long long)b->v[3],(unsigned long long)b->v[4],
            (unsigned long long)size,(unsigned long long)b->v[10],(unsigned long long)(p?p->v[POOL_SIZE]:0));
        return NVN_FALSE;
    }
    nvn_st_ent *t = nvn_st_new(ARG_OBJ(obj), NVN_T_TEX);
    if (!t) return NVN_FALSE;
    memcpy(t->v, b->v, sizeof t->v);
    t->v[15] = nvn_st_resource_serial();
    return NVN_TRUE;
}
uint64_t nvn_TextureFinalize(uint64_t obj, uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(obj), NVN_T_TEX); return 0;
}

// ------------------------------------------------------------------ 同步对象
uint64_t nvn_SyncInitialize(uint64_t sync, uint64_t dev, uint64_t a2, uint64_t a3,
                            uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = nvn_st_new(ARG_OBJ(sync), NVN_T_SYNC);
    if (!e) return NVN_FALSE;
    e->v[SYNC_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    e->v[SYNC_SIGNALED] = 0;
    return NVN_TRUE;
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
    if (e) __atomic_store_n(&e->v[SYNC_SIGNALED], 1, __ATOMIC_RELEASE);
}

// NVNsyncWaitResult: already signaled=0, satisfied after waiting=1,
// timeout=2, failed=3. A zero timeout polls; fences stay signaled until rearmed.
static uint64_t sync_now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) NVN_DIE("SyncWait clock unavailable");
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}
static int sync_poll(nvn_st_ent *e) {
    if (__atomic_load_n(&e->v[SYNC_SIGNALED], __ATOMIC_ACQUIRE)) return 1;
    uint64_t sequence = __atomic_load_n(&e->v[2], __ATOMIC_ACQUIRE);
    if (!sequence) return 0;
    return nvn_gpu_poll(sequence);
}
uint64_t nvn_SyncWait(uint64_t sync, uint64_t timeout_ns, uint64_t a2, uint64_t a3,
                      uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(sync), NVN_T_SYNC, "SyncWait");
    if (!e) return 3;
    int status = sync_poll(e);
    if (status < 0) return 3;
    if (status > 0) return 0;
    if (!timeout_ns) return 2;
    uint64_t start = sync_now_ns();
    int reported = 0;
    for (;;) {
        status = sync_poll(e);
        if (status < 0) return 3;
        if (status > 0) return 1;
        uint64_t elapsed = sync_now_ns() - start;
        if (elapsed >= timeout_ns) return 2;
        if (!reported && elapsed >= UINT64_C(1000000000)) {
            NVN_LOG("NVN_FENCE_WAIT sync=%p timeout_ns=%llu; no completion reported",
                    ARG_OBJ(sync), (unsigned long long)timeout_ns);
            reported = 1;
        }
        nvn_host_sleep_us(200);
    }
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

SAMP_FN(nvn_SamplerBuilderSetReductionFilter, SAMP_REDUCTION, ARG_U32(val))

static uint32_t sampler_float_bits(float value) {
    uint32_t bits; memcpy(&bits,&value,sizeof bits); return bits;
}
uint64_t nvn_SamplerBuilderSetMaxAnisotropy(void *builder,float value) {
    nvn_st_ent *s=need(builder,NVN_T_SAMPLERB,"SamplerBuilderSetMaxAnisotropy");
    if (s) s->v[SAMP_MAXANISO]=sampler_float_bits(value);
    return 0;
}
uint64_t nvn_SamplerBuilderSetLodClamp(void *builder,float lo,float hi) {
    nvn_st_ent *s=need(builder,NVN_T_SAMPLERB,"SamplerBuilderSetLodClamp");
    if (s) { s->v[13]=sampler_float_bits(lo); s->v[14]=sampler_float_bits(hi); }
    return 0;
}
static void sampler_border(void *builder,const void *value,uint32_t kind) {
    nvn_st_ent *s=need(builder,NVN_T_SAMPLERB,"SamplerBuilderSetBorderColor");
    if (!s || !value) return;
    uint32_t rgba[4]; memcpy(rgba,value,sizeof rgba);
    for (unsigned i=0;i<4;++i) s->v[SAMP_BORDER+i]=rgba[i];
    s->v[SAMP_BORDER]|=(uint64_t)kind<<32;
}
void nvn_SamplerBuilderSetBorderColor(void *s,const float *rgba) { sampler_border(s,rgba,0); }
void nvn_SamplerBuilderSetBorderColori(void *s,const int32_t *rgba) { sampler_border(s,rgba,1); }
void nvn_SamplerBuilderSetBorderColorui(void *s,const uint32_t *rgba) { sampler_border(s,rgba,2); }

/* Getter return types matter: floating results use s0, never the integer result register. */
#define SAMPLER_GETTERS(prefix, type) \
void nvn_##prefix##GetMinMagFilter(const void *obj,uint32_t *min,uint32_t *mag) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetMinMagFilter"); \
    if(s){if(min)*min=(uint32_t)s->v[1];if(mag)*mag=(uint32_t)(s->v[1]>>32);} } \
void nvn_##prefix##GetWrapMode(const void *obj,uint32_t *u,uint32_t *v,uint32_t *w) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetWrapMode"); \
    if(s){if(u)*u=(uint32_t)s->v[2];if(v)*v=(uint32_t)s->v[3];if(w)*w=(uint32_t)s->v[4];} } \
void nvn_##prefix##GetCompare(const void *obj,uint32_t *mode,uint32_t *func) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetCompare"); \
    if(s){if(mode)*mode=(uint32_t)s->v[7];if(func)*func=(uint32_t)(s->v[7]>>32);} } \
void nvn_##prefix##GetLodClamp(const void *obj,float *lo,float *hi) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetLodClamp"); \
    if(s){uint32_t l=(uint32_t)s->v[13],h=(uint32_t)s->v[14];if(lo)memcpy(lo,&l,4);if(hi)memcpy(hi,&h,4);} } \
float nvn_##prefix##GetLodBias(const void *obj) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetLodBias"); \
    uint32_t bits=s?(uint32_t)s->v[5]:0;float v;memcpy(&v,&bits,4);return v; } \
float nvn_##prefix##GetMaxAnisotropy(const void *obj) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetMaxAnisotropy"); \
    uint32_t bits=s?(uint32_t)s->v[6]:0;float v;memcpy(&v,&bits,4);return v; } \
uint32_t nvn_##prefix##GetReductionFilter(const void *obj) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetReductionFilter");return s?(uint32_t)s->v[8]:0; } \
void nvn_##prefix##GetBorderColor(const void *obj,void *rgba) { \
    nvn_st_ent *s=need((void *)obj,type,#prefix "GetBorderColor"); \
    if(s&&rgba){uint32_t v[4];for(unsigned i=0;i<4;++i)v[i]=(uint32_t)s->v[9+i];memcpy(rgba,v,16);} } \
void nvn_##prefix##GetBorderColori(const void *obj,void *rgba) { nvn_##prefix##GetBorderColor(obj,rgba); } \
void nvn_##prefix##GetBorderColorui(const void *obj,void *rgba) { nvn_##prefix##GetBorderColor(obj,rgba); }
SAMPLER_GETTERS(SamplerBuilder,NVN_T_SAMPLERB)
SAMPLER_GETTERS(Sampler,NVN_T_SAMPLER)
#undef SAMPLER_GETTERS

uint64_t nvn_SamplerBuilderSetMinMagFilter(void *builder,uint32_t min,uint32_t mag) {
    nvn_st_ent *s=nvn_st_get(builder,NVN_T_SAMPLERB);
    if (!s) NVN_DIE("sampler builder not initialized");
    s->v[SAMP_MINMAG]=min|((uint64_t)mag<<32);
    return 0;
}
uint64_t nvn_SamplerBuilderSetWrapMode(void *builder,uint32_t u,uint32_t v,uint32_t w) {
    nvn_st_ent *s=nvn_st_get(builder,NVN_T_SAMPLERB);
    if (!s) NVN_DIE("sampler builder not initialized");
    s->v[SAMP_WRAPS]=u; s->v[SAMP_WRAPT]=v; s->v[SAMP_WRAPR]=w;
    return 0;
}
uint64_t nvn_SamplerBuilderSetCompare(void *builder,uint32_t mode,uint32_t function) {
    nvn_st_ent *s=nvn_st_get(builder,NVN_T_SAMPLERB);
    if (!s) NVN_DIE("sampler builder not initialized");
    s->v[SAMP_COMPARE]=mode|((uint64_t)function<<32);
    return 0;
}

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
    nvn_st_ent *old=nvn_st_get(ARG_OBJ(s),NVN_T_SAMPLERB);
    uint64_t dev=old?old->v[SAMP_DEV]:0;
    nvn_st_ent *e=nvn_st_new(ARG_OBJ(s),NVN_T_SAMPLERB);
    if (!e) return 0;
    e->v[SAMP_DEV]=dev;
    e->v[SAMP_MINMAG]=4|((uint64_t)1<<32);
    e->v[SAMP_WRAPS]=e->v[SAMP_WRAPT]=e->v[SAMP_WRAPR]=1;
    e->v[SAMP_MAXANISO]=sampler_float_bits(1.f);
    e->v[SAMP_COMPARE]=(uint64_t)2<<32; /* LESS; comparison disabled. */
    e->v[13]=sampler_float_bits(-1000.f);
    e->v[14]=sampler_float_bits(1000.f);
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
    if (!b || !e) return NVN_FALSE;
    memcpy(e->v,b->v,sizeof e->v);
    e->v[15] = nvn_st_resource_serial();
    return NVN_TRUE;
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
    if (!e) return NVN_FALSE;
    e->v[PROG_DEV] = (uint64_t)(uintptr_t)ARG_OBJ(dev);
    e->v[15] = nvn_st_resource_serial();
    return NVN_TRUE;
}

uint64_t nvn_ProgramFinalize(uint64_t prog, uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_del(ARG_OBJ(prog), NVN_T_PROG);
    return 0;
}

// nvnProgramSetShaders(program, int count, const NVNshaderData *stages).
// Copy the descriptors: the caller's temporary array may disappear on return.
static unsigned g_shader_sets;
uint64_t nvn_ProgramSetShaders(uint64_t prog, uint64_t count, uint64_t stages, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    nvn_st_ent *e = need(ARG_OBJ(prog), NVN_T_PROG, "ProgramSetShaders");
    if (!e) return NVN_FALSE;
    int n = (int32_t)count;
    if (n < 0 || n > 6 || (n && !stages)) return NVN_FALSE;
    e->v[NVN_PROG_NSTAGES] = (uint64_t)n;
    memset(&e->v[NVN_PROG_STAGE_BASE], 0, 6 * sizeof(nvn_shader_data));
    if (n) memcpy(&e->v[NVN_PROG_STAGE_BASE], ARG_OBJ(stages), (size_t)n * sizeof(nvn_shader_data));
    e->v[15] = nvn_st_resource_serial();
    g_shader_sets++;
    if (g_shader_sets <= 3) {
        NVN_LOG("ProgramSetShaders stages=%d first_data=%#llx", n,
                (unsigned long long)e->v[NVN_PROG_STAGE_BASE]);
    }
    if (g_shader_sets<=24 && nvn_shader_count()) {
        for (int i=0;i<n;++i) {
            uint64_t address=e->v[NVN_PROG_STAGE_BASE+2*i];
            nvn_shader_match match;
            const nvn_shader_ent *shader=nvn_shader_find_gpu(address,&match);
            NVN_LOG("NVN_SHADER_MATCH hit=%d stage=%u offset=%u code_only=%u bytes=%u candidates=%u matches=%u",
                    shader!=NULL,shader?shader->stage:UINT16_MAX,match.offset,match.code_only,
                    match.bytes,match.candidates,match.matches);
        }
    }
    return NVN_TRUE;
}

unsigned nvn_shader_sets(void) { return g_shader_sets; }
