// BC 压缩贴图的 CPU 解码封装。见 nvn_bcdec.h。
// 派生自参考项目 d2r-mobile 的 dm/gfx/tex/nvn_bcdec.c（MIT）；
// 解码本体是 bcdec（smo/third_party/bcdec，单头文件，MIT / Unlicense 双许可，LICENSE 与 README 随附）。
//
// NVN 的 BC 格式值（与参考项目实测一致，也与我们 enums.tsv 里的枚举一致）：
//   0x41=RGB_DXT1  0x42=RGBA_DXT1  0x43=RGBA_DXT3  0x44=RGBA_DXT5
//   0x45..0x48 = 同上 sRGB
//   0x49=RGTC1_UNORM(BC4)  0x4a=RGTC1_SNORM  0x4b=RGTC2_UNORM(BC5)  0x4c=RGTC2_SNORM
//   0x4d=BPTC_UNORM(BC7)   0x4e=BPTC_UNORM_SRGB  0x4f=BPTC_SFLOAT(BC6H 有符号)  0x50=BPTC_UFLOAT
#define BCDEC_IMPLEMENTATION
#define BCDEC_BC4BC5_PRECISE   // BC4/BC5 带符号版本的接口（与参考项目同）
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"   // bcdec.h 里所有格式的解码函数都在一个 TU 里，
                                                     // 用不到的静态函数会告警；忽略（只关这一个 TU 的这条）
#include "../../third_party/bcdec/bcdec.h"
#pragma GCC diagnostic pop
#include "nvn_bcdec.h"
#include <string.h>

int nvn_bc_unpacked(uint32_t fmt, uint32_t *kind, int *bpp) {
    switch (fmt) {
    case 0x41: case 0x42: case 0x43: case 0x44: case 0x4d: *kind = NVN_BC_RGBA8Unorm;       *bpp = 4; return 1;
    case 0x45: case 0x46: case 0x47: case 0x48: case 0x4e: *kind = NVN_BC_RGBA8Unorm_sRGB;  *bpp = 4; return 1;
    case 0x49: *kind = NVN_BC_R8Unorm;  *bpp = 1; return 1;
    case 0x4a: *kind = NVN_BC_R8Snorm;  *bpp = 1; return 1;
    case 0x4b: *kind = NVN_BC_RG8Unorm; *bpp = 2; return 1;
    case 0x4c: *kind = NVN_BC_RG8Snorm; *bpp = 2; return 1;
    case 0x4f: case 0x50: *kind = NVN_BC_RGBA16Float; *bpp = 8; return 1;
    default: return 0;
    }
}

// 一个 4×4 块 → 像素（o 每行 op 字节）。全部格式的解码入口都在这里，跟参考项目一一对应。
static void block(uint32_t f, const uint8_t *b, uint8_t *o, int op) {
    switch (f) {
    case 0x41: case 0x42: case 0x45: case 0x46: bcdec_bc1(b, o, op); break;
    case 0x43: case 0x47: bcdec_bc2(b, o, op); break;
    case 0x44: case 0x48: bcdec_bc3(b, o, op); break;
    case 0x49: bcdec_bc4(b, o, op, 0); break;
    case 0x4a: bcdec_bc4(b, o, op, 1); break;
    case 0x4b: bcdec_bc5(b, o, op, 0); break;
    case 0x4c: bcdec_bc5(b, o, op, 1); break;
    case 0x4d: case 0x4e: bcdec_bc7(b, o, op); break;
    case 0x4f: case 0x50: {
        // bcdec 出 RGB 半精度，补 alpha = 1.0（与参考项目同）
        uint16_t rgb[16 * 3];
        bcdec_bc6h_half(b, rgb, 4 * 3, f == 0x4f);
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
            uint16_t *d = (uint16_t *)(o + y * op) + x * 4;
            const uint16_t *s = rgb + (y * 4 + x) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 0x3c00;
        }
        break;
    }
    }
}

int nvn_bc_decode(uint32_t fmt, const uint8_t *src, size_t pitch,
                  uint32_t w, uint32_t h, uint8_t *dst, size_t dpitch) {
    uint32_t kind;
    int bpp;
    if (!nvn_bc_unpacked(fmt, &kind, &bpp)) return -1;
    // 每块字节：BC1/BC4 是 8，其余 16（与参考项目同）
    int bb = (fmt == 0x41 || fmt == 0x42 || fmt == 0x45 || fmt == 0x46 || fmt == 0x49 || fmt == 0x4a) ? 8 : 16;
    uint8_t tmp[4 * 4 * 8];
    for (uint32_t by = 0; by * 4 < h; by++)
        for (uint32_t bx = 0; bx * 4 < w; bx++) {
            const uint8_t *b = src + by * pitch + bx * bb;
            uint32_t cw = w - bx * 4 < 4 ? w - bx * 4 : 4;
            uint32_t ch = h - by * 4 < 4 ? h - by * 4 : 4;
            uint8_t *o = dst + (size_t)by * 4 * dpitch + (size_t)bx * 4 * bpp;
            if (cw == 4 && ch == 4) { block(fmt, b, o, (int)dpitch); continue; }
            block(fmt, b, tmp, 4 * bpp);   // 边上不满 4×4 的块：先解到临时块，再拷需要的部分
            for (uint32_t y = 0; y < ch; y++) memcpy(o + y * dpitch, tmp + y * 4 * bpp, cw * bpp);
        }
    return 0;
}
