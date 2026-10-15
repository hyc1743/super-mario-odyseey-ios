// 纹理整链（块线性 → 行优先像素）的接口。实现见 nvn_texread.c。
#ifndef SMO_NVN_TEXREAD_H
#define SMO_NVN_TEXREAD_H

#include "nvn_tex.h"
#include <stdint.h>

// 解码整张贴图（所有层、所有 mip）。dst 按 层 → mip → 行 紧密排列。
// 返回写出总字节数；-1 = 失败（dst 不够 / src 不够 / 格式不认识，不会写半张）。
int64_t nvn_tex_decode(const nvn_tex_layout *L, uint32_t nvn_fmt,
                       const uint8_t *src, size_t src_size,
                       uint8_t *dst, size_t dst_cap);

#endif
