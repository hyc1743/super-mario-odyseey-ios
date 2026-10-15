// BC 压缩贴图的 CPU 解码（bcdec 的薄封装）。
//
// 为什么必需：苹果 GPU 不支持 BC 系列压缩格式（它用 ASTC）。实测游戏 18290 张贴图里
// 96.6% 是 BC（smo/gfx/tools/bntx_survey.py），所以不解码就没有贴图可用。
// 用法与参考项目 dm/gfx/tex/nvn_bcdec.h 一致（MIT 派生）。
#pragma once
#include <stddef.h>
#include <stdint.h>

// 解压后的像素布局（Metal 像素格式的映射在 Metal 层再做，这里只管字节布局）
enum {
    NVN_BC_R8Unorm = 1,
    NVN_BC_R8Snorm,
    NVN_BC_RG8Unorm,
    NVN_BC_RG8Snorm,
    NVN_BC_RGBA8Unorm,
    NVN_BC_RGBA8Unorm_sRGB,
    NVN_BC_RGBA16Float,
};

// NVN 格式是 BC → 返回 1，*kind = 解压后的像素布局、*bpp = 解压后每像素字节；不是 BC 返回 0
int nvn_bc_unpacked(uint32_t nvn_fmt, uint32_t *kind, int *bpp);

// 解一片 w×h 像素：src 按块行排（每块行 pitch 字节），dst 每像素行 dpitch 字节。成功 0。
// 宽高不一定是 4 的倍数：边上不满的块先解到临时块再拷需要的部分。
int nvn_bc_decode(uint32_t nvn_fmt, const uint8_t *src, size_t pitch,
                  uint32_t w, uint32_t h, uint8_t *dst, size_t dpitch);
