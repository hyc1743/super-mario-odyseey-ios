// Horizon 内核的线程与同步 SVC：每个客体线程 = 一个宿主 pthread，跑在 SDK 给的客体栈上。
// 派生自参考项目 d2r-mobile 的 dm/host/hle_thread.c（MIT），保留语义、去掉其诊断/现场导出部分。
//
// SDK 的 nn::os 原样运行，只把它底下的「内核」换成宿主：
//   线程  0x08 CreateThread  0x09 StartThread  0x0A ExitThread  0x0B SleepThread
//         0x0C/0x0D 取/设优先级  0x0E/0x0F 取/设核掩码  0x10 取当前核号  0x25 取线程号
//         0x19 CancelSynchronization
//   锁    0x1A ArbitrateLock  0x1B ArbitrateUnlock（标签字 = 持有者句柄 | 0x40000000 表示有等待者）
//   条件  0x1C WaitProcessWideKeyAtomic  0x1D SignalProcessWideKey
//   地址  0x34 WaitForAddress  0x35 SignalToAddress
//   事件  0x45 CreateEvent  0x11 SignalEvent  0x12 ClearEvent  0x17 ResetSignal
//         0x18 WaitSynchronization  0x16 CloseHandle
//
// 调度顺序按公开内核行为：**优先级数值小者优先，同优先级先登记者优先**（不是先到先得那么随意）。
// 所有等待挂在一把全局锁 G 上、每线程一个条件变量：先求正确，性能后量（这是参考项目的取舍，也是本项目的）。
#include "dm.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#define R_TIMEOUT     0xEA01u   // 内核模块 1 / 描述 117
#define R_CANCELLED   0xEC01u   // 118
#define R_BAD_HANDLE  0xE401u   // 114
#define R_BAD_STATE   0xFA01u   // 125
#define R_OUT_OF_RES  0xCE01u   // 103
#define WAITERS_BIT   0x40000000u
#define H_CUR_THREAD  0xFFFF8000u
#define MAX_H         4096
#define H_BASE        0x1000u   // 句柄 = 0x1000 + 槽号

enum { O_FREE, O_THREAD, O_EVENT_R, O_EVENT_W };
typedef struct event { int signaled; int refs; } event_t;

typedef struct thr {
    uint32_t handle; uint64_t id;
    uint64_t entry, arg, stack_top; int prio, core; uint64_t mask;
    int started, exited, cancel, closed;
    pthread_t pt; pthread_cond_t cv;
    // 当前等待：kind 0=无 1=锁 2=条件 3=地址 4=同步对象
    int wkind, woke; uint64_t waddr, wkey; uint32_t wtag; int64_t wres;
    uint64_t wseq;                 // 登记等待的序号（同优先级先到先醒）
    struct thr *next;              // 等待链
    uint64_t nsvc;
} thr_t;

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;   // 一把全局锁，护住所有内核对象与等待链
static struct { int type; void *obj; } g_h[MAX_H];
static thr_t *g_wait;
static uint64_t g_next_id = 0x51, g_wseq;
static pthread_key_t g_keythr;
static uint64_t g_guest_ns;                             // 客体时钟（hle_os.c 维护；这里只用于超时口径）

extern void dm_thread_enter(void *entry, void *arg, void *sp);
void guest_thread_init(void);

static uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static uint64_t now_ns(void) { return g_guest_ns ? g_guest_ns : mono_ns(); }
void dm_guest_clock_set(uint64_t ns) { g_guest_ns = ns; }   // hle_os.c 调
uint64_t dm_guest_clock(void) { return g_guest_ns; }

// ---------------------------------------------------------------- 句柄

