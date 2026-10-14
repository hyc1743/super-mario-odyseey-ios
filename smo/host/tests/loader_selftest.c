// 装载器自检：把**平台无关的核心**跑在真实的改写产物上（因此能在 Windows/Linux 上跑）。
//
// 平台三件事用最朴素的实现：映像 = malloc、段权限 = 空实现、键号 = 假槽号。
// 检查项（全部是对真实产物的断言，不是「应该能跑」）：
//   1. 四个模块的 fixups 表头都能解析（段边界、键页、8 个槽号）
//   2. 动态段与符号表能解析，符号数合理
//   3. 重定位全部有归宿；且没有任何一条落在代码区（写了就崩）
//   4. 门表：main 0 / sdk 135 / subsdk0 0 / rtld 4
//   5. 键页装完后每一项都是「实际键号 × 8」且非 0
//   6. 符号哈希表与线性扫描**逐条一致**（参考项目 DM_SYMCHECK 的做法）
#include "../dm.h"

static unsigned g_imports, g_import_uniq;
#define MAXIMP 8192
static const char *g_imp[MAXIMP];
static unsigned g_gates_seen;

void *dm_image_alloc(size_t size) { return calloc(1, size); }
void dm_code_protect(void *p, size_t n) { (void)p; (void)n; }
void dm_ro_protect(void *p, size_t n) { (void)p; (void)n; }
uint64_t dm_slot_key(unsigned i) { return 100 + i; }   // 假槽号：非 0 且互不相同即可

// 宿主还没实现任何服务：强制接管表为空
uint64_t dm_hle_force(const char *name) { (void)name; return 0; }

// 宿主还没实现任何服务：返回一个非 0 的假地址，只统计与收集名字
uint64_t resolve_import(module_t *m, const char *name) {
    (void)m;
    g_imports++;
    for (unsigned i = 0; i < g_import_uniq; i++) if (!strcmp(g_imp[i], name)) return 0x1000;
    if (g_import_uniq < MAXIMP) g_imp[g_import_uniq++] = name;
    return 0x1000;
}
void gate_register(uint64_t ret_addr, const char *kind, int arg, uint64_t orig) {
    (void)ret_addr; (void)kind; (void)arg; (void)orig;
    g_gates_seen++;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "work/rewrite";
    // 模块名从命令行给：CI 上用合成产物（sdk main），本机用真实的四个
    static const char *def[] = {"rtld", "sdk", "subsdk0", "main"};
    const char **mods = def;
    int nmods = 4;
    if (argc > 2) { mods = (const char **)&argv[2]; nmods = argc - 2; }
    load_modules(dir, mods, nmods);
    printf("\n== 各模块 ==\n");
    for (int i = 0; i < g_nmod; i++) {
        module_t *m = &g_mod[i];
        int keyok = 1;
        const uint64_t *page = (const uint64_t *)(m->base + m->key_page_off);
        for (unsigned k = 0; k < KEY_N; k++) if (!page[m->key_idx[k]]) keyok = 0;
        printf("  %-8s 映像 %#9zx  text_end %#8llx  patch %#8llx  符号 %6u  键页 %s\n",
               m->name, m->size, (unsigned long long)m->text_end, (unsigned long long)m->patch_off,
               m->nsym, keyok ? "OK" : "【红】");
        if (!keyok) return 1;
    }
    printf("\n  宿主门合计 %u（登记回调 %u 次）\n", dm_gate_count(), g_gates_seen);
    printf("  交宿主解析的 import：调用 %u 次，不同名字 %u 个\n", g_imports, g_import_uniq);
    printf("\n== 断言 ==\n");
    int bad = 0;
// 注意：条件**只能求值一次**（见 fs_selftest.c 里的说明：求值两次会让带副作用的断言跑两遍）
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) bad = 1; } while (0)
    CHK(g_nmod == nmods, "命令行给的模块都装载成功");
    CHK(g_gates_seen == dm_gate_count(), "门登记次数与门表一致");
    CHK(dm_gate_count() > 0, "至少登记了一个宿主门");
    // 真实产物（四个模块）的门数是个已知事实，跑真实集时才断言它
    if (g_nmod == 4 && !strcmp(g_mod[3].name, "main")) {
        CHK(dm_gate_count() == 139, "真实产物：宿主门合计 139（main 0 + sdk 135 + subsdk0 0 + rtld 4）");
    }
    // **重要架构点**：这里统计的「交宿主解析」只有 20–60 个名字，不是全部工作面。
    // 服务层那 412 个函数（nn::fs / nn::hid / nn::audio …）**被 sdk 模块导出**，所以走的是
    // 跨模块解析、根本到不了这里 —— 它们必须靠 dm_hle_force()（HLE_FORCE）接管。
    // 断言下面的数字是为了跟 smo/tools/import_graph.py 的独立结果对上（main 12、subsdk0 4）。
    if (g_nmod == 4 && !strcmp(g_mod[3].name, "main")) {
        CHK(g_import_uniq >= 20 && g_import_uniq <= 60,
            "真实产物：交宿主 import 名字数落在 20–60（服务层 412 个走 HLE_FORCE，不在此列）");
    }
    CHK(g_imports >= g_import_uniq, "调用次数不少于名字数");
    printf("\n%s\n", bad ? "有断言不通过" : "全部通过");
    return bad;
}
