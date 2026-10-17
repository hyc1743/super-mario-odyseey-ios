// 宿主 bootstrap：装载模块 → 跑各模块的 DT_INIT → 进游戏入口。
// 派生自参考项目 d2r-mobile 的 dm/host/main.c（MIT）。
//
// 用法：
//   smo_host [模块目录] [模块名…]                 默认 sdk subsdk0 main，进 nnMain（不装 rtld，见下）
//   smo_host <目录> <模块…> --entry <符号名>      进指定的符号（自检用：合成客体放一个入口）
//   smo_host <目录> <模块…> --list                只装载并打印统计，不进客体代码
//
// 现在还没做的（下一步，见 STATUS.md）：
//   · SDK 运行时的初始化顺序（参考项目是「按 SDK 自己的启动顺序初始化 SDK 运行时」再进 nnMain）
//   · 前端（窗口/图形）与 HLE 服务实现（当前服务层全部是生成的桩）
#include "dm.h"
#include "hle_thread.h"
#include "dm_input.h"
#include <pthread.h>
#include <unistd.h>

void host_thread_keys_create(void);
void guest_thread_init(void);
extern void dm_jump_guest(void *arg, void *fn);

/* Headless verification only: exercise the same HID state as the touch pad.
 * The iOS frontend does not link main.c and never synthesizes these presses. */
typedef struct { unsigned delay, count; } test_input_config;
static void *test_press_a(void *argument) {
    test_input_config config = *(test_input_config *)argument;
    sleep(config.delay);
    for (unsigned i=0;i<config.count;i++) {
        if (i) sleep(10);
        dm_input_set_buttons(DM_BTN_A);
        LOG("TEST_INPUT_A pressed=1 index=%u", i+1);
        usleep(300000);
        dm_input_set_buttons(0);
        LOG("TEST_INPUT_A pressed=0 index=%u", i+1);
    }
    return NULL;
}

// dm_thread_returned 由 hle_thread.c 提供（客体线程入口返回 = 违反 Horizon 语义）
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "work/rewrite";
    // 不装 rtld：宿主自己扮演 rtld（映射/重定位/DT_INIT 都在 loader_core.c），而 rtld 的 32 条重定位
    // 落在代码段边界页里，按三段装不了（smo/ios/tools/x0_layout.py）。参考项目同样只装游戏模块。
    static const char *defmods[] = {"sdk", "subsdk0", "main"};
    const char **mods = defmods;
    int nmods = 3, list_only = 0;
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
    if (list_only) {
        host_thread_keys_create();
        load_modules(dir, mods, nmods);
        unimpl_report();
        return 0;
    }

    // 引导顺序只有一处：host_boot()（建键 → 装载 → 主线程登记 → 模块 inits → 入口 → 客体线程环境）。
    // 以前这段顺序在本文件与 iOS App 里各写了一遍，漂移过一次（真机首跑：客体构造函数必崩）。
    void *fn = host_boot(dir, mods, nmods, entry);
    if (!fn) return 1;
    const char *press_after = getenv("SMO_TEST_PRESS_A_AFTER");
    if (press_after) {
        static test_input_config config;
        char *end; unsigned long seconds = strtoul(press_after, &end, 10);
        if (!*press_after || *end || seconds > 120) die("Invalid test input delay");
        config.delay=(unsigned)seconds; config.count=1;
        const char *count_text=getenv("SMO_TEST_PRESS_A_COUNT");
        if (count_text) {
            unsigned long count=strtoul(count_text,&end,10);
            if (!*count_text || *end || !count || count>8) die("Invalid test input count");
            config.count=(unsigned)count;
        }
        pthread_t input;
        if (pthread_create(&input, NULL, test_press_a, &config))
            die("Failed to create test input worker");
        pthread_detach(input);
    }
    dm_jump_guest(NULL, fn);
    LOG("== 客体入口返回了（正常游戏里 nnMain 不会返回）==");
    unimpl_report();
    return 0;
}
