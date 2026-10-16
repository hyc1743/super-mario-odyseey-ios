// 资源容器的展开实现。见 smoarc.h。
#include "smoarc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>     // _mkdir
#else
#include <sys/stat.h>   // mkdir
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

int smoarc_extract(const char *dir, const char *const *volumes, unsigned n) {
    char name[4096], full[8192];
    unsigned long long total = 0;
    for (unsigned v = 0; v < n; v++) {
        FILE *f = fopen(volumes[v], "rb");
        if (!f) return -2;
        char magic[5] = {0};
        if (fread(magic, 1, 4, f) != 4 || strcmp(magic, "SMOA")) { fclose(f); return -1; }
        int vol = fgetc(f), vols = fgetc(f);
        if (vol < 0 || vols < 0) { fclose(f); return -1; }
        for (;;) {
            long pos = ftell(f);
            char tag[4] = {0};
            size_t got = fread(tag, 1, 4, f);
            if (got == 0) break;                     // 卷尾
            if (got != 4) { fclose(f); return -1; }
            if (!memcmp(tag, "END!", 4)) {
                // 卷尾标记：可选的长度校验值（u64 已写字节数，忽略即可）
                (void)r64(f);
                break;
            }
            fseek(f, pos, SEEK_SET);                 // 不是 END!，回退按条目解析
            int nl = r16(f), sz = r32(f), crc = r32(f);
            if (nl <= 0 || sz < 0 || nl >= (int)sizeof name - 1) { fclose(f); return -1; }
            if (fread(name, 1, nl, f) != (size_t)nl) { fclose(f); return -1; }
            name[nl] = 0;
            if (snprintf(full, sizeof full, "%s/%s", dir, name) >= (int)sizeof full) { fclose(f); return -1; }
            mkdirs(full);
            FILE *o = fopen(full, "wb");
            if (!o) { fclose(f); return -2; }
            unsigned long left = (unsigned)sz;
            uint32_t c = 0xFFFFFFFFu;
            static uint8_t buf[1 << 16];
            while (left) {
                size_t want = left < sizeof buf ? left : sizeof buf;
                size_t r = fread(buf, 1, want, f);
                if (r == 0) { fclose(o); fclose(f); return -1; }
                for (size_t i = 0; i < r; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
                fwrite(buf, 1, r, o);
                left -= (unsigned)r;
            }
            fclose(o);
            if (crc != (int)~c) { fclose(f); return -2; }   // 校验不匹配：多半是卷不完整
            total += (unsigned)sz;
        }
        fclose(f);
    }
    fprintf(stderr, "[smoarc] 展开完成：%llu 字节\n", total);
    return 0;
}
