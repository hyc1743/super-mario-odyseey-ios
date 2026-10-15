// App 侧逻辑的单测（日志环形缓冲 + RomFS/ExeFS 路径解析）。不需要游戏文件：目录树自己造。
// 注意：清理不用 system("rm ...") —— mingw 下 system 走 cmd，rm 不存在；这里用可移植的 rmdir。
#include "dm.h"
#include "../logbuf.h"
#include "../romfs_resolve.h"
#include <sys/stat.h>
#include <unistd.h>       // rmdir（macOS 的 C99 不会经 stdio 传递声明）
#ifdef _WIN32
#include <direct.h>
#define SMO_MKDIR(p) _mkdir(p)
#define SMO_RMDIR(p) _rmdir(p)
#else
#define SMO_MKDIR(p) mkdir((p), 0755)
#define SMO_RMDIR(p) rmdir(p)
#endif

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

static void mkp(const char *fmt, const char *a, const char *b) {
    char p[512];
    snprintf(p, sizeof p, fmt, a, b);
    SMO_MKDIR(p);
}

int main(void) {
    printf("== 1. 日志环形缓冲 ==\n");
    dm_log_clear();
    CHK(dm_log_lines() == 0, "初始为空");
    for (int i = 0; i < 5; i++) dm_log_emit("第 %d 行", i);
    CHK(dm_log_lines() == 5, "5 行在缓冲里");
    char dump[16384];      // 512 行 × ~12 字节 ≈ 6 KB，缓冲要放得下
    size_t n = dm_log_dump(dump, sizeof dump);
    CHK(n > 0 && strstr(dump, "第 0 行") && strstr(dump, "第 4 行"), "dump 含第一行与最后一行");
    for (int i = 100; i < 700; i++) dm_log_emit("环绕 %d", i);          // 600 > 512，最旧的被挤掉
    CHK(dm_log_lines() == 512, "缓冲满后停在 512 行");
    n = dm_log_dump(dump, sizeof dump);
    CHK(strstr(dump, "环绕 188") != NULL, "600-512=88：环绕 188 是最旧的一行");
    CHK(strstr(dump, "环绕 187") == NULL, "更旧的被挤掉了");
    CHK(strstr(dump, "环绕 699") != NULL, "最新一行在末尾");
    dm_log_clear();

    printf("== 2. RomFS / ExeFS 路径解析 ==\n");
    const char *root = "/tmp/smo_applogic_test";
    SMO_RMDIR(root);
    SMO_MKDIR(root);
    mkp("%s/RomFS", root, "");
    mkp("%s/wrap", root, "");
    mkp("%s/wrap/RomFS", root, "");
    mkp("%s/only", root, "");
    mkp("%s/only/ExeFS", root, "");

    char p[1024];
    CHK(dm_romfs_path(p, sizeof p, root) == 1, "直接放根目录的 RomFS 能找到");
    CHK(strstr(p, "RomFS") != NULL, "路径以 RomFS 结尾");   // 不写死分隔符：Windows 是反斜杠

    SMO_RMDIR("/tmp/smo_applogic_test/RomFS");
    CHK(dm_romfs_path(p, sizeof p, root) == 1 && strstr(p, "wrap") && strstr(p, "RomFS"), "一层包装目录里的 RomFS 也能找到");
    CHK(dm_exefs_path(p, sizeof p, root) == 1 && strstr(p, "only") && strstr(p, "ExeFS"), "ExeFS 同样规则");

    SMO_RMDIR("/tmp/smo_applogic_test/wrap/RomFS");
    SMO_RMDIR("/tmp/smo_applogic_test/only/ExeFS");
    CHK(dm_romfs_path(p, sizeof p, root) == 0 && dm_exefs_path(p, sizeof p, root) == 0, "都没有 → 0");
    SMO_RMDIR("/tmp/smo_applogic_test/wrap");
    SMO_RMDIR("/tmp/smo_applogic_test/only");
    SMO_RMDIR(root);

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
