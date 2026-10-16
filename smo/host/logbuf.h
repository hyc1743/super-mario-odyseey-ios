// 宿主日志的环形缓冲：LOG 全量写 stderr，同时把最近 N 行留在内存里。
// 用途：iOS 真机上没有终端 —— App 把这段内存显示在屏幕上（main.m 定时刷新），
// 未接电脑也能看 UNIMPL / FATAL / 进度。平台无关，可在 CI 里测。
#ifndef SMO_LOGBUF_H
#define SMO_LOGBUF_H
#include <stddef.h>

// 把日志同时追加写入该文件（App 启动时设为 Documents/SMO-log-live.txt）。
// 真机上崩溃时环形缓冲会随进程消失，落盘的那份能留下来 —— 首轮排查靠它。
void dm_log_set_file(const char *path);

// 写入一条日志（dm.h 的 LOG 宏调它）。线程安全由调用方保证（目前单线程使用）。
void dm_log_emit(const char *fmt, ...);

// 直接写盘（只用 write(2)，异步信号安全）。用途：崩溃信号处理里打现场 ——
// 那时 stdio 的锁可能正被拿着，printf/fprintf 会死锁在别的线程上。
void dm_log_raw(const char *s);

// 把环形缓冲里的内容拼成一段文本（行序从旧到新）。返回字节数。
size_t dm_log_dump(char *out, size_t cap);

// 缓冲里的行数（自检/诊断用）。
unsigned dm_log_lines(void);
void dm_log_clear(void);

// ---- 运行进度（给 App 首屏的进度条与「该不该导出日志」提示用）----
// 宿主在后台线程里报「第几步 / 共几步 / 这一步叫什么 / 这一步做了百分之几」，UI 每秒读一次。
// 为什么放在 logbuf：它和日志一样被宿主各处调用、又要进 CI 自检，而 logbuf 已经链进所有目标。
typedef enum {
    DM_RUN_BUSY = 0,      // 正在做事：等着就行
    DM_RUN_WAIT,          // 走到了目前能走的最远处（游戏在跑但还没画面）：可以导出日志
    DM_RUN_FAIL,          // 出错停下了：请导出日志
} dm_run_state;

void dm_progress_stage(int step, int steps, const char *name);   // 进入新的一步（进度归零）
void dm_progress_frac(double frac, const char *detail);          // 本步进度 0..1（<0 = 不确定）+ 一句说明
void dm_progress_finish(dm_run_state st, const char *msg);       // 结束：WAIT / FAIL + 给用户的一句话
typedef struct {
    int step, steps;
    double frac;           // <0 = 不确定（UI 画来回滚动的条）
    dm_run_state state;
    unsigned red;          // 截至目前打出的【红】行数
    unsigned lines;        // 截至目前的日志总行数（环形缓冲只留 512 行，这里不封顶）
    double last_log;       // 最后一行日志的时刻（自启动秒数）：长时间不动 = 卡住了
    double elapsed;        // 自启动以来的秒数
    char name[96], detail[160], msg[200];
} dm_progress_t;
void dm_progress_get(dm_progress_t *out);
// 只置「出错」标志、不格式化不加锁：崩溃信号处理里也能调（异步信号安全）
void dm_progress_fatal(void);

#endif
