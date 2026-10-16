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

// 极简 UTF-8 校验（够判定「有没有被切在半个字上」）
static int utf8_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; ) {
        unsigned char c = (unsigned char)s[i];
        size_t need = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3
                    : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (!need || i + need > n) return 0;
        for (size_t k = 1; k < need; k++)
            if (((unsigned char)s[i + k] & 0xC0) != 0x80) return 0;
        i += need;
    }
    return 1;
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

    // 「实时落盘」必须真的写文件。上一版只 fopen 没写过 —— 真机崩溃后日志是 0 字节，
    // 首轮真机排查就栽在这。这个断言就是为它写的。
    {
        const char *lf = "/tmp/smo_applogic_log.txt";
        remove(lf);
        dm_log_set_file(lf);
        dm_log_emit("落盘测试 %d", 42);
        dm_log_emit("第二行");
        dm_log_raw("裸写一行（崩溃处理用 write 直写）\n");   // 崩溃现场走的就是这条路径
        dm_log_set_file(NULL);          // 关文件（实现里每行都 flush，这里只是收尾）
        FILE *f = fopen(lf, "rb");
        char txt[512] = {0};
        size_t got = f ? fread(txt, 1, sizeof txt - 1, f) : 0;
        if (f) fclose(f);
        CHK(got > 0, "dm_log_set_file 之后文件里确实有内容（不是 0 字节）");
        CHK(strstr(txt, "落盘测试 42") && strstr(txt, "第二行"), "两行都写进了文件");
        CHK(strstr(txt, "裸写一行") != NULL, "dm_log_raw 也写进了同一个文件（崩溃现场那条路）");
        remove(lf);
        dm_log_clear();
    }

    // 每行上限 200 字节：长中文行必然被截断；截断处必须是合法 UTF-8 边界，
    // 否则 UI 侧 [NSString stringWithUTF8String:] 返回 nil → 插进 NSMutableArray 直接崩
    // （真机第七崩就是这个；「运行久一点才崩」因为要等日志长到切中某个多字节字符）。
    {
        dm_log_clear();
        char big[1024];
        size_t o = 0;
        for (int i = 0; i < 120 && o + 4 < sizeof big; i++) {
            big[o++] = (char)0xE6; big[o++] = (char)0xB1; big[o++] = (char)0x89;   // 「汉」
        }
        big[o] = 0;                                  // 120 个「汉」= 360 字节，必定被切
        dm_log_emit("%s", big);
        char d2[16384];
        size_t n2 = dm_log_dump(d2, sizeof d2);
        CHK(n2 > 0, "长中文行能 dump 出来");
        CHK(utf8_ok(d2, n2), "dump 出来的内容在 UTF-8 边界上（截断不留半个汉字）");
        dm_log_clear();
    }

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
