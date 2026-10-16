// 装载器自检：把**平台无关的核心**跑在真实的改写产物上（因此能在 Windows/Linux 上跑）。
//
// 平台三件事用最朴素的实现：映像 = malloc、段权限 = 空实现、键号 = 假槽号。
// 检查项（全部是对真实产物的断言，不是「应该能跑」）：
//   1. 三个模块的 fixups 表头都能解析（段边界、键页、8 个槽号）—— 不装 rtld，见 x0_layout.py
//   2. 动态段与符号表能解析，符号数合理
//   3. 重定位全部有归宿；且没有任何一条落在代码段（含边界页，向上取整到 16K —— 写了就崩）
//   4. 门表：main 0 / sdk 135 / subsdk0 0
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
int dm_slot_keys_ready(void) { return 1; }

// 本机自检没有内嵌映像（那是 iOS 真机路线）：两个钩子都返回 NULL，装载器就走文件路径。
// macOS 上由 loader_apple.c 提供真实现；内嵌路径另有 embed_selftest 专门验。
const dm_embed_mod *dm_embed_find(const char *name) { (void)name; return NULL; }
void *dm_image_from_dylib(const char *name, size_t *size) { (void)name; (void)size; return NULL; }
void *dm_embed_map(const dm_embed_mod *e) { (void)e; return NULL; }

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
    // 模块名从命令行给：CI 上用合成产物（sdk main），本机用真实的三个（不装 rtld）
    static const char *def[] = {"sdk", "subsdk0", "main"};
    const char **mods = def;
    int nmods = 3;
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
    // 真实产物（三个模块）的门数是个已知事实，跑真实集时才断言它
    if (g_nmod == 3 && !strcmp(g_mod[2].name, "main")) {
        CHK(dm_gate_count() == 135, "真实产物：宿主门合计 135（main 0 + sdk 135 + subsdk0 0；不装 rtld）");
    }
    // **重要架构点**：这里统计的「交宿主解析」只有十几个名字，不是全部工作面。
    // 服务层那 412 个函数（nn::fs / nn::hid / nn::audio …）**被 sdk 模块导出**，所以走的是
    // 跨模块解析、根本到不了这里 —— 它们必须靠 dm_hle_force()（HLE_FORCE）接管。
    //
    // 精确值 13 与 smo/tools/import_graph.py 的独立结果对得上：「由谁导出」为 `-` 的
    // main 12 / sdk 5 / subsdk0 4 = 21 条，去掉三个模块之间重名的 8 个 = 13 个不同名字。
    // 原来是「20–60」的宽松区间，装 rtld 时是 25 —— 多出来的 12 个全是 rtld 的链接器
    // 段标记（__got_start / __rela_dyn_end / __tdata_align_abs …，import_graph 里 16 条）。
    // 改成精确值：区间太宽时，少装一个模块这种变化它也照样放过。
    if (g_nmod == 3 && !strcmp(g_mod[2].name, "main")) {
        CHK(g_import_uniq == 9,
            "真实产物：交宿主 import 9 个不同名字（= 旧的 13 减 4 个 rtld 链接器符号；服务层走 HLE_FORCE）");
    }
    // **回归断言**：四个 rtld 链接器符号必须由装载器按模块自己解析（__rel_dyn_start/end 指向
    // 模块的 .rela.dyn、__rel_plt_* 指向 .rela.plt）。真机第四崩就是它们被当成宿主 import
    // 塞进了宿主桩地址，而 __nnmusl_init_dso 拿这四个参数去做 DSO 初始化。
    {
        static const char *ls[] = {"__rel_dyn_start", "__rel_dyn_end", "__rel_plt_start", "__rel_plt_end"};
        int leaked = 0;
        for (unsigned i = 0; i < g_import_uniq; i++)
            for (unsigned k = 0; k < sizeof ls / sizeof ls[0]; k++)
                if (!strcmp(g_imp[i], ls[k])) leaked = 1;
        CHK(!leaked, "rtld 链接器符号没走宿主 import（模块自己提供重定位表边界）");
    }
    CHK(g_imports >= g_import_uniq, "调用次数不少于名字数");
    printf("\n%s\n", bad ? "有断言不通过" : "全部通过");
    return bad;
}
