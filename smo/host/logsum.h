#pragma once
#include <stddef.h>
// 把几 MB 的导出日志压成几十 KB 的可读摘要（同类消息合并计数 + 错误行 + 尾部原文）。
// 返回 0 = 成功；-1 = 打不开源日志；-2 = 写不了摘要（err 里有人话原因）。
// 平台无关（只用 stdio），所以进 CI 自检；App 的「导出日志」会同时产出摘要，真机回传用它就够。
int dm_log_summarize(const char *src_path, const char *dst_path, char *err, size_t err_n);
