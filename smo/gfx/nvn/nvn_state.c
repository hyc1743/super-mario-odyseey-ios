// NVN 状态表：客体对象指针 → 宿主侧状态。
//
// 设计要点（为什么用宿主侧表、而不往客体对象内存里写）：
//   1. 客体对象的大小/字段布局取决于**游戏编译时的 NVN 版本**，我们手上只有第三方重建的
//      nvn.h（从别的游戏的 DWARF 重建），版本未必一致。往客体对象里写字段，就有踩出分配范围的风险。
//   2. 反过来，状态全部放宿主侧、用对象指针做键，就**完全不依赖任何结构布局** —— 客体对象被当成
//      不透明句柄，宿主一个字节都不写它（唯一的例外是「出参指针」，那种宽度要靠调用点定，另说）。
//   3. 代价：需要一张表 + 类型检查。NVN 对象数量是几百个量级，开放寻址足够。
// 同址异类（对象在栈上被复用）靠 `type` 挡：Initialize 时按新类型重建槽，取用时类型不符即报错。
#include "nvn_host.h"
#include "nvn_state.h"

#define NVN_ST_MAX 1024          // 2 的幂，掩码取模；NVN 对象数百个，1024 够

static nvn_st_ent g_st[NVN_ST_MAX];
static unsigned   g_st_n;
static unsigned   g_st_reused;   // 同址异类的次数：不为 0 说明有对象在复用地址

static unsigned st_hash(const void *p) {
    uint64_t h = (uint64_t)(uintptr_t)p;
    h ^= h >> 33;                // 指针对齐得厉害，先混一下低位
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return (unsigned)h & (NVN_ST_MAX - 1);
}

// 返回「该键应落」的槽（已存在则返回它，否则返回第一个空位）
static nvn_st_ent *st_probe(const void *p) {
    unsigned i = st_hash(p);
    for (unsigned k = 0; k < NVN_ST_MAX; k++) {
        nvn_st_ent *e = &g_st[(i + k) % NVN_ST_MAX];
        if (!e->used || e->key == p) return e;
    }
    return NULL;                 // 表满
}

nvn_st_ent *nvn_st_new(void *obj, uint32_t type) {
    if (!obj) return NULL;
    nvn_st_ent *e = st_probe(obj);
    if (!e) { NVN_LOG("NVN 状态表满（%u）", NVN_ST_MAX); return NULL; }
    if (e->used && e->type != type) g_st_reused++;
    if (!e->used) { e->used = 1; e->key = obj; g_st_n++; }
    e->type = type;
    for (unsigned i = 0; i < 12; i++) e->v[i] = 0;
    return e;
}

nvn_st_ent *nvn_st_get(const void *obj, uint32_t type) {
    if (!obj) return NULL;
    nvn_st_ent *e = st_probe(obj);
    if (!e || !e->used || e->type != type) return NULL;
    return e;
}

void nvn_st_del(void *obj, uint32_t type) {
    nvn_st_ent *e = nvn_st_get(obj, type);
    if (!e) return;
    e->used = 0;
    e->key = NULL;
    g_st_n--;
    // 开放寻址删洞会让后面的项查找链断裂。与其写后移/墓碑（都容易错），直接整表重排：
    // 对象是几百个量级、删除很少发生，这点开销完全无所谓。
    static nvn_st_ent tmp[NVN_ST_MAX];
    unsigned n = 0;
    for (unsigned i = 0; i < NVN_ST_MAX; i++)
        if (g_st[i].used) tmp[n++] = g_st[i];
    for (unsigned i = 0; i < NVN_ST_MAX; i++) { g_st[i].used = 0; g_st[i].key = NULL; }
    g_st_n = 0;
    for (unsigned i = 0; i < n; i++) {
        nvn_st_ent *d = st_probe(tmp[i].key);
        *d = tmp[i];
        g_st_n++;
    }
}

unsigned nvn_st_count(void)  { return g_st_n; }
unsigned nvn_st_reused(void) { return g_st_reused; }
void nvn_st_reset(void) {
    for (unsigned i = 0; i < NVN_ST_MAX; i++) { g_st[i].used = 0; g_st[i].key = NULL; }
    g_st_n = 0;
    g_st_reused = 0;
}

// 遍历（Metal 重放层要按类型枚举对象）；i 从 0 递增，返回 NULL 表示结束
void *nvn_st_iter(unsigned i, uint32_t *type) {
    unsigned seen = 0;
    for (unsigned k = 0; k < NVN_ST_MAX; k++) {
        if (!g_st[k].used) continue;
        if (seen == i) { *type = g_st[k].type; return g_st[k].key; }
        seen++;
    }
    return NULL;
}
