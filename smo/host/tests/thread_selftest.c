// 内核线程族 svc 的单测：**不需要游戏文件**，也不需要 Apple 平台（用宿主 pthread 直接当客体线程）。
//
// 测试自己扮演客体代码：直接按 ABI 调 svc_thread()。检查的都是启动/运行时会用到的语义：
//   1. 主线程登记 → 拿主线程句柄
//   2. CreateThread + StartThread + ExitThread：线程真的跑起来并按 Horizon 的方式退出
//   3. ArbitrateLock/Unlock：两个线程争锁时**互斥成立**（共享计数器不会被打断）
//   4. WaitProcessWideKeyAtomic/SignalProcessWideKey：等者被唤醒、锁被交回
//   5. 事件：CreateEvent + WaitSynchronization(带超时) + SignalEvent 唤醒
//   6. 地址仲裁：WaitForAddress + SignalToAddress
//   7. 超时口径：WaitSynchronization 超时返回 0xEA01
#include "../dm.h"
#include "../hle_thread.h"
#include <assert.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

module_t g_mod[MAX_MOD];
int g_nmod;

// ---- 平台层与 SDK 侧的桩（本机自检用）----
static uint8_t g_fake_stack[0x8000];
static int g_entered;
void dm_thread_enter(void *entry, void *arg, void *sp) {
    (void)sp;                                  // 自检不切栈（真机上 dm_thread_enter 会切到客体栈）
    g_entered++;
    ((void (*)(void *))entry)(arg);
    dm_thread_returned();                      // 入口返回 = 违规，应当中止
}
void guest_thread_init(void) { }               // 真机上是线程槽设置点；自检不需要
void *guest_sym(const char *name) { (void)name; return NULL; }
void unimpl_report(void) { }
uint64_t dm_hle_force(const char *name) { (void)name; return 0; }
uint64_t resolve_import(module_t *m, const char *name) { (void)m; (void)name; return 0x1000; }
uint64_t dm_slot_key(unsigned i) { return 100 + i; }
void dm_code_protect(void *p, size_t n) { (void)p; (void)n; }
void dm_ro_protect(void *p, size_t n) { (void)p; (void)n; }
void gate_register(uint64_t a, const char *k, int arg, uint64_t o) { (void)a; (void)k; (void)arg; (void)o; }
int svc_dispatch(unsigned num, uint64_t *regs) { (void)num; (void)regs; return -1; }

// 等到条件成立或超时。**不能按固定次数空转**：CI 的 runner 比本机慢得多，
// 固定次数的 sched_yield 跑不完并发用例（第一次上 CI 就是这么红的）。
static int wait_flag(volatile int *flag, int seconds) {
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (!*flag) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - t0.tv_sec >= seconds) return 0;
        usleep(200);
    }
    return 1;
}

static int fails;
// 注意：条件**只能求值一次**（见 fs_selftest.c 里的说明：求值两次会让带副作用的断言跑两遍）
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

// 按 ABI 调一个 svc：regs[0..3] 进、regs[0..1] 出
static uint64_t svc(unsigned num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t *out1) {
    uint64_t r[6] = { a0, a1, a2, a3, 0, 0 };
    int handled = svc_thread((int)num, r);
    if (!handled) { printf("  【红】svc #%#x 未被线程族处理\n", num); fails++; }
    if (out1) *out1 = r[1];
    return r[0];
}
static void exit_thread(void) { svc(0x0A, 0, 0, 0, 0, NULL); printf("  【红】ExitThread 返回了\n"); fails++; }

// ---- 争锁：两个线程各加 N 次，每次都在锁里检查互斥 ----
static uint32_t g_lock_word __attribute__((aligned(8)));
static volatile int g_in_critical, g_exclusive_violations, g_counter;
static void lock_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 2000; i++) {
        svc(0x1A, 0, (uint64_t)(uintptr_t)&g_lock_word, 0x1234, 0, NULL);   // ArbitrateLock
        if (g_in_critical) g_exclusive_violations++;
        g_in_critical = 1;
        g_counter++;
        g_in_critical = 0;
        svc(0x1B, (uint64_t)(uintptr_t)&g_lock_word, 0, 0, 0, NULL);        // ArbitrateUnlock
    }
    exit_thread();
}

// ---- 条件变量：一个线程等 key，另一个 signal ----
static uint32_t g_cond_lock, g_cond_key;
static volatile int g_cond_woken;
static void cond_waiter(void *arg) {
    (void)arg;
    svc(0x1A, 0, (uint64_t)(uintptr_t)&g_cond_lock, 1, 0, NULL);            // 先拿锁
    svc(0x1C, (uint64_t)(uintptr_t)&g_cond_lock, (uint64_t)(uintptr_t)&g_cond_key, 1, -1, NULL);  // 置键+等（无限）
    g_cond_woken = 1;
    svc(0x1B, (uint64_t)(uintptr_t)&g_cond_lock, 0, 0, 0, NULL);            // 被唤醒时锁已在手（内核交回）
    exit_thread();
}
static void cond_signaler(void *arg) {
    (void)arg;
    // 等到确实有线程在等这个条件键，再 signal（确定时序）。
    // 不能写「等 g_cond_woken」—— 那是等自己被唤醒，直接死锁（我把这里改错过一次）。
    for (int waited = 0; waited < 50000 && dm_thread_waiting(2) == 0; waited++) usleep(200);
    svc(0x1D, (uint64_t)(uintptr_t)&g_cond_key, 1, 0, 0, NULL);             // SignalProcessWideKey
    exit_thread();
}

