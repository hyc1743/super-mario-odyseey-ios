// 日志摘要：把几 MB 的导出日志压成几十 KB 的可读摘要（真机排查用）。
//
// 为什么需要：渲染层接上后日志涨到 5 MB 量级（每帧几十行 NVN_RESOURCE_MISS / SUBMITTED…），
// 而其中 99% 是同一句话重复。真正有用的信息只有三类：
//   ① 各类消息出现了多少次（同一条消息里的地址/序号不同，应该归成一类）
//   ② 错误/降级类的那几行（【红】/FATAL/REJECT/SKIPPED/中止/UNIMPL…）
//   ③ 最后若干行原文（看停在哪里）
//
// 归一化规则：`0x` 开头的十六进制串与两位以上的十进制数都替换成 `#`，
// 于是 `NVN_RESOURCE_MISS type=12 object=0x70f5197ce8` 与 `… object=0x16b2f6f20`
// 会归成同一类，计数累加。平台无关，可在 CI 自检。
#include "logsum.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_GROUPS 4096
#define KEY_CHARS  200
#define TAIL_LINES 120
#define ERR_LINES  400

typedef struct {
    char key[KEY_CHARS];
    unsigned count;
    double first_ts, last_ts;
    char sample[KEY_CHARS];
} group;

static group g_groups[MAX_GROUPS];
static unsigned g_ngroups;

static unsigned hash_key(const char *s) {
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

// 归一化：数字与十六进制串 → '#'。返回写入长度。
static size_t normalize(const char *in, char *out, size_t cap) {
    size_t n = 0;
    const char *p = in;
    while (*p && n + 1 < cap) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {          // 0xABC → 0x#
            out[n++] = '0'; out[n++] = 'x'; out[n++] = '#';
            p += 2;
            while (isxdigit((unsigned char)*p)) p++;
            continue;
        }
        if (isdigit((unsigned char)*p)) {                           // 123 → #
            const char *q = p;
            while (isdigit((unsigned char)*q)) q++;
            if (q - p >= 2) { out[n++] = '#'; p = q; continue; }    // 一位数保留（步号、类型号）
        }
        out[n++] = *p++;
    }
    out[n] = 0;
    return n;
}

// 从 "[  12.345] 正文" 里取时间戳（没有就返回 -1）
static double parse_ts(const char *line, const char **body) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '[') { if (body) *body = line; return -1; }
    char *end = NULL;
    double ts = strtod(p + 1, &end);
    if (!end || *end != ']') { if (body) *body = line; return -1; }
    if (body) *body = end + 1;
    return ts;
}

static int interesting(const char *s) {
    static const char *keys[] = {"【红】", "FATAL", "REJECT", "SKIPPED", "崩溃", "中止", "UNIMPL",
                                 "RESOURCE_MISS", "SUBMIT_FAILED", "UNAVAILABLE", "ObjC 异常", "AUDIO_SINK"};
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (strstr(s, keys[i])) return 1;
    return 0;
}

int dm_log_summarize(const char *src_path, const char *dst_path, char *err, size_t err_n) {
    FILE *in = fopen(src_path, "rb");
    if (!in) { if (err) snprintf(err, err_n, "打不开源日志"); return -1; }
    g_ngroups = 0;
    memset(g_groups, 0, sizeof g_groups);

    char line[4096];
    char norm[KEY_CHARS];
    char **tail = calloc(TAIL_LINES, sizeof(char *));
    char **errs = calloc(ERR_LINES, sizeof(char *));
    unsigned ntail = 0, nerrs = 0, total = 0;
    double first_ts = -1, last_ts = -1;
    if (!tail || !errs) { fclose(in); free(tail); free(errs); if (err) snprintf(err, err_n, "内存不足"); return -1; }

    while (fgets(line, sizeof line, in)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        if (!len) continue;
        total++;
        const char *body = line;
        double ts = parse_ts(line, &body);
        if (ts >= 0) { if (first_ts < 0) first_ts = ts; last_ts = ts; }
        normalize(body, norm, sizeof norm);
        // 归类
        unsigned slot = hash_key(norm) % MAX_GROUPS;
        for (unsigned probe = 0; probe < 64; probe++) {
            group *g = &g_groups[(slot + probe) % MAX_GROUPS];
            if (!g->key[0]) {
                snprintf(g->key, sizeof g->key, "%s", norm);
                snprintf(g->sample, sizeof g->sample, "%s", body);
                g->count = 1; g->first_ts = ts; g->last_ts = ts;
                if (++g_ngroups > MAX_GROUPS * 3 / 4) { /* 表太满：后续只累加已有类 */ }
                break;
            }
            if (!strcmp(g->key, norm)) {
                g->count++; g->last_ts = ts;
                break;
            }
        }
        if (interesting(body)) {
            if (nerrs < ERR_LINES) errs[nerrs++] = strdup(line);
            else { free(errs[0]); memmove(errs, errs + 1, sizeof(char *) * (ERR_LINES - 1)); errs[ERR_LINES - 1] = strdup(line); }
        }
        if (ntail < TAIL_LINES) tail[ntail++] = strdup(line);
        else { free(tail[0]); memmove(tail, tail + 1, sizeof(char *) * (TAIL_LINES - 1)); tail[TAIL_LINES - 1] = strdup(line); }
    }
    fclose(in);

    FILE *out = fopen(dst_path, "wb");
    if (!out) {
        for (unsigned i = 0; i < nerrs; i++) free(errs[i]);
        for (unsigned i = 0; i < ntail; i++) free(tail[i]);
        free(errs); free(tail);
        if (err) snprintf(err, err_n, "写不了摘要文件");
        return -2;
    }
    fprintf(out, "== 日志摘要 ==\n源：%s\n共 %u 行；时间范围 %.3f ~ %.3f 秒；不同消息 %u 类\n",
            src_path, total, first_ts < 0 ? 0 : first_ts, last_ts < 0 ? 0 : last_ts, g_ngroups);
    fprintf(out, "\n== 出现最多的消息（数字/地址已归一成 #）==\n");
    // 按次数降序（简单选择排序，类数不大）
    for (unsigned round = 0; round < 200 && round < g_ngroups; round++) {
        group *best = NULL;
        for (unsigned i = 0; i < MAX_GROUPS; i++) {
            group *g = &g_groups[i];
            if (!g->key[0] || g->count == 0) continue;
            if (!best || g->count > best->count) best = g;
        }
        if (!best) break;
        fprintf(out, "  ×%-6u [%8.3f ~ %8.3f] %s\n", best->count,
                best->first_ts < 0 ? 0 : best->first_ts, best->last_ts < 0 ? 0 : best->last_ts, best->sample);
        best->count = 0;                     // 已输出
    }
    fprintf(out, "\n== 错误/降级类（按出现顺序，最多 %d 行）==\n", ERR_LINES);
    for (unsigned i = 0; i < nerrs; i++) fprintf(out, "%s\n", errs[i]);
    fprintf(out, "\n== 最后 %d 行原文 ==\n", TAIL_LINES);
    for (unsigned i = 0; i < ntail; i++) fprintf(out, "%s\n", tail[i]);
    fclose(out);

    for (unsigned i = 0; i < nerrs; i++) free(errs[i]);
    for (unsigned i = 0; i < ntail; i++) free(tail[i]);
    free(errs); free(tail);
    return 0;
}
