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

#endif
