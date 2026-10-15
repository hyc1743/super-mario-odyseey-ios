// BC 解码单测（不需要游戏文件、不需要 Metal）：
// 手工构造已知图样的 BC1/BC4/BC5 块，断言解出来的每个像素。
// 为什么图样可以手算：BC1 端点是 RGB565 直给、BC4/BC5 端点是 8 位直给，
// 全 0 索引 = 全部取端点 0 —— 于是期望值不依赖解码器实现，谁解码都得是这个数。
// （BC2/3/6H/7 的插值细节复杂，交给阶段 A3 用真实贴图对拍 BNTX-Extractor 验证。）
#include <stdio.h>
#include <string.h>

#include "../nvn_bcdec.h"

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

int main(void) {
    printf("== 1. BC1（RGB_DXT1，0x41）：红端点半块 ==\n");
    static const uint8_t bc1[] = { 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
    // color0 = RGB565 0xF800 = 红；color1 = 0x001F = 蓝；索引全 0 → 全部取 color0
    static uint8_t out[4 * 4 * 4];
    CHK(nvn_bc_decode(0x41, bc1, 8, 4, 4, out, 16) == 0, "BC1 4×4 解码成功");
    int ok = 1;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        const uint8_t *p = out + y * 16 + x * 4;
        if (!(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255)) ok = 0;
    }
    CHK(ok, "全部 16 个像素都是 (255,0,0,255) —— 端点直给，谁解码都得是这个数");
    uint32_t kind; int bpp;
    CHK(nvn_bc_unpacked(0x41, &kind, &bpp) && kind == NVN_BC_RGBA8Unorm && bpp == 4, "解压布局 RGBA8 / 4 字节");

    printf("== 2. BC4（RGTC1_UNORM，0x49）：灰度 255 半块 ==\n");
    static const uint8_t bc4[] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t out4[4 * 4];
    CHK(nvn_bc_decode(0x49, bc4, 8, 4, 4, out4, 4) == 0, "BC4 4×4 解码成功");
    ok = 1;
    for (int i = 0; i < 16; i++) if (out4[i] != 255) ok = 0;
    CHK(ok, "全部 16 个像素都是 255（每像素 1 字节）");
    CHK(nvn_bc_unpacked(0x49, &kind, &bpp) && kind == NVN_BC_R8Unorm && bpp == 1, "解压布局 R8 / 1 字节");

    printf("== 3. BC5（RGTC2_UNORM，0x4b）：R=255、G=128 半块 ==\n");
    static const uint8_t bc5[16] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static uint8_t out5[4 * 4 * 2];
    CHK(nvn_bc_decode(0x4b, bc5, 16, 4, 4, out5, 8) == 0, "BC5 4×4 解码成功");
    ok = 1;
    for (int i = 0; i < 16; i++) if (!(out5[i * 2] == 255 && out5[i * 2 + 1] == 128)) ok = 0;
    CHK(ok, "全部 16 个像素都是 (R=255, G=128)（每像素 2 字节）");

    printf("== 4. 边上不满 4×4 的块：2×2 区域 + 行距不受损 ==\n");
    static uint8_t dst[2 * 32 + 16];
    memset(dst, 0xAA, sizeof dst);
    CHK(nvn_bc_decode(0x41, bc1, 8, 2, 2, dst, 32) == 0, "BC1 2×2 解码成功");
    ok = 1;
    for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) {
        const uint8_t *p = dst + y * 32 + x * 4;
        if (!(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255)) ok = 0;
    }
    CHK(ok, "2×2 都是红");
    ok = 1;
    for (size_t i = 0; i < sizeof dst; i++)
        if (dst[i] == 0xAA) { /* 期望只在行尾填充区 */ }
    CHK(dst[1 * 32 + 2 * 4] == 0xAA && dst[1 * 32 + 31] == 0xAA, "行尾填充未被触碰（行距被尊重）");

    printf("== 5. 非 BC 格式 ==\n");
    CHK(nvn_bc_unpacked(0x00, &kind, &bpp) == 0, "0x00 不是 BC");
    CHK(nvn_bc_decode(0x00, bc1, 8, 4, 4, out, 16) != 0, "非 BC 解码返回错误");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
