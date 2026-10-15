// 命令重放的第一层：把记录下来的 NVN 调用**重建**成「绘制计划」。
//
// 这一层刻意不含任何 Metal 调用 —— 所以能在本机测。它做的是记账：
//   · 按调用名分类（绑定 / 设置 / 绘制 / 分派 / 清屏 / 拷贝 / 呈现）
//   · 每次绑定都在宿主侧状态表里查对象（查不到就是【红】：说明游戏绑了一个没 Initialize 的对象，
//     或者我们的状态层漏了某个 Initialize —— 两种都是必须当场暴露的问题）
//   · 每次绘制记一条计划项：当时的 program（以及它记下的控制段）、绘制类型、参数
// Metal 层接上后就是拿这些计划项去发 MTLRenderCommandEncoder 的调用。
#include "nvn_host.h"
#include "nvn_state.h"
#include "nvn_shader.h"

// 名字前缀分类（不去解析每个名字：这里只关心"这属于哪一类动作"）
enum { ACT_OTHER = 0, ACT_BIND, ACT_SET, ACT_DRAW, ACT_DISPATCH, ACT_CLEAR, ACT_COPY, ACT_PRESENT, ACT_SYNC };

#define PLAN_MAX 65536
static nvn_draw_item g_plan[PLAN_MAX];
static uint64_t      g_keys[PLAN_MAX];
static unsigned g_plan_n;
static unsigned g_binds, g_sets, g_draws, g_dispatches, g_clears, g_copies, g_presents, g_syncs;
static unsigned g_unresolved;                 // 绑定了状态表里没有的对象 = 记账出错
static const char *g_unresolved_name;
static uint64_t   g_cur_program;
// 「状态哈希」：把绑定/设置类调用按顺序混进来。管线键 = 状态哈希 + 当前 program 的着色器键。
// 为什么这么做而不是逐字段解析状态：字段语义要靠真机日志逐个确认，但**键只需要"状态变了键就变"**——
// 混哈希就能满足这一点，而且现在就能用、不会因为某个字段解析错而悄悄把两条不同状态的绘制合成一条管线。
static uint64_t   g_state_hash = 1469598103934665603ULL;

static void mix(uint64_t *h, const void *p, size_t n) {
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) { *h ^= b[i]; *h *= 1099511628211ULL; }
}