// ---- 事件：等者带超时，主线程 signal ----
static uint32_t g_ev_w, g_ev_r;
static volatile int g_ev_woken;
static void ev_waiter(void *arg) {
    (void)arg;
    uint32_t hs = g_ev_r;
    uint64_t idx = 0;
    uint64_t r = svc(0x18, 0, (uint64_t)(uintptr_t)&hs, 1, 2000000000ull, &idx);   // 2 秒
    g_ev_woken = (r == 0 && idx == 0);
    exit_thread();
}

// ---- 地址仲裁 ----
// 地址仲裁的正确用法（Horizon 的协议）：**状态放在那个字里**。
//   等者：WaitForAddress(addr, type=0, val=1) —— 只在 *addr < 1 时* 才睡（即值为 0 时睡）
//   信号方：先把 addr 写成 1，再 SignalToAddress(addr, type=0) —— 谁先谁后都不丢：
//           信号先到 → 等者读到 1，根本不睡；等者先睡 → 被唤醒
// 一开始我把测试写成「先 SleepThread 一会儿再 signal」，那是竞态（信号早于登记就永久睡死）。
static uint32_t g_addr_word __attribute__((aligned(8)));
static volatile int g_addr_woken;
static void addr_waiter(void *arg) {
    (void)arg;
    uint64_t r = svc(0x34, (uint64_t)(uintptr_t)&g_addr_word, 0, 1, 1000000000ull, NULL);  // 类型 0：值 < 1 才等
    g_addr_woken = (r == 0);
    exit_thread();
}

// ---- 超时 ----
static volatile int g_timeout_seen;
static void timeout_waiter(void *arg) {
    (void)arg;
    uint32_t hs = 0xDEAD;                       // 故意给一个不等的句柄？不行——会用坏句柄报错
    (void)hs;
    uint32_t h = 0;
    uint64_t idx = 0;
    // 用一个真事件句柄，但没人触发 → 应当超时
    uint32_t e = g_ev_r;
    uint64_t r = svc(0x18, 0, (uint64_t)(uintptr_t)&e, 1, 20000000ull, &idx);   // 20 毫秒
    g_timeout_seen = ((r & 0xFFFF) == (0xEA01 & 0xFFFF));
    (void)h;
    exit_thread();
}

