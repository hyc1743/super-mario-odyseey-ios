// NVN 接口层：接管 nvnBootstrapLoader，按名字返回函数地址。
// 派生自参考项目 d2r-mobile 的 dm/gfx/nvn/hle_nvn.c（MIT），本阶段只做「接口层骨架 + 记录」，
// 状态实现在下一批（需要按 NVN 的真实结构布局落字段，见 gfx/README.md）。
//
// 为什么这条路口成立（第 2.3 节实测）：游戏的 main **只 import 一个图形入口 nvnBootstrapLoader**，
// 其余全部函数按名字向它索取。宿主接管这一个入口，就等于接管了整个图形接口 —— SDK 里 NVN 驱动的
// 指令一条都不会执行。
//
// 统一签名：所有 NVN 函数都按 (a0..a7) → u64 收，理由见 nvn_host.h。参数按真实签名解释的工作
// 留给各函数自己的实现；在只有记录桩的阶段，参数原样进记录。
#include "nvn_host.h"
#include <stdarg.h>

static void (*g_log_handler)(const char *);
void nvn_set_log_handler(void (*handler)(const char *)) { g_log_handler = handler; }
void nvn_host_log(const char *fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    if (g_log_handler) g_log_handler(message);
    else fprintf(stderr, "[nvn] %s\n", message);
}

// ---------------------------------------------------------------- 命令记录（Metal 重放的接缝）
// 现在只记不重放：一条记录 = 函数名指针 + 8 个参数。重放层接上后从这里取。
#define CMD_MAX (1u << 22)
typedef struct { const char *name; uint64_t a[8]; } cmd_t;
static cmd_t *g_cmds;
static unsigned g_cmd_n;

void nvn_record_append(const char *name, const uint64_t *a) {
    if (!g_cmds) g_cmds = calloc(CMD_MAX, sizeof *g_cmds);
    if (g_cmd_n >= CMD_MAX) { NVN_LOG("命令记录满了（%u）", CMD_MAX); return; }
    cmd_t *c = &g_cmds[g_cmd_n++];
    c->name = name;
    memcpy(c->a, a, sizeof c->a);
}
unsigned nvn_record_count(void) { return g_cmd_n; }
void nvn_record_reset(void) { g_cmd_n = 0; }

// 见过的名字（首次调用打印一行；之后只计数）
#define SEEN_MAX 512
static const char *g_seen[SEEN_MAX];
static unsigned g_seen_n;

static int seen(const char *name) {
    for (unsigned i = 0; i < g_seen_n; i++) if (g_seen[i] == name) return 1;
    if (g_seen_n < SEEN_MAX) g_seen[g_seen_n++] = name;
    return 0;
}

uint64_t nvn_record_call(const char *name, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    uint64_t a[8] = { a0, a1, a2, a3, a4, a5, a6, a7 };
    nvn_record_append(name, a);
    nvn_replay_call(name, a);            // 记账：绑定/设置/绘制/分派各自归类（见 nvn_replay.c）
    if (!seen(name)) NVN_LOG("记录 %s（首个未逐字段实现的调用）", name);
    return 0;
}

// 记录桩的实现：由生成器列名字，这里一条宏全部展开（统一签名下不需要逐个人工写）
#include "nvn_record_names.inc"

// ---------------------------------------------------------------- 未实现
static const char *g_unimpl[SEEN_MAX];
static unsigned g_unimpl_n;
static unsigned long long g_unimpl_calls;

uint64_t nvn_unimplemented(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    g_unimpl_calls++;
    // 未实现的名字没法从参数里认出来（NVN 不传名字），只报总数与首个调用的次数
    if (g_unimpl_n < SEEN_MAX && g_unimpl_calls == 1) { g_unimpl[0] = "（首个未实现调用）"; g_unimpl_n = 1; }
    if (g_unimpl_calls == 1) NVN_LOG("未实现的 NVN 调用（第一次；总数会在收尾时报出）");
    return 0;
}
unsigned nvn_unimpl_kinds(void) { return g_unimpl_n; }
unsigned long long nvn_unimpl_calls(void) { return g_unimpl_calls; }

// ---------------------------------------------------------------- bootstrap
const nvn_ent *nvn_lookup_ent(const char *name);

const nvn_ent *nvn_lookup_ent(const char *name) {
    for (unsigned i = 0; i < g_nvn_table_n; i++)
        if (!strcmp(g_nvn_table[i].name, name)) return &g_nvn_table[i];
    return NULL;
}

// 对外：名字 → 统一签名的函数指针（nvn_core.c 的 DeviceGetProcAddress 用它）
nvn_fn nvn_lookup(const char *name) {
    const nvn_ent *e = nvn_lookup_ent(name);
    return e ? e->fn : (nvn_fn)0;
}

uint64_t nvn_BootstrapLoader(const char *name) {
    static int said;
    const nvn_ent *e = nvn_lookup_ent(name);
    if (!e) { NVN_LOG("nvnBootstrapLoader：游戏要的名字不在表里：%s", name); return 0; }
    if (!said) {
        said = 1;
        NVN_LOG("nvnBootstrapLoader 已接管：表内 %u 个名字（已实现 %u，其余接未实现入口）",
                g_nvn_table_n, g_nvn_impl_n);
    }
    return (uint64_t)(uintptr_t)e->fn;
}
