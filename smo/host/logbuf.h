// 宿主日志的环形缓冲：LOG 全量写 stderr，同时把最近 N 行留在内存里。
// 用途：iOS 真机上没有终端 —— App 把这段内存显示在屏幕上（main.m 定时刷新），
// 未接电脑也能看 UNIMPL / FATAL / 进度。平台无关，可在 CI 里测。
#ifndef SMO_LOGBUF_H
#define SMO_LOGBUF_H
#include <stddef.h>

// 写入一条日志（dm.h 的 LOG 宏调它）。线程安全由调用方保证（目前单线程使用）。
void dm_log_emit(const char *fmt, ...);

// 把环形缓冲里的内容拼成一段文本（行序从旧到新）。返回字节数。
size_t dm_log_dump(char *out, size_t cap);

// 缓冲里的行数（自检/诊断用）。
unsigned dm_log_lines(void);
void dm_log_clear(void);

#endif
