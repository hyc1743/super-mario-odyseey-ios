// 宿主引导顺序 —— **唯一一处**。CLI 宿主（main.c）与 iOS App（ios/Sources/main.m）都调它。
//
// 为什么单独一个文件、为什么只有这一份顺序：
//   原来这段顺序在 main.c 与 ios/Sources/main.m 里各写了一遍，结果漂移了 —— 真机首跑时
//   App 里 run_module_inits 排在了 svc_thread_main_init 前面：客体（sdk）的第一个构造函数
//   就去读线程/TLS 环境，拿到空指针，进程当场崩，日志停在「sdk: DT_INIT +0x24（init_array 103 项）」。
//   顺序这种东西写两份就一定会漂移（项目里同一个教训已经栽过两次：CI 里的单测文件清单、
//   现在是引导顺序）。所以：顺序只在这里。
//
//   也不能放进 loader_core.c：loader_selftest 只链 loader_core.c + logbuf.c（平台无关的
//   结构性自检），而这里要用线程层（hle_thread.c）。
#include "dm.h"
#include "hle_thread.h"

void *host_boot(const char *dir, const char *const *mods, unsigned nmods, const char *entry) {
    host_thread_keys_create();
    load_modules(dir, mods, nmods);

    // 主线程登记（句柄之后交给 SDK 的 nn::os；真机上 SDK 会拿它当主线程）—— **必须早于 run_module_inits**
    uint32_t main_h = svc_thread_main_init();
    dm_boot_mark_main_thread();
    LOG("主线程句柄 %#x", main_h);
    if (!svc_thread_main_ready()) {
        LOG("【红】主线程登记没生效（pthread_setspecific 失败？）—— 继续下去客体构造函数会崩");
        return NULL;
    }

    // 扮演 rtld 的 InitModules：按装载顺序调各模块 DT_INIT（模块自己的 init_array 在 _init 里跑）
    run_module_inits();

    void *fn = guest_sym(entry);
    if (!fn) {
        LOG("【红】找不到入口符号 %s", entry);
        return NULL;
    }
    module_t *m = module_of(fn);
    LOG("== 进入客体入口 %s（在 %s，偏移 +%#zx）==", entry, m ? m->name : "?",
        m ? (size_t)((const uint8_t *)fn - m->base) : 0);

    // 主线程也要有客体线程环境（线程槽、门、x18），再跳进客体
    guest_thread_init();
    return fn;
}
