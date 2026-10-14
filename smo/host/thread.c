// 客体线程环境（Apple 侧）：把四个线程槽填好。
// 派生自参考项目 d2r-mobile 的 dm/host/thread.c（MIT）。
//
// 线程槽的取法（改写器与宿主之间的约定）：
//   槽地址 = tpidrro_el0 + 8 × 键号        —— 键页里存的就是「键号 × 8」
//   K18   = 客体 x18（Switch 的 x18 语义；这里清零）
//   KTLS  = ELF TLS 块（客体自己会在上面设线程指针）
//   KTLR  = Horizon 的 ThreadLocalRegion（0x200 字节）
//   KGATE = 宿主门入口 _dm_gate
//
// 关于 KTLR：SDK 的内部临界区会从 TLR[0x1f8] 取「当前线程对象」指针，
// 所以每个线程要有一块零初始化的对象写到那一格（参考项目实测如此，见其注释）。
#include "dm.h"
#include <pthread.h>

#define TLS_SIZE 8192
#define TLR_SIZE 0x200
#define THREAD_OBJ_SIZE 0x400

extern void dm_gate(void);
extern void dm_thread_enter(void *entry, void *arg, void *sp);
extern void dm_jump_guest(void *arg, void *fn);

static pthread_key_t g_key18, g_keytls, g_keytlr, g_keygate;
static pthread_key_t g_keyres[2], g_keyaspect, g_keyaspectui;
static pthread_key_t g_keyown;                      // 本线程申请的 TLS/TLR 原始指针（供析构释放）
static pthread_once_t g_keyown_once = PTHREAD_ONCE_INIT;

uint64_t dm_slot_key(unsigned i) {
    switch (i) {
    case KEY_K18: return g_key18;
    case KEY_KTLS: return g_keytls;
    case KEY_KTLR: return g_keytlr;
    case KEY_KGATE: return g_keygate;
    case KEY_KRES0: return g_keyres[0];
    case KEY_KRES1: return g_keyres[1];
    case KEY_KASPECT: return g_keyaspect;
    default: return g_keyaspectui;
    }
}

// pthread TSD 的布局必须是「tpidrro_el0 + 8×键」，装载器只会在键页里存键号 × 8。
// 这条在真机上不成立就会全线错位，所以在起跑前先自证一次（参考项目的 tsd_check 同口径）。
static void tsd_check(void) {
    pthread_setspecific(g_key18, (void *)0x1234);
    uint64_t base;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(base));
    uint64_t off = *(const uint64_t *)(g_mod[0].base + g_mod[0].key_page_off + g_mod[0].key_idx[KEY_K18] * 8);
    if (((const uint64_t *)(base & ~7ull))[off / 8] != 0x1234)
        die("pthread TSD 布局与改写器的假设不符（tpidrro_el0 + 8×键号 取不到刚设的值）");
    pthread_setspecific(g_key18, 0);
}

static void own_free(void *p) {
    void **own = p;
    free(own[0]);
    free(own[1]);
    free(own);
}
static void own_key_init(void) { if (pthread_key_create(&g_keyown, own_free)) die("创建线程块释放键失败"); }

void host_thread_keys_create(void) {
    if (pthread_key_create(&g_key18, NULL) || pthread_key_create(&g_keytls, NULL) ||
        pthread_key_create(&g_keytlr, NULL) || pthread_key_create(&g_keygate, NULL))
        die("创建宿主线程键失败");
    // 四个参数位键一律建：iOS 包里没有 params.tsv，按文件判断会漏建，键页就会写成 0
    if (pthread_key_create(&g_keyres[0], NULL) || pthread_key_create(&g_keyres[1], NULL) ||
        pthread_key_create(&g_keyaspect, NULL) || pthread_key_create(&g_keyaspectui, NULL))
        die("创建参数位键失败");
    LOG("线程键：K18=%lu KTLS=%lu KTLR=%lu KGATE=%lu（KRES0=%lu KRES1=%lu KASPECT=%lu KASPECTUI=%lu）",
        g_key18, g_keytls, g_keytlr, g_keygate, g_keyres[0], g_keyres[1], g_keyaspect, g_keyaspectui);
    pthread_once(&g_keyown_once, own_key_init);
    tsd_check();
}

// 每个客体线程进来都要调一次（SDK 起线程 → dm_thread_enter → 入口函数里调）
void guest_thread_init(void) {
    pthread_setspecific(g_keygate, (void *)dm_gate);
    pthread_setspecific(g_key18, 0);
    pthread_setspecific(g_keyres[0], (void *)0);      // SMO 没有 PARAMS 剪口，两个参数位给 0 也可，但键页要求非 0 键号
    pthread_setspecific(g_keyres[1], (void *)0);
    pthread_setspecific(g_keyaspect, (void *)0);
    pthread_setspecific(g_keyaspectui, (void *)0);
    // TLS 块与 TLR（+ 当前线程对象）并成一块申请，原始指针记在自己的键上，析构里释放。
    // 不能按键里的当前值 free：客体会改写这两把键指向的槽（它自己设线程指针）。
    void **own = calloc(2, sizeof *own);
    uint8_t *tlr = calloc(1, TLR_SIZE + THREAD_OBJ_SIZE);
    own[0] = calloc(1, TLS_SIZE);
    own[1] = tlr;
    *(void **)(tlr + 0x1f8) = tlr + TLR_SIZE;         // 当前线程对象
    pthread_setspecific(g_keytls, own[0]);
    pthread_setspecific(g_keytlr, tlr);
    pthread_setspecific(g_keyown, own);
}
