// 着色器索引的实现（见 nvn_shader.h 的设计说明）。
#include "nvn_shader.h"
#include "nvn_host.h"

#define SHADER_MAX 65536          // 2 的幂：已译着色器 18470 个，留一倍余量
#define CTRL_SCAN_MAX (64u * 1024)   // 滑动扫描的尺寸上限

static nvn_shader_ent g_sh[SHADER_MAX];
static unsigned g_sh_n;
static unsigned g_sh_hits;
static unsigned g_sh_coll;        // 键冲突（不同 blob 撞同一个 64 位键）：出现就要报警

uint64_t nvn_shader_key(const uint8_t *p, size_t n) {
    uint64_t h = 1469598103934665603ULL;               // FNV-1a 64 偏移基
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;                          // FNV 质数
    }
    return h;
}

static unsigned slot_of(uint64_t key) { return (unsigned)(key * 0x9E3779B97F4A7C15ULL >> 40) & (SHADER_MAX - 1); }

static void put(uint64_t key, unsigned stage, const char *msl) {
    unsigned i = slot_of(key);
    for (unsigned k = 0; k < SHADER_MAX; k++) {
        nvn_shader_ent *e = &g_sh[(i + k) % SHADER_MAX];
        if (!e->key) {
            e->key = key;
            e->stage = (uint16_t)stage;
            unsigned j = 0;
            for (; msl[j] && j < NVN_SHADER_MSL_MAX - 1; j++) e->msl[j] = msl[j];
            e->msl[j] = 0;
            g_sh_n++;
            return;
        }
        if (e->key == key) return;                      // 同键重复：忽略
    }
}

static const nvn_shader_ent *find_key(uint64_t key) {
    if (!key) return NULL;
    unsigned i = slot_of(key);
    for (unsigned k = 0; k < SHADER_MAX; k++) {
        const nvn_shader_ent *e = &g_sh[(i + k) % SHADER_MAX];
        if (!e->key) return NULL;
        if (e->key == key) return e;
    }
    return NULL;
}

// 文本索引：`<fnv64 十六进制>\t<stage>\t<msl 文件名>`，`#` 开头是注释
int nvn_shader_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char *p = line;
        char *tab1 = strchr(p, '\t');
        if (!tab1) continue;
        *tab1 = 0;
        char *tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;
        *tab2 = 0;
        char *msl = tab2 + 1;
        size_t ml = strlen(msl);
        while (ml && (msl[ml - 1] == '\n' || msl[ml - 1] == '\r')) msl[--ml] = 0;
        uint64_t key = strtoull(p, NULL, 16);
        unsigned stage = (unsigned)strtoul(tab1 + 1, NULL, 10);
        if (!key) continue;
        put(key, stage, msl);
        n++;
    }
    fclose(f);
    NVN_LOG("着色器索引：载入 %d 行，表内 %u 条（%s）", n, g_sh_n, path);
    return n;
}

const nvn_shader_ent *nvn_shader_find(const uint8_t *p, size_t n) {
    if (!p || n < 0x58) return NULL;
    const nvn_shader_ent *e = find_key(nvn_shader_key(p, n));
    if (e) g_sh_hits++;
    return e;
}

// 控制段里找。两条路：
//   ① 游戏归档的槽布局：+0x00 marker 0x12345678、+0x30 SPH(0x50)、+0x80 机器码，整段到末尾
//   ② 8 字节对齐滑动窗口（控制段 ≤ 64 KiB 时）：从每个候选起点试「SPH+剩余」这一个长度
// ② 是有意的简化：因为每个 stage 的**代码长度**在控制段里由控制段的布局给出，而那个布局还没证据。
// 先把这两种试法都打日志，真机日志一到就能定下布局、把 ② 换成精确长度。
const nvn_shader_ent *nvn_shader_find_control(const uint8_t *ctrl, size_t n) {
    if (!ctrl || n < 0x58) return NULL;

    // ① 归档槽布局
    if (n >= 0x88 && *(const uint32_t *)ctrl == 0x12345678u) {
        const nvn_shader_ent *e = nvn_shader_find(ctrl + 0x30, n - 0x30);
        if (e) {
            static unsigned said;
            if (!said) { said = 1; NVN_LOG("控制段命中了归档槽布局（marker@+0、SPH@+0x30、码@+0x80）"); }
            return e;
        }
    }

    // ② 滑动扫描（只在整段里找「SPH+到末尾」这一种长度）
    if (n <= CTRL_SCAN_MAX) {
        for (size_t off = 0; off + 0x58 <= n; off += 8) {
            const nvn_shader_ent *e = nvn_shader_find(ctrl + off, n - off);
            if (e) {
                static unsigned said2;
                if (!said2) { said2 = 1; NVN_LOG("控制段命中在偏移 0x%zx（滑动扫描）", off); }
                return e;
            }
        }
    }
    return NULL;
}

unsigned nvn_shader_count(void) { return g_sh_n; }
unsigned nvn_shader_hits(void)  { return g_sh_hits; }
unsigned nvn_shader_collisions(void) { return g_sh_coll; }
void nvn_shader_reset(void) {
    for (unsigned i = 0; i < SHADER_MAX; i++) g_sh[i].key = 0;
    g_sh_n = 0;
    g_sh_hits = 0;
    g_sh_coll = 0;
}
