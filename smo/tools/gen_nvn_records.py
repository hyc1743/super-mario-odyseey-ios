#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 `names.txt` 生成记录桩的实现（`smo/gfx/nvn/nvn_record_names.inc`）。

为什么单独一个脚本：`hle_nvn.c` 里那些「已实现家族」的函数有几百个，签名统一（8 个 uint64 → uint64），
所以一条宏就能全部展开 —— 手写几百行没有意义，而且加/去名字时容易漏。
状态实现逐批替换它们：某个名字在 hle_nvn.c 里手写实现后，把它加进 SKIP 即可。

用法：gen_nvn_records.py [--check]
"""
import os
import sys
from gen_nvn import COMMAND_IMPLEMENTED, QUEUE_IMPLEMENTED, STATE_FAMILIES, RESOURCE_FAMILIES, RESOURCE_HANDLES, WINDOW_IMPLEMENTED

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NAMES = os.path.join(ROOT, "smo", "gfx", "nvn", "names.txt")
OUT = os.path.join(ROOT, "smo", "gfx", "nvn", "nvn_record_names.inc")

# 与 gen_nvn.py 的 IMPLEMENTED_FAMILIES 保持一致
FAMILIES = ["nvnDevice", "nvnMemoryPool", "nvnBuffer", "nvnSync", "nvnWindow",
            "nvnProgram", "nvnTexture", "nvnSampler", "nvnQueue", "nvnEvent"]
FAMILIES += STATE_FAMILIES

# 已经在 hle_nvn.c 里手写了状态实现的名字（逐步往这里加）
SKIP = {
    'nvnTextureBuilderSetDevice',
    'nvnTextureBuilderSetWidth',
    'nvnTextureBuilderSetHeight',
    'nvnTextureBuilderSetDepth',
    'nvnTextureBuilderSetLevels',
    'nvnTextureBuilderSetFormat',
    'nvnTextureBuilderSetFlags',
    'nvnTextureBuilderSetTarget',
    'nvnTextureBuilderSetSamples',
    'nvnTextureBuilderSetStride',
    'nvnTextureBuilderSetSize1D',
    'nvnTextureBuilderSetSize2D',
    'nvnTextureBuilderSetSize3D',
    'nvnTextureBuilderSetStorage',
    'nvnTextureBuilderSetDefaults',
    'nvnTextureBuilderGetStorageSize',
    'nvnTextureBuilderGetStorageAlignment',
    'nvnTextureInitialize',
    'nvnTextureFinalize',

    "nvnBootstrapLoader",            # 本体（不是表项）
    # 以下在 nvn_core.c 里手写了状态实现（见该文件）；每次新增实现都要往这里加，
    # 否则生成的记录桩会和真实现重复定义（链接期报 duplicate symbol）。
    "nvnDeviceBuilderSetDefaults", "nvnDeviceBuilderSetFlags",
    "nvnDeviceInitialize", "nvnDeviceFinalize", "nvnDeviceGetInteger", "nvnDeviceGetProcAddress",
    "nvnDeviceGetTimestampInNanoseconds",
    "nvnMemoryPoolBuilderSetDefaults", "nvnMemoryPoolBuilderSetDevice",
    "nvnMemoryPoolBuilderSetStorage", "nvnMemoryPoolBuilderSetFlags",
    "nvnMemoryPoolInitialize", "nvnMemoryPoolFinalize", "nvnMemoryPoolMap",
    "nvnMemoryPoolGetFlags", "nvnMemoryPoolGetBufferAddress", "nvnMemoryPoolFlushMappedRange", "nvnMemoryPoolInvalidateMappedRange",
    "nvnBufferBuilderSetDefaults", "nvnBufferBuilderSetDevice", "nvnBufferBuilderSetStorage",
    "nvnBufferInitialize", "nvnBufferFinalize", "nvnBufferGetAddress", "nvnBufferMap",
    "nvnSyncInitialize", "nvnSyncFinalize", "nvnSyncWait",
    "nvnSamplerBuilderSetDefaults", "nvnSamplerBuilderSetDevice",
    "nvnSamplerBuilderSetMinMagFilter", "nvnSamplerBuilderSetWrapMode",
    "nvnSamplerBuilderSetLodBias", "nvnSamplerBuilderSetMaxAnisotropy",
    "nvnSamplerBuilderSetCompare", "nvnSamplerBuilderSetReductionFilter",
    "nvnSamplerBuilderSetLodClamp", "nvnSamplerBuilderSetBorderColor",
    "nvnSamplerBuilderSetBorderColori", "nvnSamplerBuilderSetBorderColorui",
    "nvnSamplerInitialize", "nvnSamplerFinalize",
    "nvnProgramInitialize", "nvnProgramFinalize", "nvnProgramSetShaders",
}
SKIP.update(WINDOW_IMPLEMENTED)
SKIP.update({"nvnTextureBuilderSetSwizzle", "nvnTextureBuilderSetDepthStencilMode",
             "nvnTextureBuilderSetPackagedTextureLayout", "nvnTextureBuilderGetSwizzle",
             "nvnTextureGetSwizzle", "nvnTextureBuilderGetDepthStencilMode",
             "nvnTextureGetDepthStencilMode"})
SKIP.update("nvn" + prefix + "Get" + suffix
            for prefix in ("SamplerBuilder", "Sampler")
            for suffix in ("MinMagFilter", "WrapMode", "Compare", "LodClamp", "LodBias",
                           "MaxAnisotropy", "ReductionFilter", "BorderColor", "BorderColori", "BorderColorui"))

HEAD = """// 【自动生成，不要手改】由 smo/tools/gen_nvn_records.py 从 names.txt 生成。
// 作用：为「已实现家族」里还没手写状态实现的名字展开记录桩（统一签名 → 一条宏）。
// 记录桩把调用与参数写进命令记录（Metal 重放的接缝），首次调用打印一行。
"""


def main():
    names = [l.strip() for l in open(NAMES, encoding="utf-8") if l.strip() and not l.startswith("#")]
    impl = sorted(n for n in names if any(n.startswith(p) for p in FAMILIES) or n in COMMAND_IMPLEMENTED)
    todo = [n for n in impl if n not in SKIP and n not in COMMAND_IMPLEMENTED and n not in QUEUE_IMPLEMENTED
            and n not in RESOURCE_HANDLES and not any(n.startswith(p) for p in STATE_FAMILIES + RESOURCE_FAMILIES)]
    lines = [HEAD,
             "#define NVN_REC(name) uint64_t nvn_##name(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, \\",
             "    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) { \\",
             '    return nvn_record_call("nvn" #name, a0, a1, a2, a3, a4, a5, a6, a7); }']
    for n in todo:
        lines.append(f"NVN_REC({n[3:]})")
    lines.append("#undef NVN_REC")
    lines.append("")
    text = "\n".join(lines)
    if "--check" in sys.argv:
        cur = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if cur != text:
            print(f"🔴 {OUT} 与 names.txt 不一致：重跑 gen_nvn_records.py")
            return 1
        print(f"✓ {OUT} 一致（{len(todo)} 个记录桩）")
        return 0
    open(OUT, "w", encoding="utf-8", newline="\n").write(text)
    print(f"已生成 {OUT}：{len(todo)} 个记录桩（家族共 {len(impl)} 个名字，手写实现 {len(impl) - len(todo)} 个）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
