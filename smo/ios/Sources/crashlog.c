// 崩溃现场记录（真机）。单独一个编译单元的理由：
// iOS SDK 的 <ucontext.h> 要求先定义 _XOPEN_SOURCE，而把它定义在 main.m 顶部会影响 UIKit /
// Foundation 那一大堆头文件的可见性（_XOPEN_SOURCE 会改 __DARWIN_C_LEVEL）。
// 隔离在这里：只有这一个文件受影响，其它照旧。
#define _XOPEN_SOURCE 700

#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>

#include "logbuf.h"

// 不用 printf 系：崩溃时 stdio 的锁可能正被别的线程拿着，printf/fprintf 会死锁。
// 这里只做最小格式化，然后 dm_log_raw()（只调 write(2)，异步信号安全）写盘。
static char *put_s(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *put_x(char *p, uint64_t v) {
    *p++ = '0'; *p++ = 'x';
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        unsigned d = (unsigned)((v >> i) & 0xF);
        if (d || started || i == 0) { *p++ = (char)(d < 10 ? '0' + d : 'a' + d - 10); started = 1; }
    }
    return p;
}

static const char *SigName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGTRAP: return "SIGTRAP";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        default:      return "SIG?";
    }
}

static void CrashHandler(int sig, siginfo_t *info, void *uctx) {
    char buf[256], *p;
    ucontext_t *uc = (ucontext_t *)uctx;

    p = put_s(buf, "【崩溃】");
    p = put_s(p, SigName(sig));
    p = put_s(p, " 地址 ");
    p = put_x(p, (uint64_t)(uintptr_t)(info ? info->si_addr : 0));
    if (uc) {
        // arm64：x0..x28 在 __ss.__x[]，x29=fp、x30=lr、pc 单列
        p = put_s(p, " PC "); p = put_x(p, (uint64_t)uc->uc_mcontext->__ss.__pc);
        p = put_s(p, " LR "); p = put_x(p, (uint64_t)uc->uc_mcontext->__ss.__lr);
        p = put_s(p, " FP "); p = put_x(p, (uint64_t)uc->uc_mcontext->__ss.__fp);
    }
    *p++ = '\n'; *p = 0;
    dm_log_raw(buf);

    if (uc) {                                  // 帧指针链：看得出是谁调过来的
        uint64_t fp = (uint64_t)uc->uc_mcontext->__ss.__fp;
        for (int i = 0; i < 12 && fp; i++) {
            uint64_t next = *(uint64_t *)fp, ret = *(uint64_t *)(fp + 8);
            p = put_s(buf, "  回溯 "); p = put_x(p, ret);
            p = put_s(p, " (fp "); p = put_x(p, fp); p = put_s(p, ")\n"); *p = 0;
            dm_log_raw(buf);
            if (next <= fp || next - fp > (1u << 20)) break;   // 链断了/不靠谱就停
            fp = next;
        }
    }
    dm_log_raw("（进程马上会被系统收走）\n");

    // 还原默认处理并重发，保持系统的崩溃上报行为（设置 → 分析数据 里照样有 .ips）
    signal(sig, SIG_DFL);
    raise(sig);
}

void dm_crash_install(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = CrashHandler;
    sa.sa_flags = SA_SIGINFO;
    static const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGABRT, SIGFPE };
    for (unsigned i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
        sigaction(sigs[i], &sa, NULL);
}
