// 宿主引导顺序 —— **唯一一处**。CLI 宿主（main.c）与 iOS App（ios/Sources/main.m）都调它。
//
// 正确顺序（每一步都有真机事故在背后；SDK 运行时那一段照参考项目逐条抄 nn::init::Start）：
//   1. host_thread_keys_create()     建宿主线程键（改写器的线程槽要用）
//   2. load_modules()                映射 + 重定位 + 解析 import
//   3. svc_thread_main_init()        主线程登记（SDK 把这个句柄当主线程）
//   4. guest_thread_init()           客体线程环境：x18 / TLS / TLR / 门
//   5. TLS 分配器接线                  game 提供 alloc/free，sdk 提供 setter（rtld 的活）
//   6. SDK 运行时初始化（六步）        _init_libc0 → nnosInitialize → _init_libc1 →
//                                     nninitInitializeSdkModule → nninitStartup → _init_libc2
//   7. run_module_inits()            模块 DT_INIT —— 这里才第一次跑 SDK 的 DSO 初始化/构造函数
//   8. SetHostArgc/SetHostArgv       游戏参数
//   9. 返回入口 → dm_jump_guest(nnMain)
//
// 为什么顺序单独一个文件、只有一份：
//   · §3/§4 曾经排在 §7 后面（App 与 CLI 各写一份、还漂移了）→ 真机第二/第三崩；
//   · §6 整段曾经缺失 → nnosInitialize 没跑，nn::os 资源管理器（g_OsResourceManagerStorage）
//     一直是 0 → 模块构造函数用 TLS 时崩在空链表上（真机第四/第五崩）；
//     而 §6 里 nninitStartup 初始化游戏分配器，缺了它 §7 的 DSO 初始化会拿到 NULL（真机第六崩）。
//   顺序这种东西写两份就一定会漂移（同一类教训第三次：CI 单测文件清单、CI 注入清单、引导顺序）。
//
// 为什么不放 loader_core.c：loader_selftest 只链 loader_core.c + logbuf.c（平台无关的
// 结构性自检），而这里要用线程层（hle_thread.c）。
// 
// 真实 rtld 也是这个次序：先把线程/TLS 建起来，再跑 init_array。
#include "dm.h"
#include "hle_thread.h"
#include "logbuf.h"     // dm_progress_*：App 首屏的步骤/进度条

// ---- SDK 运行时初始化的每一步（符号从客体取，缺了就红字报出来，不让它悄悄跳过）----
// 顺序照参考项目逐条抄的 nn::init::Start：它在真机上跑通了，而顺序错了的后果我们连吃两轮
// （模块构造函数的 DSO 初始化要用 nn::os 与游戏分配器，两者都得在这之前就绪）。
static void call0(const char *name) {
    void *p = guest_sym(name);
    if (!p) { LOG("【红】缺少 %s —— SDK 初始化顺序里的一步，跳过会让后面在奇怪的地方崩", name); return; }
    LOG(">>> %s", name);
    ((void (*)(void))p)();
}
static void call1(const char *name, uint64_t a) {
    void *p = guest_sym(name);
    if (!p) { LOG("【红】缺少 %s", name); return; }
    LOG(">>> %s(%#llx)", name, (unsigned long long)a);
    ((void (*)(uint64_t))p)(a);
}
static void call2(const char *name, uint64_t a, uint64_t b) {
    void *p = guest_sym(name);
    if (!p) { LOG("【红】缺少 %s", name); return; }
    LOG(">>> %s(%#llx, %#llx)", name, (unsigned long long)a, (unsigned long long)b);
    ((void (*)(uint64_t, uint64_t))p)(a, b);
}

void *host_boot(const char *dir, const char *const *mods, unsigned nmods, const char *entry) {
    dm_progress_stage(3, 6, "装载游戏程序（3 个模块）");
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
    dm_progress_stage(4, 6, "建立线程环境");
    uint32_t main_h = svc_thread_main_init();
    dm_boot_mark(1);
    LOG("主线程句柄 %#x", main_h);
    if (!svc_thread_main_ready()) {
        LOG("【红】主线程登记没生效（pthread_setspecific 失败？）—— 继续下去客体构造函数会崩");
        dm_progress_finish(DM_RUN_FAIL, "线程环境没建起来");
        return NULL;
    }

    // 客体线程环境（x18 / TLS / TLR / 门）必须早于**任何**客体代码。
    // 真机第三轮实录：TLS/TLR 还没建就跑模块构造函数，sdk 构造函数读 tpidrro_el0+0x1f8
    // （Horizon 的「当前线程对象」）时 TLR 为空 → 访问地址 0x1f8 → SIGSEGV。
    // 这也是真实 rtld 的做法：先建线程/TLS，再跑 init_array。
    guest_thread_init();
    dm_boot_mark(2);

    // ---- SDK 运行时初始化：按 nn::init::Start 的真实顺序（参考项目逐条照抄）----
    //   _init_libc0 → nnosInitialize（构造 nn::os 资源管理器/线程管理器）→ _init_libc1（musl 锁/SSP）
    //   → nninitInitializeSdkModule → nninitStartup（游戏分配器 InitializeAllocator）→ _init_libc2
    // 这几步必须在模块 DT_INIT 之前：模块构造函数一上来就要用 nn::os 的 TLS，
    // 而 nnosInitialize 正是构造 g_OsResourceManagerStorage 的地方（真机第五/第六崩就缺它）。
    dm_progress_stage(5, 6, "初始化游戏运行库（SDK）");
    dm_progress_frac(0, "nn::init 六步");
    call0("_init_libc0");
    call2("nnosInitialize", main_h, 0);
    call0("_init_libc1");
    call0("nninitInitializeSdkModule");
    call0("nninitStartup");
    call0("_init_libc2");

    // 扮演 rtld 的 InitModules：按装载顺序调各模块 DT_INIT（模块自己的 init_array 在 _init 里跑）
    dm_progress_frac(0.6, "模块构造函数");
    run_module_inits();

    // 游戏参数：真机由 nn::init 从进程参数区取，这里只给一个 argv[0]（SMO 不需要别的参数）
    {
        static char name0[] = "SMO";
        static char *g_argv[] = { name0, NULL };
        call1("_ZN2nn2os11SetHostArgcEi", 1);
        call2("_ZN2nn2os11SetHostArgvEPPc", (uint64_t)(uintptr_t)g_argv, 0);
    }

    void *fn = guest_sym(entry);
    if (!fn) {
        LOG("【红】找不到入口符号 %s", entry);
        dm_progress_finish(DM_RUN_FAIL, "找不到游戏入口");
        return NULL;
    }
    module_t *m = module_of(fn);
    LOG("== 进入客体入口 %s（在 %s，偏移 +%#zx）==", entry, m ? m->name : "?",
        m ? (size_t)((const uint8_t *)fn - m->base) : 0);

    // （客体线程环境已在上面建好；跳到入口即可）
    dm_progress_stage(6, 6, "运行游戏");
    return fn;
}