int main(void) {
    printf("== 0. 主线程登记 ==\n");
    uint32_t main_h = svc_thread_main_init();
    CHK(main_h != 0, "主线程句柄非 0");

    {
        // 寄存器布局照 libnx：x0 是输出指针（宿主不该读它），句柄在 w1 / w2。
        // 真机第九轮就栽在「把 x0 当句柄」：GetThreadId 报坏句柄、优先级拿到 0x1001 → svcBreak。
        uint64_t mh = main_h, out = 0;
        uint64_t junk = 0x16f8c7c20ull;
        CHK(svc(0x25, junk, mh, 0, 0, &out) == 0, "GetThreadId：句柄从 w1 取");
        CHK(out != 0 && out != mh, "GetThreadId：x1 回的是线程 id（不是句柄原样）");
        CHK(svc(0x25, junk, 0xFFFF8000u, 0, 0, &out) == 0, "GetThreadId：伪句柄「当前线程」也认");
        CHK(svc(0x0C, junk, mh, 0, 0, &out) == 0 && out <= 63, "GetThreadPriority：w1 句柄 → 0..63 的优先级");
        uint64_t r[6] = { junk, 0, mh, 0, 0, 0 };
        CHK(svc_thread(0x0E, r) && r[0] == 0 && r[1] <= 3 && r[2] != 0 && r[2] != mh,
            "GetThreadCoreMask：w2 句柄 → 核号 + 非零掩码");
        CHK(svc(0x25, junk, 0x7777, 0, 0, NULL) != 0, "坏句柄照样报错");
    }

    printf("== 1. 事件 + 等待 + 唤醒 ==\n");
    uint64_t hw = 0, hr = 0, hw2 = 0, hr2 = 0;
    CHK(svc(0x45, 0, 0, 0, 0, NULL) == 0, "CreateEvent 成功");   // 下面用 out1 拿写端
    { uint64_t r[6] = {0}; svc_thread(0x45, r); hw = r[1]; hr = r[2]; }
    CHK(hw && hr && hw != hr, "拿到写端与读端两个句柄");
    { uint64_t r[6] = {0}; svc_thread(0x45, r); hw2 = r[1]; hr2 = r[2]; }
    CHK(hw2 && hr2 && hw2 != hr2, "second event handles are distinct");
    g_ev_w = (uint32_t)hw; g_ev_r = (uint32_t)hr;
    { uint64_t r[6] = {0}; r[1] = (uint64_t)(uintptr_t)ev_waiter; r[2] = 0; r[3] = (uint64_t)(uintptr_t)(g_fake_stack + sizeof g_fake_stack); r[4] = 16;
      svc_thread(0x08, r); svc(0x09, r[1], 0, 0, 0, NULL); }
    wait_flag((volatile int *)&g_entered, 20);
    CHK(g_entered > 0, "客体线程跑起来了（CreateThread+StartThread）");
    svc(0x11, hw2, 0, 0, 0, NULL);
    usleep(5000);
    CHK(!g_ev_woken && dm_thread_waiting(4) > 0, "unrelated event leaves this waiter blocked");
    svc(0x11, hw, 0, 0, 0, NULL);                                 // SignalEvent
    wait_flag((volatile int *)&g_ev_woken, 20);
    CHK(g_ev_woken, "WaitSynchronization 被 SignalEvent 唤醒，返回下标 0");

    printf("== 2. 地址仲裁 ==\n");
    g_addr_word = 0;
    { uint64_t r[6] = {0}; r[1] = (uint64_t)(uintptr_t)addr_waiter; r[3] = (uint64_t)(uintptr_t)(g_fake_stack + sizeof g_fake_stack);
      svc_thread(0x08, r); svc(0x09, r[1], 0, 0, 0, NULL); }
    for (int waited = 0; waited < 50000 && dm_thread_waiting(3) == 0; waited++) usleep(200);
    CHK(dm_thread_waiting(3) > 0, "有线程正在 WaitForAddress 上等（时序确定）");
    g_addr_word = 1;                                   // 状态先进字里（协议的一半）
    CHK(svc(0x35, (uint64_t)(uintptr_t)&g_addr_word, 0, 0, 0, NULL) == 0, "SignalToAddress 成功");
    wait_flag((volatile int *)&g_addr_woken, 20);
    CHK(g_addr_woken, "WaitForAddress 被唤醒");

    printf("== 3. 争锁互斥（两线程各 2000 次）==\n");
    g_lock_word = 0; g_counter = 0; g_exclusive_violations = 0;
    for (int k = 0; k < 2; k++) {
        uint64_t r[6] = {0};
        r[1] = (uint64_t)(uintptr_t)lock_worker;
        r[3] = (uint64_t)(uintptr_t)(g_fake_stack + sizeof g_fake_stack);
        r[4] = 16;
        svc_thread(0x08, r);
        svc(0x09, r[1], 0, 0, 0, NULL);
    }
    for (int waited = 0; waited < 300000 && g_counter < 4000; waited++) usleep(200);
    CHK(g_counter == 4000, "两次共 4000 次自增都完成了");
    CHK(g_exclusive_violations == 0, "临界区没有重叠（锁的交接正确）");
    CHK(g_lock_word == 0, "全部解锁后锁字回到 0");

    printf("== 4. 条件变量（WaitProcessWideKey / Signal）==\n");
    g_cond_lock = 0; g_cond_key = 0; g_cond_woken = 0;
    for (int k = 0; k < 2; k++) {
        uint64_t r[6] = {0};
        r[1] = (uint64_t)(uintptr_t)(k == 0 ? cond_waiter : cond_signaler);
        r[3] = (uint64_t)(uintptr_t)(g_fake_stack + sizeof g_fake_stack);
        r[4] = 16;
        svc_thread(0x08, r);
        svc(0x09, r[1], 0, 0, 0, NULL);
    }
    wait_flag((volatile int *)&g_cond_woken, 20);
    CHK(g_cond_woken, "等者被 SignalProcessWideKey 唤醒");

    printf("== 5. 超时口径 ==\n");
    g_timeout_seen = 0;
    g_ev_r = (uint32_t)hr;                                  // 用刚才那个事件（已 clear）
    svc(0x12, hr, 0, 0, 0, NULL);                           // ClearEvent 清掉
    { uint64_t r[6] = {0}; r[1] = (uint64_t)(uintptr_t)timeout_waiter; r[3] = (uint64_t)(uintptr_t)(g_fake_stack + sizeof g_fake_stack);
      svc_thread(0x08, r); svc(0x09, r[1], 0, 0, 0, NULL); }
    wait_flag((volatile int *)&g_timeout_seen, 20);
    CHK(g_timeout_seen, "没人触发时 WaitSynchronization 返回超时 0xEA01");

    printf("== 6. CloseHandle ==\n");
    CHK(svc(0x16, hw, 0, 0, 0, NULL) == 0, "关写端");
    CHK(svc(0x16, hr, 0, 0, 0, NULL) == 0, "关读端（两端都关才释放事件）");
    CHK(svc(0x16, hw2, 0, 0, 0, NULL) == 0, "close unrelated event write handle");
    CHK(svc(0x16, hr2, 0, 0, 0, NULL) == 0, "close unrelated event read handle");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
