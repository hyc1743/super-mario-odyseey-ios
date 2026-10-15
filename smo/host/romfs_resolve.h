// RomFS 路径解析：在 App 自己的 Documents 目录里找游戏文件。
// 装包口径与参考项目一致（资源由用户经「文件」App / Finder 放进 App 的目录）：
//   接受 `Documents/RomFS` 或 `Documents/<任意目录>/RomFS` 两种放法。
// 平台无关（用普通文件系统 API），CI 里用临时目录树测。
#ifndef SMO_ROMFS_RESOLVE_H
#define SMO_ROMFS_RESOLVE_H
#include <stddef.h>

// 在 docs_dir 下找 RomFS 目录，写出完整路径。返回 1 = 找到，0 = 没找到。
int dm_romfs_path(char *out, size_t n, const char *docs_dir);

// 同样的规则找 ExeFS（内嵌装载上线前，开发期从文件加载会用）。
int dm_exefs_path(char *out, size_t n, const char *docs_dir);

#endif
