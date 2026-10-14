// 宿主 bootstrap：装载模块 → 跑各模块的 DT_INIT → 进游戏入口。
// 派生自参考项目 d2r-mobile 的 dm/host/main.c（MIT）。
//
// 用法：
//   smo_host [模块目录] [模块名…]                 默认 rtld sdk subsdk0 main，进 nnMain
//   smo_host <目录> <模块…> --entry <符号名>      进指定的符号（自检用：合成客体放一个入口）
//   smo_host <目录> <模块…> --list                只装载并打印统计，不进客体代码
//
// 现在还没做的（下一步，见 STATUS.md）：
//   · SDK 运行时的初始化顺序（参考项目是「按 SDK 自己的启动顺序初始化 SDK 运行时」再进 nnMain）
//   · 前端（窗口/图形）与 HLE 服务实现（当前服务层全部是生成的桩）
#include "dm.h"
#include "hle_thread.h"
#include <unistd.h>

void host_thread_keys_create(void);
void guest_thread_init(void);
extern void dm_jump_guest(void *arg, void *fn);

// 客体线程从入口返回 = 违反 Horizon 语义（必须 ExitThread），报告出来
void dm_thread_returned(void) { LOG("【红】客体线程从入口返回了（Horizon 要求经 ExitThread 退出）"); }

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "work/rewrite";
    static const char *defmods[] = {"rtld", "sdk", "subsdk0", "main"};
    const char **mods = defmods;
    int nmods = 4, list_only = 0;
    const char *entry = "nnMain";

    // 参数：目录之后到第一个 -- 选项之间是模块名
    int i = 2;
    if (i < argc && strncmp(argv[i], "--", 2)) {
        int start = i;
        while (i < argc && strncmp(argv[i], "--", 2)) i++;
        mods = (const char **)&argv[start];
        nmods = i - start;
    }
    for (; i < argc; i++) {
        if (!strcmp(argv[i], "--list")) list_only = 1;
        else if (!strcmp(argv[i], "--entry") && i + 1 < argc) entry = argv[++i];
    }

    LOG("== SMO 宿主：装载 %s 下的 %d 个模块 ==", dir, nmods);
    host_thread_keys_create();
    load_modules(dir, mods, nmods);
    if (list_only) { unimpl_report(); return 0; }

    // 主线程登记（句柄之后交给 SDK 的 nn::os；真机上 SDK 会拿它当主线程）
    uint32_t main_h = svc_thread_main_init();
    LOG("主线程句柄 %#x", main_h);

    // 扮演 rtld 的 InitModules：按装载顺序调各模块 DT_INIT（模块自己的 init_array 在 _init 里跑）
    run_module_inits();

    void *fn = guest_sym(entry);
    if (!fn) { LOG("【红】找不到入口符号 %s", entry); return 1; }
    module_t *m = module_of(fn);
    LOG("== 进入客体入口 %s（在 %s，偏移 +%#zx）==", entry, m ? m->name : "?",
        m ? (size_t)((const uint8_t *)fn - m->base) : 0);

    // 主线程也要有客体线程环境（线程槽），再跳进客体
    guest_thread_init();
    dm_jump_guest(NULL, fn);
    LOG("== 客体入口返回了（正常游戏里 nnMain 不会返回）==");
    unimpl_report();
    return 0;
}