static uint32_t h_new(int type, void *obj) {            // 调用方持 G
    for (unsigned i = 1; i < MAX_H; i++)
        if (g_h[i].type == O_FREE) { g_h[i].type = type; g_h[i].obj = obj; return H_BASE + i; }
    die("句柄表满");
    return 0;
}
static void *h_peek(uint32_t h, int type) {             // 只查不报
    unsigned i = h - H_BASE;
    return (i > 0 && i < MAX_H && g_h[i].type == type) ? g_h[i].obj : NULL;
}
// 当前在等的客体线程数（可按种类过滤：0=全部 1=锁 2=条件 3=地址 4=同步对象）。
// 诊断用（真机上"卡住时谁在等什么"就靠它），单测用它把时序变成确定的。
unsigned dm_thread_waiting(int kind) {
    unsigned n = 0;
    pthread_mutex_lock(&G);
    for (thr_t *w = g_wait; w; w = w->next) if (!kind || w->wkind == kind) n++;
    pthread_mutex_unlock(&G);
    return n;
}

static void *h_get(uint32_t h, int type) {              // 查不到即报（句柄错是真错误）
    void *p = h_peek(h, type);
    if (!p) die("句柄无效 %#x（期望类型 %d）", h, type);
    return p;
}
static thr_t *cur(void) {
    thr_t *t = pthread_getspecific(g_keythr);
    if (!t) die("在非客体线程上调了线程族 svc");
    return t;
}

// ---------------------------------------------------------------- 等待原语（持 G 调用）

static void wait_unlink(thr_t *t) {
    for (thr_t **p = &g_wait; *p; p = &(*p)->next)
        if (*p == t) { *p = t->next; t->next = NULL; return; }
}
// 按内核顺序挑等待者：优先级数值小者优先，同优先级先登记者优先
static thr_t *pick(int kind, uint64_t a) {
    thr_t *best = NULL;
    for (thr_t *w = g_wait; w; w = w->next) {
        if (w->wkind != kind) continue;
        if (kind == 1 ? w->waddr != a : kind == 2 ? w->wkey != a : w->waddr != a) continue;
        if (!best || w->prio < best->prio || (w->prio == best->prio && w->wseq < best->wseq)) best = w;
    }
    return best;
}
static void wake(thr_t *t, int64_t res) {
    wait_unlink(t);
    t->wkind = 0; t->woke = 1; t->wres = res;
    pthread_cond_signal(&t->cv);
}
static int64_t block(thr_t *t, int kind, int64_t ns) {   // ns<0 无限、0 立即超时
    t->wkind = kind; t->woke = 0; t->wseq = ++g_wseq;
    t->next = g_wait;                                    // 挂到等待链头
    g_wait = t;
    if (ns == 0) { wait_unlink(t); t->wkind = 0; return R_TIMEOUT; }
    if (ns < 0) { while (!t->woke) pthread_cond_wait(&t->cv, &G); }
    else {
        struct timespec dl;
        clock_gettime(CLOCK_REALTIME, &dl);
        uint64_t abs = (uint64_t)dl.tv_sec * 1000000000ull + (uint64_t)dl.tv_nsec + (uint64_t)ns;
        dl.tv_sec = abs / 1000000000ull; dl.tv_nsec = abs % 1000000000ull;
        while (!t->woke) { if (pthread_cond_timedwait(&t->cv, &G, &dl) == ETIMEDOUT) break; }
        if (!t->woke) { wait_unlink(t); t->wkind = 0; return R_TIMEOUT; }
    }
    return t->wres;
}

// ---------------------------------------------------------------- 锁（ArbitrateLock/Unlock）

