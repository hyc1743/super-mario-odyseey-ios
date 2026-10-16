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

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
