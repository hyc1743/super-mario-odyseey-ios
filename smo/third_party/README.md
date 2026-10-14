# third_party —— 第三方代码

## ryujinx/ —— Ryujinx 的着色器翻译库（MIT）

来源：<https://github.com/Ryujinx/Ryujinx>，MIT 许可（本目录的 `LICENSE.txt` 是上游原文）。

收录的是**着色器翻译链必需的那一部分**（不是整个模拟器）：`Ryujinx.Graphics.Shader`（Maxwell 机器码 →
SPIR-V 的翻译器）、`Ryujinx.Common`、`Spv.Generator` 三个项目及其 `.csproj`。参考项目
[jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile) 同样这么做，本仓沿用同一做法，
以便两边对照。

**为什么是它**：Nintendo Switch 的 GPU 是 NVIDIA Maxwell，游戏的着色器是 Maxwell 机器码。要把它们
搬到 Metal 上，第一步是解 Maxwell 指令 → SPIR-V。Ryujinx 的这套翻译器是公开实现里最完整的，
而且它解的就是真实 Switch 游戏里的着色器，所以它的 SPH（着色器程序头）解析与真机一致 ——
本项目正是用它当**判据**来核对 SMO 的 shader blob 结构（见 `smo/tools/probe_sharcb.py` 的注释）。

用法见 `smo/gfx/tools/dmsh_xlate/`。

## deko3d —— 纹理块线性布局的尺寸算法（zlib，**只移植算法，不收录代码**）

来源：<https://github.com/devkitPro/deko3d>，zlib 许可。

**本目录没有 deko3d 的源码**：我们只需要它 `source/dk_image.cpp` 里的**尺寸算法**，
所以把那几个函数按 C 重写进了 `smo/gfx/nvn/nvn_tex.c`，函数一一对位、在注释里标明出处：

| deko3d | 本仓 |
|---|---|
| `pickTileSize` | `tex_pick_tile_size` |
| `adjustTileSize` | `tex_adjust_tile_size` |
| `adjustMipSize` / `adjustBlockSize` | `tex_mip_size` / `tex_div_ceil` |
| `alignLayerSize` | `tex_align_layer_size` |
| `DkImageLayout::calcLevelOffset` | `nvn_tex_level_offset` |

参考项目 d2r-mobile 的 `dm/gfx/nvn/nvn_tex.c` 同样「移植自 deko3d」，本仓沿用同一做法。

**zlib 许可的义务**：不得声称是原作、修改过的版本要标明。`nvn_tex.c` 的文件头写明了
来源与「这是移植、函数对位」，满足这两条。

**一处 deko3d 没有的东西**：GOB 内部 512 字节的重排公式。deko3d **不需要**它 ——
它把解交错交给 GPU（走 2D 引擎的 `dkCmdBufCopyImage`）。我们要在 CPU 侧解（Metal 只认
行优先），所以这个公式取自 <https://github.com/aboood40091/BNTX-Extractor> 的
`swizzle.py`（`getAddrBlockLinear`），并用单测的双射检查与往返测试核对过。

## bcdec —— BC 纹理 CPU 解码（MIT / Unlicense 双授权）

来源：<https://github.com/iOrange/bcdec>，单头文件。

**为什么是必需项，不是可选优化**：苹果 GPU 不支持 BC 系列压缩格式（它用 ASTC）。
实测全部 4335 个 `.szs`（18290 张贴图）里 **96.6% 是 BC**（`smo/gfx/tools/bntx_survey.py --all`），
所以不解码就没有贴图可用。参考项目同样收录它，用途一致。

收录时机：接 Metal 上传那一步。现在 `nvn_tex.c` 已经能把块线性解成行优先的 BC 块，
下一步才需要把 BC 块解成 RGBA。
