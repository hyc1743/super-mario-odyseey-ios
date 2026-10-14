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