static inline uint32_t ld32(uint64_t a) { return __atomic_load_n((uint32_t *)(uintptr_t)a, __ATOMIC_SEQ_CST); }
static inline void st32(uint64_t a, uint32_t v) { __atomic_store_n((uint32_t *)(uintptr_t)a, v, __ATOMIC_SEQ_CST); }
static inline int cas32(uint64_t a, uint32_t *exp, uint32_t v) {
    return __atomic_compare_exchange_n((uint32_t *)(uintptr_t)a, exp, v, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// 释放 addr 处的锁：交给等在它上面的第一个线程，或清零（持 G）
static void lock_handoff(uint64_t addr) {
    thr_t *w = pick(1, addr);
    if (w) { w->wkind = 0; wait_unlink(w); st32(addr, w->handle); wake(w, 0); }
    else st32(addr, 0);
}
// 让 t 去拿 addr 处的锁：空闲就直接拿到并唤醒，否则置等待者位并转为锁等待（持 G）
static void lock_acquire_for(thr_t *t, uint64_t addr, uint32_t tag) {
    (void)tag;
    uint32_t e = 0;
    if (cas32(addr, &e, t->handle)) { wake(t, 0); return; }
    t->wkind = 1; t->waddr = addr; t->woke = 0; t->wseq = ++g_wseq;
    st32(addr, ld32(addr) | WAITERS_BIT);
}

// ---------------------------------------------------------------- 线程

static void thr_free(thr_t *t) { pthread_cond_destroy(&t->cv); free(t); }

static void *thread_main(void *p) {
    thr_t *t = p;
    pthread_setspecific(g_keythr, t);
    guest_thread_init();                                  // 每线程 TSD 槽（门、x18、TLS、TLR）唯一设置点
    dm_thread_enter((void *)(uintptr_t)t->entry, (void *)(uintptr_t)t->arg, (void *)(uintptr_t)t->stack_top);
    return NULL;                                          // dm_thread_enter 不返回（返回即 brk）
}

static void thread_exit(thr_t *t) {                       // 调用方持 G
    t->exited = 1;
    for (thr_t *w = g_wait; w; w = w->next) if (w->wkind == 4) wake(w, 0);   // 等同步对象的线程要重新判定
    int free_me = t->closed;                              // 句柄已关 → 这里释放；否则由 CloseHandle 释放
    pthread_mutex_unlock(&G);                             // 退出前必须放锁（否则全局锁永远锁死）
    if (free_me) thr_free(t);
    pthread_exit(NULL);
}

// 客体线程入口函数返回 = 违反 Horizon 语义（必须经 ExitThread 退出）
void dm_thread_returned(void) { die("客体线程入口函数返回了（Horizon 要求 ExitThread 退出）"); }

uint32_t svc_thread_main_init(void) {                     // 主线程登记，交给 nn::os 当主线程句柄
    pthread_mutex_lock(&G);
    if (!g_keythr) pthread_key_create(&g_keythr, NULL);
    thr_t *t = calloc(1, sizeof *t);
    t->handle = h_new(O_THREAD, t);
    t->id = g_next_id++;
    t->prio = 16; t->core = -1; t->mask = 1; t->started = 1;
    pthread_cond_init(&t->cv, NULL);
    pthread_setspecific(g_keythr, t);
    pthread_mutex_unlock(&G);
    return t->handle;
}

// 主线程是否已登记。run_module_inits 之前必须为真：否则客体构造函数拿不到线程/TLS 环境（
// 真机首跑就是这么崩的：App 里 run_module_inits 排在了 svc_thread_main_init 前面）。
int svc_thread_main_ready(void) {
    return g_keythr != 0 && pthread_getspecific(g_keythr) != NULL;
}

// ---------------------------------------------------------------- 同步对象（事件）

static int sig_obj(uint32_t h, int *bad) {                // 该句柄是否处于「已触发」
    event_t *e = h_peek(h, O_EVENT_R);
    if (!e) e = h_peek(h, O_EVENT_W);
    thr_t *t = h_peek(h, O_THREAD);
    if (t) return t->exited;                              // 线程句柄在线程退出后变为已触发
    if (!e) { *bad = 1; return 0; }
    return e->signaled;
}
static void wake_sync_waiters(void) {
    for (thr_t *w = g_wait; w; w = w->next) if (w->wkind == 4) wake(w, 0);
}

// ---------------------------------------------------------------- 分派

int svc_thread(int num, uint64_t *regs) {
    switch (num) {
    case 0x08: {                                          // CreateThread(→w0 句柄; x1 入口, x2 参数, x3 栈顶, w4 优先级, w5 核)
        pthread_mutex_lock(&G);
        thr_t *t = calloc(1, sizeof *t);
        t->handle = h_new(O_THREAD, t);
        t->id = g_next_id++;
        t->entry = regs[1]; t->arg = regs[2]; t->stack_top = regs[3];
        t->prio = (int)regs[4]; t->core = (int)regs[5]; t->mask = 1;
        pthread_cond_init(&t->cv, NULL);
        pthread_mutex_unlock(&G);
        regs[0] = 0; regs[1] = t->handle;
        return 1;
    }
    case 0x09: {                                          // StartThread(w0 句柄)
        thr_t *t = h_get((uint32_t)regs[0], O_THREAD);
        if (t->started) { regs[0] = R_BAD_STATE; return 1; }
        t->started = 1;
        if (pthread_create(&t->pt, NULL, thread_main, t)) die("pthread_create 失败");
        LOG("GUEST_THREAD_STARTED handle=%#x entry=%#llx stack=%#llx", t->handle,
            (unsigned long long)t->entry, (unsigned long long)t->stack_top);
        regs[0] = 0;
        return 1;
    }
    case 0x0A: {                                          // ExitThread()
        pthread_mutex_lock(&G);
        thread_exit(cur());
        return 1;                                          // 不返回
    }
    case 0x0B: {                                          // SleepThread(x0 纳秒；正=睡，0/负=让出)
        int64_t ns = (int64_t)regs[0];
        if (ns > 0) {
            struct timespec ts = { ns / 1000000000ll, ns % 1000000000ll };
            nanosleep(&ts, NULL);
        } else {
            sched_yield();
        }
        regs[0] = 0;
        return 1;
    }
    // 注意寄存器：带输出的 svc，x0 是用户态留给结果码的（libnx 先把输出指针存栈上），
    // **句柄在 w1（GetThreadCoreMask 是 w2）**，输出也从 x1 起。真机第九轮：以前这里把句柄当成
    // 在 x0、优先级又不写回 —— SDK 拿 0x1001（句柄原样留在 x1）当主线程优先级、GetThreadId 报
    // 坏句柄，nnosInitialize 随即 svcBreak。
    case 0x0C: {                                          // GetThreadPriority(w1 句柄) → w1 优先级
        thr_t *t = (uint32_t)regs[1] == H_CUR_THREAD ? cur() : h_peek((uint32_t)regs[1], O_THREAD);
        regs[0] = t ? 0 : R_BAD_HANDLE;
        regs[1] = t ? (uint64_t)t->prio : 0;
        return 1;
    }
    case 0x0D: {                                          // SetThreadPriority(w0 句柄, w1 优先级)
        thr_t *t = (uint32_t)regs[0] == H_CUR_THREAD ? cur() : h_peek((uint32_t)regs[0], O_THREAD);
        if (t) t->prio = (int)regs[1];
        regs[0] = t ? 0 : R_BAD_HANDLE;
        return 1;
    }
    case 0x0E: {                                          // GetThreadCoreMask(w2 句柄) → w1 理想核, x2 掩码
        thr_t *t = (uint32_t)regs[2] == H_CUR_THREAD ? cur() : h_peek((uint32_t)regs[2], O_THREAD);
        regs[0] = t ? 0 : R_BAD_HANDLE;
        regs[1] = t && t->core >= 0 ? (uint64_t)t->core : 0;
        regs[2] = t ? (t->mask ? t->mask : 1) : 0;
        return 1;
    }
    case 0x0F: regs[0] = 0; return 1;                     // SetThreadCoreMask(w0 句柄, w1 核, x2 掩码)
    case 0x10: regs[0] = 0; return 1;                     // GetCurrentProcessorNumber
    case 0x25: {                                          // GetThreadId(w1 句柄；H_CUR_THREAD = 当前线程) → x1
        uint32_t h = (uint32_t)regs[1];
        if (h == H_CUR_THREAD) { regs[0] = 0; regs[1] = cur()->id; return 1; }
        thr_t *t = h_peek(h, O_THREAD);
        regs[0] = t ? 0 : R_BAD_HANDLE;
        regs[1] = t ? t->id : 0;
        return 1;
    }
    case 0x19: {                                          // CancelSynchronization(w0 线程)
        pthread_mutex_lock(&G);
        thr_t *t = h_peek((uint32_t)regs[0], O_THREAD);
        if (t) { t->cancel = 1; if (t->wkind == 4) wake(t, -1); }
        pthread_mutex_unlock(&G);
        regs[0] = t ? 0 : R_BAD_HANDLE;
        return 1;
    }

    case 0x1A: {                                          // ArbitrateLock(w0 当前线程, x1 地址, w2 标签)
        thr_t *t = cur();
        pthread_mutex_lock(&G);
        uint32_t e = 0, tag = (uint32_t)regs[2];
        if (cas32(regs[1], &e, tag)) { pthread_mutex_unlock(&G); regs[0] = 0; return 1; }
        t->waddr = regs[1]; t->wtag = tag;
        st32(regs[1], ld32(regs[1]) | WAITERS_BIT);
        int64_t r = block(t, 1, -1);
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        return 1;
    }
    case 0x1B: {                                          // ArbitrateUnlock(x0 地址)
        pthread_mutex_lock(&G);
        lock_handoff(regs[0]);
        pthread_mutex_unlock(&G);
        regs[0] = 0;
        return 1;
    }
    case 0x1C: {                                          // WaitProcessWideKeyAtomic(x0 地址, x1 条件键, w2 标签, x3 超时)
        thr_t *t = cur();
        uint64_t addr = regs[0], key = regs[1];
        pthread_mutex_lock(&G);
        // 顺序要紧：**先把手上的锁交出去，再置条件键**。
        // 反过来（先置键后放锁）会出现「已经有人 Signal 过、但这里才登记等待」的窗口，
        // 唤醒丢失 —— 参考项目的单测就是拿这个顺序当负对照的。
        lock_handoff(addr);
        st32(key, 1);
        t->wkey = key; t->waddr = addr; t->wtag = (uint32_t)regs[2];
        int64_t r = block(t, 2, (int64_t)regs[3]);        // 超时后不重新拿锁（用户态自己拿）
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        return 1;
    }
    case 0x1D: {                                          // SignalProcessWideKey(x0 条件键, w1 个数；<=0 全部)
        uint64_t key = regs[0];
        int cnt = (int)regs[1], n = 0;
        pthread_mutex_lock(&G);
        for (thr_t *w; (cnt <= 0 || n < cnt) && (w = pick(2, key)); n++) lock_acquire_for(w, w->waddr, w->wtag);
        int left = 0;
        for (thr_t *w = g_wait; w; w = w->next) if (w->wkind == 2 && w->wkey == key) { left = 1; break; }
        if (!left) st32(key, 0);                          // 没人等了才清键（否则会把后来的等待者漏掉）
        pthread_mutex_unlock(&G);
        regs[0] = 0;
        return 1;
    }

    case 0x34: {                                          // WaitForAddress(x0 地址, w1 类型, w2 值, x3 超时)
        thr_t *t = cur();
        uint64_t addr = regs[0];
        int type = (int)regs[1];
        int32_t val = (int32_t)regs[2];
        pthread_mutex_lock(&G);
        int64_t r;
        uint32_t v = ld32(addr);
        int ok = type == 2 ? (int32_t)v == val : (int32_t)v < val;
        if (ok && type == 1) { while (!cas32(addr, &v, v - 1)) if (!((int32_t)v < val)) { ok = 0; break; } }
        if (!ok) r = R_BAD_STATE;
        else { t->waddr = addr; r = block(t, 3, (int64_t)regs[3]); }
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        return 1;
    }
    case 0x35: {                                          // SignalToAddress(x0 地址, w1 类型, w2 值, w3 个数)
        uint64_t addr = regs[0];
        int type = (int)regs[1];
        int32_t val = (int32_t)regs[2];
        int cnt = (int)regs[3];
        pthread_mutex_lock(&G);
        int nw = 0;
        for (thr_t *w = g_wait; w; w = w->next) if (w->wkind == 3 && w->waddr == addr) nw++;
        int64_t r = 0;
        if (type != 0) {
            int32_t nv = val + 1;
            if (type == 2) {
                if (cnt <= 0) nv = nw > 0 ? val - 1 : val + 1;
                else nv = nw == 0 ? val + 1 : (nw <= cnt ? val - 1 : val);
            }
            uint32_t e = (uint32_t)val;
            if (!cas32(addr, &e, (uint32_t)nv)) r = R_BAD_STATE;
        }
        if (!r) { int n = 0; for (thr_t *w; (cnt <= 0 || n < cnt) && (w = pick(3, addr)); n++) wake(w, 0); }
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        return 1;
    }

    case 0x45: {                                          // CreateEvent(→w1 写端, w2 读端)
        event_t *e = calloc(1, sizeof *e);
        e->refs = 2;
        pthread_mutex_lock(&G);
        uint32_t hw = h_new(O_EVENT_W, e), hr = h_new(O_EVENT_R, e);
        pthread_mutex_unlock(&G);
        regs[0] = 0; regs[1] = hw; regs[2] = hr;
        return 1;
    }
    case 0x11: case 0x12: case 0x17: {                    // SignalEvent(写端) / ClearEvent(两端) / ResetSignal(读端，未触发报错)
        uint32_t h = (uint32_t)regs[0];
        pthread_mutex_lock(&G);
        event_t *e = num == 0x11 ? h_peek(h, O_EVENT_W)
                   : num == 0x17 ? h_peek(h, O_EVENT_R)
                                 : (h_peek(h, O_EVENT_W) ? h_peek(h, O_EVENT_W) : h_peek(h, O_EVENT_R));
        int64_t r = 0;
        if (!e) r = (num == 0x17 && h_peek(h, O_THREAD)) ? R_BAD_STATE : R_BAD_HANDLE;
        else if (num == 0x11) { e->signaled = 1; wake_sync_waiters(); }
        else if (num == 0x17 && !e->signaled) r = R_BAD_STATE;
        else e->signaled = 0;
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        return 1;
    }
    case 0x18: {                                          // WaitSynchronization(→w1 下标; x1 句柄数组, w2 个数, x3 超时)
        thr_t *t = cur();
        const uint32_t *hs = (const uint32_t *)(uintptr_t)regs[1];
        int n = (int)regs[2];
        int64_t ns = (int64_t)regs[3];
        int64_t deadline = ns > 0 ? (int64_t)now_ns() + ns : 0;
        pthread_mutex_lock(&G);
        int64_t r;
        int idx = -1;
        for (;;) {
            if (t->cancel) { t->cancel = 0; r = R_CANCELLED; break; }
            int bad = 0;
            for (int i = 0; i < n && idx < 0; i++) if (sig_obj(hs[i], &bad)) idx = i;
            if (bad) { r = R_BAD_HANDLE; break; }
            if (idx >= 0) { r = 0; break; }
            int64_t left = ns < 0 ? -1 : (ns == 0 ? 0 : deadline - (int64_t)now_ns());
            if (ns > 0 && left <= 0) { r = R_TIMEOUT; break; }
            if (block(t, 4, left) == R_TIMEOUT) { r = R_TIMEOUT; break; }
        }
        pthread_mutex_unlock(&G);
        regs[0] = (uint64_t)r;
        regs[1] = idx >= 0 ? (uint64_t)idx : 0;
        return 1;
    }
    case 0x16: {                                          // CloseHandle(w0)
        unsigned i = (uint32_t)regs[0] - H_BASE;
        pthread_mutex_lock(&G);
        if (i > 0 && i < MAX_H && (g_h[i].type == O_EVENT_R || g_h[i].type == O_EVENT_W)) {
            event_t *e = g_h[i].obj;
            if (--e->refs == 0) free(e);                  // 事件两端都关才释放
            g_h[i].type = O_FREE; g_h[i].obj = NULL;
        } else if (i > 0 && i < MAX_H && g_h[i].type == O_THREAD) {
            thr_t *t = g_h[i].obj;
            g_h[i].type = O_FREE; g_h[i].obj = NULL;
            t->closed = 1;
            if (t->exited) thr_free(t);
        }
        pthread_mutex_unlock(&G);
        regs[0] = 0;
        return 1;
    }
    default:
        return 0;
    }
}

// ---------------------------------------------------------------- 服务层的 SystemEvent
//
// 真机：服务把内核事件的读端交给客户端，客体经 SDK 的 nn::os 等待。
// 宿主：在客体线程上调 SDK 自己的 CreateSystemEvent(跨进程)，SDK 会经 svc 0x45 建出内核事件；
// 宿主记下写端句柄，之后任何线程（例如宿主的音频线程）都能触发它。
static struct { void *ev; uint32_t wh; } g_se[512];
static int g_nse;
static pthread_mutex_t g_se_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t se_find(void *ev) {
    for (int i = 0; i < g_nse; i++) if (g_se[i].ev == ev) return g_se[i].wh;
    return 0;
}
void hle_bind_system_event(void *ev) {                     // 必须在客体线程上调
    pthread_mutex_lock(&g_se_lock);
    uint32_t wh = se_find(ev);
    pthread_mutex_unlock(&g_se_lock);
    if (wh) return;
    if (!pthread_getspecific(g_keythr)) die("hle_bind_system_event 须在客体线程上调用");
    static uint32_t (*create)(void *, int, int);
    static uint32_t (*getw)(const void *);
    if (!create) {
        create = guest_sym("_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb");
        getw = guest_sym("_ZN2nn2os30GetWritableHandleOfSystemEventEPKNS0_15SystemEventTypeE");
        if (!create || !getw) die("SDK 里找不到 CreateSystemEvent / GetWritableHandleOfSystemEvent");
    }
    if (create(ev, 0, 1)) die("CreateSystemEvent 失败");
    wh = getw(ev);
    pthread_mutex_lock(&g_se_lock);
    if (g_nse >= (int)(sizeof g_se / sizeof g_se[0])) die("SystemEvent 绑定表满");
    g_se[g_nse].ev = ev;
    g_se[g_nse].wh = wh;
    g_nse++;
    pthread_mutex_unlock(&g_se_lock);
}
void hle_signal_system_event(void *ev) {
    pthread_mutex_lock(&g_se_lock);
    uint32_t wh = se_find(ev);
    pthread_mutex_unlock(&g_se_lock);
    if (!wh) { hle_bind_system_event(ev); pthread_mutex_lock(&g_se_lock); wh = se_find(ev); pthread_mutex_unlock(&g_se_lock); }
    pthread_mutex_lock(&G);
    event_t *e = h_get(wh, O_EVENT_W);
    e->signaled = 1;
    wake_sync_waiters();
    pthread_mutex_unlock(&G);
}
void hle_forget_system_event(void *ev) {
    pthread_mutex_lock(&g_se_lock);
    for (int i = 0; i < g_nse; i++) {
        if (g_se[i].ev == ev) { g_se[i] = g_se[--g_nse]; break; }
    }
    pthread_mutex_unlock(&g_se_lock);
}
