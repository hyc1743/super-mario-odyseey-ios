// HLE 层的接线：强制接管表查询 + 未实现导入的记名桩。
#include "hle.h"

// ------------------------------------------------------------------ 记名桩

static const char *g_stub_name[MAX_STUB];
static unsigned g_stub_n;
static unsigned g_stub_hits[MAX_STUB];
static unsigned g_stub_total;

// 登记一个没有实现的名字；装载器在解析 import 时对「无人提供」的名字调它。
// 返回桩号：entry.S 里第 k 个桩就是 movz x9,#k; b unimpl_entry。桩号 0 留作「表满」的哨兵。
unsigned unimpl_register(const char *name) {
    for (unsigned i = 0; i < g_stub_n; i++) if (!strcmp(g_stub_name[i], name)) return i + 1;
    if (g_stub_n >= MAX_STUB - 1) return 0;
    g_stub_name[g_stub_n++] = name;
    return g_stub_n;                       // 1 起编，0 = 表满
}

// 桩入口（entry.S 调进来）：x9 = 桩号，regs = 保存下来的 x0..x8
void unimpl_c(unsigned idx, const uint64_t *regs) {
    (void)regs;
    if (idx >= 1 && idx <= g_stub_n) {
        if (!g_stub_hits[idx - 1]++) {
            LOG("UNIMPL %s（第 %u 个未实现导入，首次调用）", g_stub_name[idx - 1], idx);
        }
        g_stub_total++;
    }
}

void unimpl_report(void) {
    unsigned used = 0;
    for (unsigned i = 0; i < g_stub_n; i++) if (g_stub_hits[i]) used++;
    LOG("未实现导入：登记 %u 个，运行中被调用过 %u 个（调用合计 %u 次）", g_stub_n, used, g_stub_total);
    for (unsigned i = 0; i < g_stub_n; i++)
        if (g_stub_hits[i]) LOG("  被调过：%s ×%u", g_stub_name[i], g_stub_hits[i]);
}

// ------------------------------------------------------------------ 强制接管表

// 名字哈希（与装载器的符号表同一套 FNV-1a），开放寻址
#define FORCE_CAP 4096
static void *g_fn[FORCE_CAP];
static const char *g_nm[FORCE_CAP];
static int g_force_ready;

// ---- 真实现表：与生成的桩分开，查询时**优先** ----
#define IMPL_CAP 2048
static void *g_ifn[IMPL_CAP];
static const char *g_inm[IMPL_CAP];
static unsigned g_impl_n;

