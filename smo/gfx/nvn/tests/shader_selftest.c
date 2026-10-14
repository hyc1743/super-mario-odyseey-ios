// 着色器索引单测。
//   1. 合成自检：自己造两个 blob + 一份索引 → 验证精确查找、未命中、控制段两种布局（归档槽 / 滑动扫描）
//   2. 真实数据（本机，可选）：`shader_selftest <索引文件> <blob…>` —— 每个 blob 都必须命中，
//      这是「运行时算的键 == 离线算的键」的直接验证（CI 里跑合成那部分即可）。
#include "../nvn_host.h"
#include "../nvn_shader.h"

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

static uint8_t g_blobA[0x100];
static uint8_t g_blobB[0x100];

static void fill(uint8_t *p, unsigned n, unsigned seed) {
    for (unsigned i = 0; i < n; i++) p[i] = (uint8_t)(i * 31 + seed);
}

int main(int argc, char **argv) {
    printf("== 1. 合成：自己造的 blob 与索引 ==\n");
    fill(g_blobA, sizeof g_blobA, 7);
    fill(g_blobB, sizeof g_blobB, 99);
    uint64_t kA = nvn_shader_key(g_blobA, sizeof g_blobA);
    uint64_t kB = nvn_shader_key(g_blobB, sizeof g_blobB);
    CHK(kA != kB, "两个 blob 的键不同");
    CHK(kA != nvn_shader_key(g_blobA, sizeof g_blobA - 8), "少 8 字节就变键（长度参与）");

    char idx[] = "/tmp/smo_shader_selftest.idx";
    FILE *f = fopen(idx, "wb");
    if (!f) { printf("  【红】 写不出 %s\n", idx); return 1; }
    fprintf(f, "# 合成索引\n%016llx\t1\tsynthA.metal\n%016llx\t5\tsynthB.metal\n",
            (unsigned long long)kA, (unsigned long long)kB);
    fclose(f);
    CHK(nvn_shader_load(idx) == 2, "载入 2 条");
    CHK(nvn_shader_count() == 2, "表内 2 条");
    const nvn_shader_ent *e = nvn_shader_find(g_blobA, sizeof g_blobA);
    CHK(e && e->stage == 1 && !strcmp(e->msl, "synthA.metal"), "精确查到 A（stage=1, synthA.metal）");
    e = nvn_shader_find(g_blobB, sizeof g_blobB);
    CHK(e && e->stage == 5 && !strcmp(e->msl, "synthB.metal"), "精确查到 B（stage=5, synthB.metal）");
    CHK(nvn_shader_find(g_blobA, sizeof g_blobA - 8) == NULL, "长度不对 → 查不到");
    CHK(nvn_shader_find(NULL, 0) == NULL, "空指针 → 查不到");

    printf("== 2. 控制段：游戏归档槽布局（marker@+0 / SPH@+0x30 / 码@+0x80）==\n");
    // 注意：归档槽里**码一直延伸到槽尾**，所以容器长度必须正好是 0x30 + blob，多留填充就会算错键
    static uint8_t ctrl[0x30 + sizeof g_blobA];
    for (unsigned i = 0; i < sizeof ctrl; i++) ctrl[i] = 0xAA;
    *(uint32_t *)ctrl = 0x12345678u;
    memcpy(ctrl + 0x30, g_blobA, sizeof g_blobA);
    const nvn_shader_ent *c = nvn_shader_find_control(ctrl, sizeof ctrl);
    CHK(c && !strcmp(c->msl, "synthA.metal"), "按归档槽布局命中");

    printf("== 3. 控制段：没有 marker 时靠滑动扫描 ==\n");
    static uint8_t ctrl2[0x38 + sizeof g_blobB];
    for (unsigned i = 0; i < sizeof ctrl2; i++) ctrl2[i] = 0x55;
    memcpy(ctrl2 + 0x38, g_blobB, sizeof g_blobB);
    c = nvn_shader_find_control(ctrl2, sizeof ctrl2);
    CHK(c && !strcmp(c->msl, "synthB.metal"), "无 marker 时滑动扫描命中（偏移 0x38）");
    CHK(nvn_shader_find_control(ctrl2, 0x20) == NULL, "太短的控制段 → 查不到");

    printf("== 4. 真实数据（可选，本机）==\n");
    if (argc >= 3) {
        nvn_shader_reset();
        CHK(nvn_shader_load(argv[1]) > 0, "载入真实索引");
        printf("     表内 %u 条\n", nvn_shader_count());
        unsigned hit = 0, miss = 0;
        for (int i = 2; i < argc; i++) {
            FILE *g = fopen(argv[i], "rb");
            if (!g) { miss++; continue; }
            static uint8_t buf[1 << 20];
            size_t n = fread(buf, 1, sizeof buf, g);
            fclose(g);
            if (nvn_shader_find(buf, n)) hit++; else { miss++; if (miss <= 3) printf("     未命中：%s\n", argv[i]); }
        }
        printf("     真实 blob：命中 %u，未命中 %u\n", hit, miss);
        CHK(hit > 0 && miss == 0, "运行时算的键与离线一致（每个真实 blob 都命中）");
    } else {
        printf("  （未给真实 blob，跳过；用法：shader_selftest <索引> <blob…>）\n");
    }

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
