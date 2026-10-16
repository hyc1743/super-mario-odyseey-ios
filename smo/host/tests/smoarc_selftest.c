// 资源容器展开器自检：手工构造两个卷（含子目录、CRC 校验、卷尾标记），展开并逐字节比对。
#include "smoarc.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define SMO_MKDIR(p) _mkdir(p)
#else
#define SMO_MKDIR(p) mkdir((p), 0755)
#endif

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

extern uint32_t crc_buf(const void *p, size_t n);   // 来自 smoarc.c（测试自己构造卷，共用同一份算法）

static void put(FILE *f, const char *name, const uint8_t *data, size_t n) {
    uint32_t c = crc_buf(data, n);
    uint16_t nl16 = (uint16_t)strlen(name);
    fwrite(&nl16, 2, 1, f);
    uint32_t sz = (uint32_t)n, crc = c;
    fwrite(&sz, 4, 1, f);
    fwrite(&crc, 4, 1, f);
    fwrite(name, 1, nl16, f);
    fwrite(data, 1, n, f);
}

int main(void) {
    SMO_MKDIR("/tmp/smoarc_test");   // mingw 的 /tmp 与 Git Bash 不同，自己建
    printf("== 构造两卷（v0: a.txt + 子目录; v1: b.bin + END 标记）==\n");
    static uint8_t dataA[300], dataB[100];
    for (int i = 0; i < 300; i++) dataA[i] = (uint8_t)(i * 3);
    for (int i = 0; i < 100; i++) dataB[i] = (uint8_t)(255 - i);
    FILE *v0 = fopen("/tmp/smoarc_test/smoa.0", "wb");
    FILE *v1 = fopen("/tmp/smoarc_test/smoa.1", "wb");
    if (!v0 || !v1) { printf("  打不开临时文件\n"); return 1; }
    fwrite("SMOA", 1, 4, v0); fputc(0, v0); fputc(2, v0);
    put(v0, "a.txt", dataA, sizeof dataA);
    put(v0, "sub/dir.txt", dataA, 64);
    fwrite("END!", 1, 4, v0);
    fwrite("SMOA", 1, 4, v1); fputc(1, v1); fputc(2, v1);
    put(v1, "b.bin", dataB, sizeof dataB);
    fwrite("END!", 1, 4, v1);
    fclose(v0);
    fclose(v1);

    printf("== 展开 ==\n");
    static const char *vols[] = { "/tmp/smoarc_test/smoa.0", "/tmp/smoarc_test/smoa.1" };
    int r = smoarc_extract("/tmp/smoarc_test/out", vols, 2);
    CHK(r == 0, "展开返回 0");

    printf("== 逐字节比对 ==\n");
    FILE *a = fopen("/tmp/smoarc_test/out/a.txt", "rb");
    FILE *s = fopen("/tmp/smoarc_test/out/sub/dir.txt", "rb");
    FILE *b = fopen("/tmp/smoarc_test/out/b.bin", "rb");
    CHK(a && s && b, "三个文件都展开了（含子目录）");
    if (a && s && b) {
        uint8_t buf[512];
        size_t na = fread(buf, 1, sizeof buf, a);
        CHK(na == sizeof dataA && !memcmp(buf, dataA, sizeof dataA), "a.txt 逐字节一致");
        size_t ns = fread(buf, 1, sizeof buf, s);
        CHK(ns == 64 && !memcmp(buf, dataA, 64), "子目录文件逐字节一致");
        size_t nb = fread(buf, 1, sizeof buf, b);
        CHK(nb == sizeof dataB && !memcmp(buf, dataB, sizeof dataB), "b.bin 逐字节一致");
        fclose(a); fclose(s); fclose(b);
    }

    printf("== 校验失败要报错 ==\n");
    FILE *bad = fopen("/tmp/smoarc_test/smoa.0", "r+b");
    fseek(bad, 6 + 2 + 4 + 4 + 1 + 10, SEEK_SET);      // a.txt 数据中段
    fputc(0xEE, bad);
    fclose(bad);
    static const char *badvols[] = { "/tmp/smoarc_test/smoa.0", "/tmp/smoarc_test/smoa.1" };
    CHK(smoarc_extract("/tmp/smoarc_test/out2", badvols, 2) != 0, "CRC 不匹配 → 报错");
    CHK(strstr(smoarc_error(), "CRC") != NULL, "错误原因说清楚是 CRC（不是光一个 -2）");

    printf("== 卷组规划（决定「在哪儿展开」—— 真机首次就错在这一步）==\n");
    // 上一版只认「目录里没有 RomFS 子目录」的形状，而我们自己打的包是
    // SMO Resource/RomFS/RomFS.smoa.N —— 结果手机上根本没展开，游戏一读文件就崩。
    // 三种形状都造出来，逐个验。
    const char *roots[3]   = { "/tmp/smo_plan_a", "/tmp/smo_plan_b", "/tmp/smo_plan_c" };
    const char *voldirs[3] = { "/tmp/smo_plan_a", "/tmp/smo_plan_b/wrap", "/tmp/smo_plan_c/pack/RomFS" };
    const char *names[3]   = { "形状①直接拷卷", "形状②包装目录下", "形状③包装/RomFS 里（我们的包）" };
    SMO_MKDIR("/tmp/smo_plan_a");
    SMO_MKDIR("/tmp/smo_plan_b");
    SMO_MKDIR("/tmp/smo_plan_b/wrap");
    SMO_MKDIR("/tmp/smo_plan_c");
    SMO_MKDIR("/tmp/smo_plan_c/pack");
    SMO_MKDIR("/tmp/smo_plan_c/pack/RomFS");
    for (int i = 0; i < 3; i++) {
        char p[512];
        for (int v = 0; v < 2; v++) {
            snprintf(p, sizeof p, "%s/RomFS.smoa.%d", voldirs[i], v);
            FILE *f = fopen(p, "wb");
            if (f) { fputs("SMOA", f); fclose(f); }
        }
    }
    for (int i = 0; i < 3; i++) {
        char vols[8][1024], target[1024], base[1024];
        unsigned n = smoarc_plan(roots[i], vols, 8, target, sizeof target, base, sizeof base);
        char msg[1600];   /* target/base 是 1024 字节缓冲：msg 小了会被 -Wformat-truncation 当错误 */
        snprintf(msg, sizeof msg, "%s：找到 2 卷", names[i]);
        CHK(n == 2, msg);
        snprintf(msg, sizeof msg, "%s：目标是 %s", names[i], target);
        CHK(strlen(target) > 6 && !strcmp(target + strlen(target) - 6, "/RomFS"), msg);
        snprintf(msg, sizeof msg, "%s：第二卷是 %s", names[i], vols[1]);
        CHK(strstr(vols[1], "RomFS.smoa.1") != NULL, msg);
        snprintf(msg, sizeof msg, "%s：标记目录 base = %s", names[i], base);
        CHK(strlen(base) > 0, msg);
    }
    {
        // 多卷（12 卷）：1 GB 分卷后会有 6 卷，留足余量；上限由调用方给（App 里是 24）
        SMO_MKDIR("/tmp/smo_plan_many");
        SMO_MKDIR("/tmp/smo_plan_many/RomFS");
        for (int v = 0; v < 12; v++) {
            char p2[512];
            snprintf(p2, sizeof p2, "/tmp/smo_plan_many/RomFS/RomFS.smoa.%d", v);
            FILE *f = fopen(p2, "wb");
            if (f) { fputs("SMOA", f); fclose(f); }
        }
        char vols[24][1024], target[1024], base[1024];
        unsigned n = smoarc_plan("/tmp/smo_plan_many", vols, 24, target, sizeof target, base, sizeof base);
        CHK(n == 12, "12 个卷能全部认出来（App 里上限 24）");
        CHK(strstr(vols[11], "RomFS.smoa.11") != NULL, "第 12 个卷的路径正确");
    }
    {
        // 只有展开好的目录树、没有卷（smoarc_test/out 是上面展开出来的）→ 0，不该重复展开
        char vols[8][1024], target[1024], base[1024];
        CHK(smoarc_plan("/tmp/smoarc_test/out", vols, 8, target, sizeof target, base, sizeof base) == 0,
            "只有目录树没有卷 → 0（不该重复展开）");
    }

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
