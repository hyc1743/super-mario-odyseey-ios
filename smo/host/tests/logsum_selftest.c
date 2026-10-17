// 日志摘要自检：造一份「和真机一样吵」的日志（两万行、绝大多数是同一句话带不同地址），
// 断言摘要①足够小②把同类合并并数对③留住了错误行与最后一行。
#include "logsum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

static long size_of(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

int main(void) {
    const char *src = "/tmp/logsum_test_in.txt";
    const char *dst = "/tmp/logsum_test_out.txt";
    FILE *f = fopen(src, "wb");
    if (!f) { printf("打不开临时文件\n"); return 1; }
    // 20000 行：其中 18000 行是同一类消息（地址不同），另有每 500 行一条【红】，最后一行是停止点
    for (int i = 0; i < 20000; i++) {
        if (i % 500 == 499)
            fprintf(f, "[%8.3f] 【红】测试告警 %d\n", i * 0.001, i);
        else
            fprintf(f, "[%8.3f] NVN_RESOURCE_MISS type=12 object=0x%llx (render command skipped)\n",
                    i * 0.001, 0x7000000000ull + (unsigned long long)i * 16);
    }
    fprintf(f, "[ 20.000] 最后一行：STOP HERE\n");
    fclose(f);

    long in = size_of(src);
    char err[256] = {0};
    int rc = dm_log_summarize(src, dst, err, sizeof err);
    CHK(rc == 0, "摘要生成成功");
    long out = size_of(dst);
    CHK(out > 0 && out < in / 10, "摘要比原日志小一个量级以上");

    char *buf = calloc(1, (size_t)out + 1);
    f = fopen(dst, "rb");
    size_t n = f ? fread(buf, 1, (size_t)out, f) : 0;
    if (f) fclose(f);
    buf[n] = 0;
    CHK(strstr(buf, "NVN_RESOURCE_MISS") != NULL, "摘要里有高频消息");
    CHK(strstr(buf, "×19960") != NULL || strstr(buf, "×19961") != NULL, "同类消息合并计数正确（19960 次）");
    CHK(strstr(buf, "【红】") != NULL, "错误行被保留");
    CHK(strstr(buf, "STOP HERE") != NULL, "最后一行原文被保留");
    CHK(strstr(buf, "不同消息") != NULL, "摘要头部有统计信息");
    free(buf);

    // 空日志也要能出摘要（不能崩在除零/空指针上）
    FILE *empty = fopen(src, "wb"); fclose(empty);
    err[0] = 0;
    rc = dm_log_summarize(src, dst, err, sizeof err);
    CHK(rc == 0, "空日志也能摘要");
    // 打不开的源：明确报错
    rc = dm_log_summarize("/tmp/definitely-not-there-xyz.txt", dst, err, sizeof err);
    CHK(rc == -1 && err[0], "源日志不存在 → 返回 -1 并给出原因");

    remove(src);
    remove(dst);
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
