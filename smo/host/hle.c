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

uint64_t dm_hle_force_lookup(const char *name) {
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
        LOG("强制接管表：%u 条", g_hle_force_n);
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
