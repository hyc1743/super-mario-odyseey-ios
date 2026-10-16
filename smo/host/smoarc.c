// 资源容器的展开实现。见 smoarc.h。
#include "smoarc.h"
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>     // _mkdir
#else
#include <unistd.h>
#endif

// CRC32（IEEE，查表法）：不依赖 zlib（mingw 没带头；iOS 上也无妨）
static uint32_t crc_table[256];
static void crc_init(void) {
    static int done;
    if (done) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    done = 1;
}
uint32_t crc_buf(const void *p, size_t n) {
    crc_init();
    uint32_t c = 0xFFFFFFFFu;
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

static int r16(FILE *f) {
    uint8_t b[2];
    return fread(b, 1, 2, f) == 2 ? (b[0] | (b[1] << 8)) : -1;
}
static int r32(FILE *f) {
    uint8_t b[4];
    return fread(b, 1, 4, f) == 4 ? (int)(b[0] | (b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24)) : -1;
}
static int r64(FILE *f) {
    int lo = r32(f), hi = r32(f);
    if (lo < 0 || hi < 0) return -1;
    return lo;                       // 大小用低 32 位就够（单条 ≤ 4GB）
}

// 建目录（mkdir -p）
static int mkdirs(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
#if defined(_WIN32)
            _mkdir(path);
#else
            mkdir(path, 0755);
#endif
            *p = '/';
        }
    }
    return 0;
}

// 失败原因：真机上只有一个「-2」时，分不清空间不够还是卷不完整（用户第一次展开就遇到这个）
static char g_err[512];
const char *smoarc_error(void) { return g_err[0] ? g_err : "（没有错误）"; }
static int fail(int code, const char *msg, const char *detail) {
    // 精度限死 380：详情可能是几千字节的路径，不限 gcc 会报 -Wformat-truncation（当错误）
    if (detail) snprintf(g_err, sizeof g_err, "%s：%.380s", msg, detail);
    else        snprintf(g_err, sizeof g_err, "%s", msg);
    return code;
}