static unsigned hash_slot(const char *name, unsigned cap) {
    uint32_t h = 2166136261u;
    for (const char *p = name; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    return h & (cap - 1);
}

void hle_impl_register(const hle_impl_ent *ents, unsigned n) {
    for (unsigned i = 0; i < n; i++) {
        unsigned j = hash_slot(ents[i].name, IMPL_CAP);
        while (g_inm[j]) {
            if (!strcmp(g_inm[j], ents[i].name)) die("真实现重复登记：%s", ents[i].name);
            j = (j + 1) & (IMPL_CAP - 1);
        }
        g_inm[j] = ents[i].name;
        g_ifn[j] = ents[i].fn;
        g_impl_n++;
    }
}
unsigned hle_impl_count(void) { return g_impl_n; }

uint64_t dm_hle_force_lookup(const char *name) {
    // ① 真实现优先
    for (unsigned j = hash_slot(name, IMPL_CAP); g_inm[j]; j = (j + 1) & (IMPL_CAP - 1))
        if (!strcmp(g_inm[j], name)) return (uint64_t)(uintptr_t)g_ifn[j];
    if (!g_force_ready) {
        // 一遍装表：同名重复直接报错（清单写错要响亮，与参考项目一致）
        for (unsigned i = 0; i < g_hle_force_n; i++) {
            uint32_t h = 2166136261u;
            for (const char *p = g_hle_force[i].name; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
            unsigned j = h & (FORCE_CAP - 1);
            while (g_nm[j]) {
                if (!strcmp(g_nm[j], g_hle_force[i].name)) die("强制接管清单重复：%s", g_hle_force[i].name);
                j = (j + 1) & (FORCE_CAP - 1);
            }
            g_nm[j] = g_hle_force[i].name;
            g_fn[j] = g_hle_force[i].fn;
        }
        g_force_ready = 1;
        // 真实现必须是强制接管清单的子集：不是的话说明清单漏了它（清单写错要响亮）
        unsigned missing = 0;
        for (unsigned j = 0; j < IMPL_CAP; j++)
            if (g_inm[j] && !dm_hle_force_lookup(g_inm[j])) { LOG("【红】真实现不在接管清单里：%s", g_inm[j]); missing++; }
        if (missing) die("有 %u 条真实现没登记在 hle_imports.tsv 生成的清单里", missing);
        LOG("强制接管表：%u 条（其中真实现 %u 条）", g_hle_force_n, g_impl_n);
    }
    uint32_t h = 2166136261u;
    for (const char *p = name; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    for (unsigned j = h & (FORCE_CAP - 1); g_nm[j]; j = (j + 1) & (FORCE_CAP - 1)) {
        if (!strcmp(g_nm[j], name)) return (uint64_t)(uintptr_t)g_fn[j];
    }
    return 0;
}

// 装载器接口：先查强制接管表；没有则登记成记名桩
uint64_t dm_hle_force(const char *name) { return dm_hle_force_lookup(name); }

uint64_t resolve_import(module_t *m, const char *name) {
    unsigned idx = unimpl_register(name);
    if (!idx) die("%s: 未实现导入表满了（MAX_STUB=%d）：%s", m->name, MAX_STUB, name);
    LOG("import 接到桩：%s（第 %u 个，来自 %s）", name, idx, m->name);
    return (uint64_t)(uintptr_t)((const char *)unimpl_stubs + (idx - 1) * 8);   // 每桩 8 字节：movz + b
}

// ==================================================================================
// 宿主门分派：改写器把 `svc #n` 与 `mrs xN, cntpct_el0` 换成「bl dm_gate」。
// dm_gate（entry.S）把寄存器全存下来，用返回点当门标识调进来；这里按返回点查门表，
// 再分派到 svc 层或计时器。
// ==================================================================================

#include "guest_clock.h"

// ---- 门表：以返回点为键（改写产物的 gates.tsv 一行一条）----
#define GATE_CAP 4096                                  // SMO 实测 139 个门，留足余量
typedef struct { uint64_t ret; char kind[12]; int arg; uint64_t orig; } gate_ent;
static gate_ent g_gate[GATE_CAP];
static unsigned g_gate_n;

void gate_register(uint64_t ret_addr, const char *kind, int arg, uint64_t orig) {
    if (g_gate_n >= GATE_CAP) die("门表满了（%d）", GATE_CAP);
    gate_ent *g = &g_gate[g_gate_n++];
    g->ret = ret_addr;
    snprintf(g->kind, sizeof g->kind, "%s", kind);
    g->arg = arg;
    g->orig = orig;
}

static const gate_ent *gate_find(uint64_t ret) {
    for (unsigned i = 0; i < g_gate_n; i++) if (g_gate[i].ret == ret) return &g_gate[i];
    return NULL;
}

// ---- 客体时钟：Horizon 的 cntpct 是 19.2 MHz 系统计数器（参考项目同口径）----

// svc 实现在 hle_svc.c（内存族）与 hle_thread.c（线程族）；这里只做门分派
int svc_dispatch(unsigned num, uint64_t *regs);

// 门入口（entry.S 的 dm_gate 调进来）：ret = 门块的返回点，regs = 保存下来的 x0..x30
uint64_t gate_dispatch(uint64_t ret, uint64_t *regs) {
    const gate_ent *g = gate_find(ret);
    if (!g) die("门返回点 %#llx 不在门表里（产物与宿主不匹配）", (unsigned long long)ret);
    if (!strcmp(g->kind, "svc")) return svc_dispatch((unsigned)g->arg, regs);
    if (!strcmp(g->kind, "cntpct") || !strcmp(g->kind, "cntvct")) {
        regs[g->arg & 31] = dm_guest_tick();             // 目标寄存器可以是任意 xN（实测含 x20/x21/x23）
        return 0;
    }
    die("未知门种类 %s", g->kind);
    return 0;
}