static uint16_t classify(const char *n) {
    // 名字形如 nvnCommandBufferXxx / nvnQueueXxx / nvnWindowXxx
    const char *p = strstr(n, "CommandBuffer");
    if (!p) {
        if (strstr(n, "QueueSubmitCommands")) return ACT_PRESENT;
        if (strstr(n, "Window")) return ACT_SET;
        return ACT_OTHER;
    }
    p += 13;
    struct { const char *pfx; uint16_t act; } tab[] = {
        { "Bind", ACT_BIND }, { "Set", ACT_SET }, { "Draw", ACT_DRAW },
        { "Dispatch", ACT_DISPATCH }, { "Clear", ACT_CLEAR }, { "Copy", ACT_COPY },
        { "Present", ACT_PRESENT }, { "Barrier", ACT_SYNC }, { "Signal", ACT_SYNC }, { "Wait", ACT_SYNC },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (!strncmp(p, tab[i].pfx, strlen(tab[i].pfx))) return tab[i].act;
    return ACT_OTHER;
}

// 绑定类调用的对象参数位置：绝大多数是 a1（a0 = command buffer）
static uint64_t bind_object(const char *n, uint64_t a1) {
    if (!strcmp(n, "nvnCommandBufferBindProgram")) return a1;
    return a1;
}

uint64_t nvn_replay_item_key(unsigned i) { return i < g_plan_n ? g_keys[i] : 0; }

void nvn_replay_reset(void) {
    g_state_hash = 1469598103934665603ULL;
    g_plan_n = g_binds = g_sets = g_draws = g_dispatches = g_clears = g_copies = g_presents = g_syncs = 0;
    g_unresolved = 0;
    g_unresolved_name = NULL;
    g_cur_program = 0;
}

// 处理一条记录。返回 1 表示产生了计划项（绘制/分派）。
int nvn_replay_call(const char *name, const uint64_t *a) {
    uint16_t act = classify(name);
    switch (act) {
    case ACT_BIND: {
        g_binds++;
        mix(&g_state_hash, name, strlen(name));
        mix(&g_state_hash, &a[1], 8);
        uint64_t obj = bind_object(name, a[1]);
        // 绑定的是 program 时记下来（绘制计划要带上它）
        if (strstr(name, "Program")) {
            if (!nvn_st_get((void *)(uintptr_t)obj, 10 /* NVN_T_PROG */)) {
                g_unresolved++; if (!g_unresolved_name) g_unresolved_name = name;
            }
            g_cur_program = obj;
        }
        // 别的绑定对象（pool/texture/sampler/buffer）不强求在表里：有的走的是 pool 里的句柄
        return 0;
    }
    case ACT_SET:
        g_sets++;
        mix(&g_state_hash, name, strlen(name));
        mix(&g_state_hash, a, 8 * sizeof a[0]);
        return 0;
    case ACT_SYNC:
        g_syncs++;
        mix(&g_state_hash, name, strlen(name));
        return 0;
    case ACT_CLEAR: g_clears++; return 0;
    case ACT_COPY: g_copies++; return 0;
    case ACT_PRESENT: g_presents++; return 0;
    case ACT_DRAW:
    case ACT_DISPATCH: {
        if (act == ACT_DRAW) g_draws++; else g_dispatches++;
        if (g_plan_n < PLAN_MAX) {
            nvn_draw_item *it = &g_plan[g_plan_n++];
            memset(it, 0, sizeof *it);
            it->act = act;
            it->program = g_cur_program;
            it->a0 = a[0]; it->a1 = a[1]; it->a2 = a[2];
            it->name = name;
            // 管线键：状态哈希 + 当前 program 的着色器键（Metal 层按它缓存 MTLRenderPipelineState）
            uint64_t k = g_state_hash;
            nvn_st_ent *pp = nvn_st_get((void *)(uintptr_t)g_cur_program, 10);
            if (pp && pp->v[1]) {
                const nvn_shader_ent *se2 = nvn_shader_find_control((const uint8_t *)(uintptr_t)pp->v[1], (size_t)pp->v[2]);
                uint64_t sk = se2 ? se2->key : 0;
                mix(&k, &sk, 8);
            }
            mix(&k, &it->program, 8);
            if (g_plan_n < PLAN_MAX) g_keys[g_plan_n - 1] = k;
            // 程序的着色器阶段：从状态表里 program 记下的控制段查索引（SetShaders 时记的）
            nvn_st_ent *pe = nvn_st_get((void *)(uintptr_t)g_cur_program, 10);
            if (pe && pe->v[1]) {
                const nvn_shader_ent *se = nvn_shader_find_control((const uint8_t *)(uintptr_t)pe->v[1], (size_t)pe->v[2]);
                it->stage_hint = se ? se->stage : 0xFFFF;      // 0xFFFF = 没查到（真机上要报出来）
            } else {
                it->stage_hint = 0xFFFE;                        // 0xFFFE = 当时还没 SetShaders
            }
        }
        return 1;
    }
    default: return 0;
    }
}

unsigned nvn_replay_count(void)      { return g_plan_n; }
const nvn_draw_item *nvn_replay_item(unsigned i) { return i < g_plan_n ? &g_plan[i] : NULL; }
unsigned nvn_replay_draws(void)      { return g_draws; }
unsigned nvn_replay_dispatches(void) { return g_dispatches; }
unsigned nvn_replay_binds(void)      { return g_binds; }
unsigned nvn_replay_unresolved(void) { return g_unresolved; }
const char *nvn_replay_unresolved_name(void) { return g_unresolved_name; }
void nvn_replay_stats(unsigned *binds, unsigned *sets, unsigned *draws, unsigned *dispatches,
                      unsigned *clears, unsigned *copies, unsigned *presents, unsigned *syncs) {
    *binds = g_binds; *sets = g_sets; *draws = g_draws; *dispatches = g_dispatches;
    *clears = g_clears; *copies = g_copies; *presents = g_presents; *syncs = g_syncs;
}