int smoarc_extract_cb(const char *dir, const char *const *volumes, unsigned n,
                      smoarc_progress_fn cb, void *user) {
    char name[4096], full[8192];
    g_err[0] = 0;
    unsigned long long total = 0;
    unsigned done = 0;
    for (unsigned v = 0; v < n; v++) {
        FILE *f = fopen(volumes[v], "rb");
        if (!f) return fail(-2, "卷打不开（权限/路径？）", volumes[v]);
        char magic[5] = {0};
        if (fread(magic, 1, 4, f) != 4 || strcmp(magic, "SMOA")) {
            fclose(f); return fail(-1, "卷头不是 SMOA（文件被截断或拷错了？）", volumes[v]);
        }
        int vol = fgetc(f), vols = fgetc(f);
        if (vol < 0 || vols < 0) { fclose(f); return fail(-1, "卷头损坏", volumes[v]); }
        for (;;) {
            long pos = ftell(f);
            char tag[4] = {0};
            size_t got = fread(tag, 1, 4, f);
            if (got == 0) break;                     // 卷尾
            if (got != 4) { fclose(f); return fail(-1, "读卷中断（文件被截断？）", volumes[v]); }
            if (!memcmp(tag, "END!", 4)) {
                // 卷尾标记：可选的长度校验值（u64 已写字节数，忽略即可）
                (void)r64(f);
                break;
            }
            fseek(f, pos, SEEK_SET);                 // 不是 END!，回退按条目解析
            int nl = r16(f), sz = r32(f), crc = r32(f);
            if (nl <= 0 || sz < 0 || nl >= (int)sizeof name - 1) {
                fclose(f); return fail(-1, "条目字段越界（卷损坏）", volumes[v]);
            }
            if (fread(name, 1, nl, f) != (size_t)nl) {
                fclose(f); return fail(-1, "读条目名中断（卷被截断？）", volumes[v]);
            }
            name[nl] = 0;
            if (snprintf(full, sizeof full, "%s/%s", dir, name) >= (int)sizeof full) {
                fclose(f); return fail(-1, "路径太长", name);
            }
            mkdirs(full);
            FILE *o = fopen(full, "wb");
            if (!o) { fclose(f); return fail(-2, "输出文件打不开（空间不够？目录不可写？）", full); }
            unsigned long left = (unsigned)sz;
            uint32_t c = 0xFFFFFFFFu;
            uint8_t head[16] = {0};                      // 前 16 字节：CRC 不符时用来判断「读到的是什么」
            size_t head_n = 0;
            static uint8_t buf[1 << 16];
            while (left) {
                size_t want = left < sizeof buf ? left : sizeof buf;
                size_t r = fread(buf, 1, want, f);
                if (r == 0) { fclose(o); fclose(f); return fail(-1, "读卷没读到数据（卷被截断？）", name); }
                for (size_t i = 0; i < r; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
                if (head_n < sizeof head) {
                    size_t k = sizeof head - head_n < r ? sizeof head - head_n : r;
                    memcpy(head + head_n, buf, k);
                    head_n += k;
                }
                if (fwrite(buf, 1, r, o) != r) {          // 不检查的话，磁盘写满会静默出半截文件
                    fclose(o); fclose(f);
                    return fail(-2, "写文件失败（空间不够？）", full);
                }
                left -= (unsigned)r;
            }
            if (ferror(o) || fflush(o) != 0) {
                fclose(o); fclose(f);
                return fail(-2, "写盘没落稳（空间不够？）", full);
            }
            fclose(o);
            if (crc != (int)~c) {
                fclose(f);
                // 把「实际读到的前 16 字节」写进原因：全是 0 = 读到空洞/占位符；别的值 = 内容不同版本；
                // 再附两个 CRC 值，一眼能看出是「读的东西不对」还是「校验算法不对」。
                char detail[512];
                int o2 = snprintf(detail, sizeof detail, "%s（读到的前 16 字节:", name);
                for (size_t i = 0; i < 16 && o2 < (int)sizeof detail - 4; i++)
                    o2 += snprintf(detail + o2, sizeof detail - (size_t)o2, " %02x", head[i]);
                snprintf(detail + (o2 < (int)sizeof detail - 40 ? o2 : (int)sizeof detail - 40),
                         sizeof detail, "；实算 %08x / 表里 %08x）", (unsigned)(~c), (unsigned)crc);
                return fail(-2, "条目 CRC 不符", detail);
            }
            total += (unsigned)sz;
            if (cb) cb(user, ++done, name);
        }
        fclose(f);
    }
    fprintf(stderr, "[smoarc] 展开完成：%llu 字节\n", total);
    return 0;
}

int smoarc_extract(const char *dir, const char *const *volumes, unsigned n) {
    return smoarc_extract_cb(dir, volumes, n, NULL, NULL);
}

// ---- 卷组规划（见 smoarc.h 的说明）----
static int is_dir_p(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file_p(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

// 收集 base 下的 RomFS.smoa.0/.1/…（不连续就停），返回卷数
static unsigned collect_vols(const char *base, char (*vols)[1024], unsigned max) {
    unsigned n = 0;
    for (unsigned i = 0; i < max; i++) {
        char p[1024];
        if (snprintf(p, sizeof p, "%s/RomFS.smoa.%u", base, i) >= (int)sizeof p) break;
        if (!is_file_p(p)) break;
        snprintf(vols[n], 1024, "%s", p);
        n++;
    }
    return n;
}

unsigned smoarc_plan(const char *docs_dir, char (*volumes)[1024], unsigned max,
                     char *target, size_t tcap, char *base, size_t bcap) {
    unsigned n = collect_vols(docs_dir, volumes, max);
    if (n) {
        snprintf(target, tcap, "%s/RomFS", docs_dir);
        snprintf(base, bcap, "%s", docs_dir);
        return n;
    }
    DIR *d = opendir(docs_dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char sub[1024];
        if (snprintf(sub, sizeof sub, "%s/%s", docs_dir, e->d_name) >= (int)sizeof sub) continue;
        if (!is_dir_p(sub)) continue;
        // ② 包装目录下直接放卷
        n = collect_vols(sub, volumes, max);
        if (n) {
            snprintf(target, tcap, "%s/RomFS", sub);
            snprintf(base, bcap, "%s", sub);
            closedir(d);
            return n;
        }
        // ③ 包装目录/RomFS 里放卷（我们打的包就是这个形状）
        char sub2[1024];
        if (snprintf(sub2, sizeof sub2, "%s/RomFS", sub) >= (int)sizeof sub2) continue;
        if (!is_dir_p(sub2)) continue;
        n = collect_vols(sub2, volumes, max);
        if (n) {
            snprintf(target, tcap, "%s", sub2);
            snprintf(base, bcap, "%s", sub2);
            closedir(d);
            return n;
        }
    }
    closedir(d);
    return 0;
}
