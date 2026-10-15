// 整条纹理链路：块线性贴图 → 行优先解码像素。
// 组合 nvn_tex.c 的解交错（块线性 → 行优先块）与 nvn_bcdec.c 的解码（块行 → 像素）：
//   · BC 格式：每 mip 先解交错成「行优先块」，再按块行喂给 nvn_bc_decode
//   · 未压缩格式：块 = 1×1 texel，解交错后就是行优先像素，直接落位
// 这条链路在 Metal 上传之前全在 CPU 侧完成；Metal 层拿到的就是它要上传的行优先数据。
#include "nvn_tex.h"
#include "nvn_bcdec.h"
#include <stdlib.h>
#include <string.h>

// 每块的字节数（与 nvn_bcdec.c 同一口径：BC1/BC4 = 8，其余 BC = 16；未压缩用布局里的值）
static uint32_t block_bytes(uint32_t fmt, int is_bc, uint32_t layout_bpb) {
    if (!is_bc) return layout_bpb;
    switch (fmt) {
    case 0x41: case 0x42: case 0x45: case 0x46:    // BC1 / BC1 sRGB
    case 0x49: case 0x4a:                          // BC4 / BC4 有符号
        return 8;
    default:
        return 16;                                 // BC2/3/5/6H/7
    }
}

// 解码整张贴图（所有层、所有 mip）。dst 按 层 → mip → 行 紧密排列。
// 返回写出总字节数；-1 = 失败（dst 不够 / src 不够 / 布局参数不合法，不会写半张）。
int64_t nvn_tex_decode(const nvn_tex_layout *L, uint32_t nvn_fmt,
                       const uint8_t *src, size_t src_size,
                       uint8_t *dst, size_t dst_cap) {
    uint32_t kind;
    int bpp;
    int is_bc = nvn_bc_unpacked(nvn_fmt, &kind, &bpp);
    uint32_t bb = block_bytes(nvn_fmt, is_bc, L->bytes_per_block);
    uint64_t layer_sz = nvn_tex_level_offset(L, L->mip_levels);

    int64_t total = 0;
    for (uint32_t layer = 0; layer < L->layers; layer++) {
        for (unsigned level = 0; level < L->mip_levels; level++) {
            uint32_t w_px = tex_mip_size(L->width, level);
            uint32_t h_px = tex_mip_size(L->height, level);
            uint32_t w_blk = tex_div_ceil(w_px, L->block_w);
            uint32_t h_blk = tex_div_ceil(h_px, L->block_h);
            uint64_t off = nvn_tex_level_offset(L, level) + (uint64_t)layer * layer_sz;
            if (off > src_size) return -1;
            const uint8_t *msrc = src + off;
            size_t mip_src = (size_t)(src_size - off);

            if (is_bc) {
                size_t row = (size_t)w_blk * bb;
                size_t need = (size_t)h_blk * row;
                if ((uint64_t)total + (uint64_t)h_px * w_px * bpp > dst_cap) return -1;
                uint8_t *blockbuf = malloc(need ? need : 1);
                if (!blockbuf) return -1;
                if (L->linear) {
                    size_t stride = L->stride ? L->stride : row;
                    for (uint32_t y = 0; y < h_blk; y++) {
                        if ((size_t)y * stride + row > mip_src) { free(blockbuf); return -1; }
                        memcpy(blockbuf + y * row, msrc + (size_t)y * stride, row);
                    }
                } else {
                    size_t got = nvn_tex_detile(blockbuf, need, 0, msrc, mip_src,
                                                w_blk, h_blk, bb, L->tile_h_log2);
                    if (got != need) { free(blockbuf); return -1; }
                }
                // dpitch = 每像素行的字节数（**不是 0**：0 会让 nvn_bc_decode 把所有行写到同一处）
                int rc = nvn_bc_decode(nvn_fmt, blockbuf, row, w_px, h_px, dst + total,
                                       (size_t)w_px * bpp);
                free(blockbuf);
                if (rc != 0) return -1;
                total += (int64_t)h_px * w_px * bpp;
            } else {
                // 非 BC 格式必须块 = 1×1：不然「解交错后就是像素」不成立（4×4 块但没有解码器）
                if (L->block_w != 1 || L->block_h != 1) return -1;
                uint32_t px = L->bytes_per_block;               // 块 = 1×1 texel
                size_t need = (size_t)w_px * h_px * px;
                if ((uint64_t)total + need > dst_cap) return -1;
                if (L->linear) {
                    size_t stride = L->stride ? L->stride : (size_t)w_px * px;
                    for (uint32_t y = 0; y < h_px; y++) {
                        if ((size_t)y * stride + w_px * px > mip_src) return -1;
                        memcpy(dst + total + (size_t)y * w_px * px, msrc + (size_t)y * stride, w_px * px);
                    }
                } else {
                    size_t got = nvn_tex_detile(dst + total, dst_cap - (size_t)total, 0, msrc, mip_src,
                                                w_px, h_px, px, L->tile_h_log2);
                    if (got != need) return -1;
                }
                total += (int64_t)need;
            }
        }
    }
    return total;
}
