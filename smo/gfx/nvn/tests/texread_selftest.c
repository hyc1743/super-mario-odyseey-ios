// 纹理整链单测（块线性 BC 贴图 → 行优先像素；不需要游戏文件、不需要 Metal）：
// 手工构造一张 8×8 BC1、tile_h=0、两个 mip 的贴图 —— 块的摆位用 nvn_tex_block_offset 算
// （自洽），再断言每个 4×4 块区的颜色与手工摆放一致、mip1 是 4×4 红块。
// 期望值手算可得：块线性 tile_h=0 时 GOB 内部排布退化为「1 行 16 块、块序 = x 顺序」。
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../nvn_tex.h"
#include "../nvn_texread.h"

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

static void bc1_block(uint8_t *b, int red) {      // 红块或蓝块（全取端点 0）
    b[0] = red ? 0x00 : 0x1F; b[1] = red ? 0xF8 : 0x00; b[2] = red ? 0x1F : 0x00;
    b[3] = 0x00; b[4] = b[5] = b[6] = b[7] = 0x00;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);   // 排障：崩溃时缓冲会吞掉前面的输出
    printf("== 0. 构造 8×8 BC1、两个 mip、tile_h=0 的块线性贴图 ==\n");
    nvn_tex_layout L;
    memset(&L, 0, sizeof L);
    L.width = 8; L.height = 8; L.depth = 1; L.layers = 1; L.mip_levels = 2;
    L.bytes_per_block = 8; L.block_w = 4; L.block_h = 4;
    L.samples_x = L.samples_y = 1; L.dims = 2; L.tile_h_log2 = 0; L.tile_d_log2 = 0;

    uint64_t mip0_off = nvn_tex_level_offset(&L, 0);
    uint64_t mip1_off = nvn_tex_level_offset(&L, 1);
    uint64_t total_sz = nvn_tex_storage_size(&L);
    printf("      mip0 @ 0x%llx，mip1 @ 0x%llx，总 0x%llx\n",
           (unsigned long long)mip0_off, (unsigned long long)mip1_off, (unsigned long long)total_sz);
    CHK(mip0_off == 0, "mip0 从 0 开始");

    uint8_t *src = calloc((size_t)total_sz, 1);
    CHK(src != NULL, "源缓冲分配");
    if (!src) return 1;
    // mip0：8×8 = 2×2 块，左上红、其余蓝；mip1：4×4 = 1 块红
    uint8_t blk[8];
    uint64_t o = nvn_tex_block_offset(0, 0, 2, 8, L.tile_h_log2);
    bc1_block(blk, 1); memcpy(src + o, blk, 8);
    bc1_block(blk, 0);
    for (int by = 0; by < 2; by++) for (int bx = 0; bx < 2; bx++) {
        if (bx == 0 && by == 0) continue;
        uint64_t p = nvn_tex_block_offset(bx, by, 2, 8, L.tile_h_log2);
        memcpy(src + p, blk, 8);
    }
    bc1_block(blk, 1);
    memcpy(src + mip1_off + nvn_tex_block_offset(0, 0, 1, 8, L.tile_h_log2), blk, 8);
    CHK(nvn_tex_block_offset(1, 0, 2, 8, 0) == 8, "tile_h=0 时块序就是 x 顺序（自洽前提）");

    printf("== 1. 整链解码 ==\n");
    uint8_t *dst = malloc(8 * 8 * 4 + 4 * 4 * 4 + 64);
    int64_t n = nvn_tex_decode(&L, 0x41, src, (size_t)total_sz, dst, 8 * 8 * 4 + 4 * 4 * 4);
    CHK(n == 8 * 8 * 4 + 4 * 4 * 4, "写出的字节数 = mip0 256 + mip1 64");
    if (n < 0) { printf("  提前结束（解码失败）\n"); return 1; }

    printf("== 2. mip0：左上 4×4 红、其余蓝 ==\n");
    int ok = 1;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        const uint8_t *p = dst + y * 32 + x * 4;
        int want_r = (x < 4 && y < 4);
        if (p[0] != (want_r ? 255 : 0) || p[2] != (want_r ? 0 : 255) || p[3] != 255) ok = 0;
    }
    CHK(ok, "8×8 每像素与摆放一致（红区 vs 蓝区）");

    printf("== 3. mip1：整块 4×4 红 ==\n");
    ok = 1;
    const uint8_t *m1 = dst + 8 * 8 * 4;
    for (int i = 0; i < 4 * 4; i++)
        if (!(m1[i * 4] == 255 && m1[i * 4 + 2] == 0)) ok = 0;
    CHK(ok, "mip1 的 16 个像素都是红");

    printf("== 4. ASTC_4x4：只解交错不解码（苹果 GPU 原生支持，Metal 层直接上传）==");
    putchar('\n');
    nvn_tex_layout LA;
    memset(&LA, 0, sizeof LA);
    LA.width = 8; LA.height = 8; LA.depth = 1; LA.layers = 1; LA.mip_levels = 1;
    LA.bytes_per_block = 16; LA.block_w = 4; LA.block_h = 4;
    LA.samples_x = LA.samples_y = 1; LA.dims = 2; LA.tile_h_log2 = 0; LA.tile_d_log2 = 0;
    uint8_t *sa = calloc((size_t)nvn_tex_storage_size(&LA), 1);
    CHK(sa != NULL, "ASTC 源缓冲");
    uint8_t astcblk[16];
    for (unsigned i = 0; i < 16; i++) astcblk[i] = (uint8_t)(0x50 + i);
    for (int by = 0; by < 2; by++) for (int bx = 0; bx < 2; bx++)
        memcpy(sa + nvn_tex_block_offset(bx, by, 2, 16, 0), astcblk, 16);
    uint8_t da[2 * 2 * 16 + 16];
    memset(da, 0xAA, sizeof da);
    int64_t na = nvn_tex_decode(&LA, 0x00, sa, (size_t)nvn_tex_storage_size(&LA), da, 64);
    CHK(na == 64, "ASTC 8×8 输出 = 2×2 块 × 16 字节 = 64");
    CHK(da[0] == 0x50 && da[16] == 0x50 && da[32] == 0x50 && da[48] == 0x50, "四块按行序排列（每块首字节 0x50）");
    CHK(da[64] == 0xAA, "没写出 64 字节之外");

    printf("== 5. RGBA8 未压缩块线性（块 = 1×1 texel）==");
    putchar('\n');
    nvn_tex_layout LU;
    memset(&LU, 0, sizeof LU);
    LU.width = 8; LU.height = 8; LU.depth = 1; LU.layers = 1; LU.mip_levels = 1;
    LU.bytes_per_block = 4; LU.block_w = 1; LU.block_h = 1;
    LU.samples_x = LU.samples_y = 1; LU.dims = 2; LU.tile_h_log2 = 0; LU.tile_d_log2 = 0;
    uint8_t *su = calloc((size_t)nvn_tex_storage_size(&LU), 1);
    CHK(su != NULL, "RGBA8 源缓冲");
    for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++) {
        uint64_t o = nvn_tex_block_offset(x, y, 8, 4, 0);
        uint32_t v = 0x01000000u * (y * 8 + x);
        memcpy(su + o, &v, 4);
    }
    uint8_t du[8 * 8 * 4 + 8];
    memset(du, 0xAA, sizeof du);
    int64_t nu = nvn_tex_decode(&LU, 0x00, su, (size_t)nvn_tex_storage_size(&LU), du, 256);
    CHK(nu == 256, "RGBA8 输出 256 字节");
    int okU = 1;
    for (unsigned i = 0; i < 64; i++) {
        uint32_t v;
        memcpy(&v, du + i * 4, 4);
        if (v != 0x01000000u * i) okU = 0;
    }
    CHK(okU, "解交错后每个 texel 的值与摆放一致（第 i 个 = i）");
    free(sa); free(su);

    printf("== 6. 失败路径 ==");
    putchar('\n');
    CHK(nvn_tex_decode(&L, 0x41, src, 4, dst, 99999) < 0, "src 太小 → 报错");
    // 新语义：块>1×1 的非 BC 格式 = 只解交错不解码（ASTC 路径），不再是错误
    CHK(nvn_tex_decode(&L, 0x00, src, (size_t)total_sz, dst, 8 * 8 * 4 + 4 * 4 * 4) == 40,
        "BC 布局 + 非 BC 格式号 → 只解交错（mip0 32 字节 + mip1 8 字节 = 40）");

    free(src); free(dst);
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
