// 日志环形缓冲的实现。见 logbuf.h。
#include "logbuf.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define LOGBUF_LINES 512          // 屏上日志 512 行足够排查一轮启动
#define LOGBUF_LINE  200

// 把 n 字节的串尾修到合法 UTF-8 边界（最多回退 3 字节）。
// 为什么需要：每行上限 200 字节，中文/emoji 被切在半个字上时，UI 侧
// [NSString stringWithUTF8String:] 会返回 nil —— App 把 nil 插进 NSMutableArray 会抛
// NSInvalidArgumentException 直接终止进程（真机第七崩就是这么来的；「运行久一点才崩」
// 是因为要等日志长到切中某个多字节字符）。
static size_t utf8_trim(const char *s, size_t n) {
    for (size_t back = 1; back <= 3 && back <= n; back++) {
        unsigned char c = (unsigned char)s[n - back];
        if ((c & 0xC0) == 0x80) continue;              // 续字节：继续往前找起始字节
        size_t need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        return (back == need) ? n : (n - back);        // 序列完整就原样；不完整就把这半个字丢掉
    }
    return n;
}

// 自启动以来的秒数：落盘的那份日志每行带时间戳（导出给人看时要知道「卡在哪一步卡了多久」）
static double now_s(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}
static double g_t0;
static double elapsed_s(void) {
    if (g_t0 == 0) g_t0 = now_s();
    return now_s() - g_t0;
}

// 进度状态。写方是宿主后台线程、读方是 UI 主线程：不加锁 —— 最坏情况是 UI 读到半句说明，
// 下一秒就刷新掉；字符串都在固定缓冲里且以 0 结尾（UI 侧对非法 UTF-8 有兜底），不会越界。
static dm_progress_t g_prog = { .frac = -1 };

static char g_lines[LOGBUF_LINES][LOGBUF_LINE];
static unsigned g_head, g_n;
static FILE *g_file;                  // 实时落盘（崩溃也留得下）
static int g_fd = -1;                 // 同一份文件的裸 fd：崩溃处理用 write 直写

void dm_log_set_file(const char *path) {
    if (g_file) fclose(g_file);
    if (g_fd >= 0) close(g_fd);
    g_file = path ? fopen(path, "w") : NULL;      // 每次启动新开（覆盖上一轮）
    // O_APPEND：裸写总是落在文件末尾，不会跟 FILE* 的偏移互相覆盖（两者是各自独立的文件描述）
    g_fd = path ? open(path, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;
}

void dm_log_raw(const char *s) {
    if (g_fd >= 0 && s) {
        ssize_t r = write(g_fd, s, strlen(s));
        (void)r;
    }
}

void dm_log_emit(const char *fmt, ...) {
    char *slot = g_lines[g_head];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(slot, LOGBUF_LINE, fmt, ap);
    va_end(ap);
    slot[LOGBUF_LINE - 1] = 0;
    slot[utf8_trim(slot, strlen(slot))] = 0;      // 截断时别留下半个汉字
    fprintf(stderr, "[smo] %s\n", slot);
    if (strstr(slot, "【红】")) g_prog.red++;
    g_prog.lines++;
    g_prog.last_log = elapsed_s();
    // 真机崩溃时环形缓冲会随进程消失 —— 每行同时写盘并 flush，崩溃后文件里一定留得下尾部。
    // （上一版这里只 fopen 没写过：所谓「实时落盘」是空的，第一次真机排查才发现。）
    if (g_file) {
        fprintf(g_file, "[%8.3f] %s\n", elapsed_s(), slot);   // 文件里带时间戳；屏上不带（省地方）
        fflush(g_file);
    }
    g_head = (g_head + 1) % LOGBUF_LINES;
    if (g_n < LOGBUF_LINES) g_n++;
}

size_t dm_log_dump(char *out, size_t cap) {
    if (!out || !cap) return 0;
    size_t used = 0;
    unsigned oldest = (g_head + LOGBUF_LINES - g_n) % LOGBUF_LINES;
    for (unsigned i = 0; i < g_n; i++) {
        const char *l = g_lines[(oldest + i) % LOGBUF_LINES];
        size_t n = utf8_trim(l, strlen(l));       // 双保险：老行也过一遍
        if (used + n + 2 <= cap) {
            memcpy(out + used, l, n);
            out[used + n] = '\n';
            used += n + 1;
        } else {
            break;                          // 装不下就截断（旧行优先）
        }
    }
    if (used < cap) out[used] = 0;
    return used;
}

unsigned dm_log_lines(void) { return g_n; }
void dm_log_clear(void) { g_head = 0; g_n = 0; }

// ---- 运行进度 ----
static void copy_utf8(char *dst, size_t cap, const char *src) {
    snprintf(dst, cap, "%s", src ? src : "");
    dst[utf8_trim(dst, strlen(dst))] = 0;          // 截断处别留半个汉字（UI 侧 NSString 会给 nil）
}

void dm_progress_stage(int step, int steps, const char *name) {
    g_prog.step = step;
    g_prog.steps = steps;
    g_prog.frac = -1;
    g_prog.detail[0] = 0;
    copy_utf8(g_prog.name, sizeof g_prog.name, name);
    dm_log_emit("== 第 %d/%d 步：%s ==", step, steps, g_prog.name);   // 导出的日志里也看得到分段
}

void dm_progress_frac(double frac, const char *detail) {
    g_prog.frac = frac > 1 ? 1 : frac;
    if (detail) copy_utf8(g_prog.detail, sizeof g_prog.detail, detail);
}

void dm_progress_finish(dm_run_state st, const char *msg) {
    // FAIL 一旦报出就不被后面的 WAIT 盖掉：先出错再「走到头」时，用户该看到的是出错
    if (g_prog.state == DM_RUN_FAIL && st != DM_RUN_FAIL) return;
    g_prog.state = st;
    if (st == DM_RUN_WAIT) g_prog.frac = 1;
    copy_utf8(g_prog.msg, sizeof g_prog.msg, msg);
    dm_log_emit("%s %s", st == DM_RUN_FAIL ? "【红】【停下】" : "【到头】", g_prog.msg);
}

void dm_progress_fatal(void) { g_prog.state = DM_RUN_FAIL; }

void dm_progress_get(dm_progress_t *out) {
    if (!out) return;
    *out = g_prog;
    if (out->state == DM_RUN_FAIL && !out->msg[0])
        snprintf(out->msg, sizeof out->msg, "程序在「%s」这一步崩了", out->name[0] ? out->name : "启动");
    out->elapsed = elapsed_s();
}
