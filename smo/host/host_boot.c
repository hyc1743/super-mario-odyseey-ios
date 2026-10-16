// 宿主引导顺序 —— **唯一一处**。CLI 宿主（main.c）与 iOS App（ios/Sources/main.m）都调它。
//
// 正确顺序（每一步都有真机事故在背后）：
//   1. host_thread_keys_create()         建宿主线程键（改写器的线程槽要用）
//   2. load_modules()                    映射 + 重定位 + 解析 import
//   3. svc_thread_main_init()            主线程登记（SDK 会把这个句柄当主线程）
//   4. guest_thread_init()               客体线程环境：x18 / TLS / TLR / 门
//   5. run_module_inits()                跑模块构造函数 —— **第一次执行客体代码**
//   6. guest_sym(entry) → dm_jump_guest  进游戏入口
//
// 为什么顺序单独一个文件、只有一份：
//   §4 原来排在 §5 后面 → 真机第三轮崩在 sdk 构造函数读 tpidrro_el0+0x1f8（当前线程对象，
//     TLR 为空 ⇒ 访问地址 0x1f8）；
//   §3 也曾经排在 §5 后面（App 里顺序与 CLI 不一致）→ 真机第二轮崩在同一个地方。
//   顺序写两份就一定会漂移（同一类教训第三次：CI 单测文件清单、CI 注入清单、引导顺序）。
//
// 为什么不放 loader_core.c：loader_selftest 只链 loader_core.c + logbuf.c（平台无关的
// 结构性自检），而这里要用线程层（hle_thread.c）。
// 
// 真实 rtld 也是这个次序：先把线程/TLS 建起来，再跑 init_array。
#include "dm.h"
#include "hle_thread.h"

void *host_boot(const char *dir, const char *const *mods, unsigned nmods, const char *entry) {
    host_thread_keys_create();
    load_modules(dir, mods, nmods);

    // rtld 的活之一：把 TLS 分配器接上（game 提供 alloc/free，sdk 提供 setter）。
    // 必须在 run_module_inits **之前** —— sdk 的 __nnmusl_init_dso 第一步就查这个标志，
    // 为假就直接返回：TLS 不建、SDK 运行时等于没初始化（真机第五崩）。
    {
        void *setter = NULL, *alloc = NULL, *dealloc = NULL;
        char why[160] = {0};
        if (dm_find_tls_allocator_wiring(&setter, &alloc, &dealloc, why, sizeof why)) {
            ((void (*)(void *, void *))setter)(alloc, dealloc);
            LOG("TLS 分配器已接上（alloc %p / free %p；setter %p）", alloc, dealloc, setter);
        } else {
            LOG("【红】TLS 分配器没接上（%s）—— SDK 的 DSO 初始化会直接返回，构造函数用 TLS 就会崩", why);
        }
    }

    // 主线程登记（句柄之后交给 SDK 的 nn::os；真机上 SDK 会拿它当主线程）—— **必须早于 run_module_inits**
    uint32_t main_h = svc_thread_main_init();
    dm_boot_mark(1);
    LOG("主线程句柄 %#x", main_h);
    if (!svc_thread_main_ready()) {
        LOG("【红】主线程登记没生效（pthread_setspecific 失败？）—— 继续下去客体构造函数会崩");
        return NULL;
    }

    // 客体线程环境（x18 / TLS / TLR / 门）必须早于**任何**客体代码。
    // 真机第三轮实录：TLS/TLR 还没建就跑模块构造函数，sdk 构造函数读 tpidrro_el0+0x1f8
    // （Horizon 的「当前线程对象」）时 TLR 为空 → 访问地址 0x1f8 → SIGSEGV。
    // 这也是真实 rtld 的做法：先建线程/TLS，再跑 init_array。
    guest_thread_init();
    dm_boot_mark(2);

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

    // （客体线程环境已在上面建好；跳到入口即可）
    return fn;
}
