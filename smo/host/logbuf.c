// 日志环形缓冲的实现。见 logbuf.h。
#include "logbuf.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOGBUF_LINES 512          // 屏上日志 512 行足够排查一轮启动
#define LOGBUF_LINE  200

static char g_lines[LOGBUF_LINES][LOGBUF_LINE];
static unsigned g_head, g_n;
static FILE *g_file;                  // 实时落盘（崩溃也留得下）

void dm_log_set_file(const char *path) {
    if (g_file) fclose(g_file);
    g_file = path ? fopen(path, "w") : NULL;      // 每次启动新开（覆盖上一轮）
}

void dm_log_emit(const char *fmt, ...) {
    char *slot = g_lines[g_head];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(slot, LOGBUF_LINE, fmt, ap);
    va_end(ap);
    slot[LOGBUF_LINE - 1] = 0;
    fprintf(stderr, "[smo] %s\n", slot);
    // 真机崩溃时环形缓冲会随进程消失 —— 每行同时写盘并 flush，崩溃后文件里一定留得下尾部。
    // （上一版这里只 fopen 没写过：所谓「实时落盘」是空的，第一次真机排查才发现。）
    if (g_file) {
        fprintf(g_file, "%s\n", slot);
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
        size_t n = strlen(l);
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
