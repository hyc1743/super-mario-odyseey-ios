#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""量一遍游戏真实用到的贴图格式与块线性参数 —— 纹理层实现范围的依据。

为什么要先量：纹理层要做多少工作，取决于游戏**实际**用了哪些格式。NVN 声明了 158 个
NVNformat，但一个游戏只会用其中一小撮。苹果 GPU 不支持 BC 系列（它用 ASTC），
所以「BC 占多少」直接决定 bcdec 是必需还是可选 —— 这一条不能靠猜。

贴图在哪（实测）：
  RomFS/**/*.szs  →  Yaz0  →  SARC  →  *.bfres（magic `FRES`，v8）
  .bfres 里内嵌一个 BNTX 容器（magic `BNTX`），BNTX 才是贴图本体。
  并非每个 .bfres 都有 BNTX（只带模型/骨骼的就没有）。

BNTX 结构（只读我们要的那几项，不做完整解析）：
  +0x00 'BNTX'  +0x0C bom  +0x10 版本  +0x20 'NX  ' 平台块
  平台块：+0x04 贴图数(u32)  +0x08 贴图信息数组指针表的偏移(u64)
  每个 BRTI（贴图信息）块里：维度、格式、tile mode、块高、mip 数。

用法：
  bntx_survey.py [--sample N] [--all] [--tsv 输出]
默认抽样 300 个 .szs（全量 4335 个要跑几分钟）。--all 跑全部。
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, "smo", "tools"))
import nintendo_archives as na                                      # noqa: E402

ROMFS = os.path.join(ROOT, "work", "game", "RomFS")
ENUMS = os.path.join(ROOT, "smo", "gfx", "nvn", "enums.tsv")


def load_formats():
    """NVNformat 数值 → 名字（从仓内 enums.tsv，它由 nvn_enum.py 从 nvn.h 抽出）。"""
    out = {}
    with open(ENUMS, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            c = line.rstrip("\n").split("\t")
            if len(c) >= 3 and c[2] == "NVNformat":
                out[int(c[1])] = c[0]
    return out


# BNTX 的格式字段是 u32：高字节 = 格式 id，低字节 = 分量类型（UNORM/SNORM/SRGB/FLOAT…）。
# 实测 CoinCollectM：0x1a06 / 0x1d01 / 0x1e02 —— id 0x1a/0x1d/0x1e 分别是 BC1/BC4/BC5。
# 这张表只放**实测出现过**的 id；没见过的照原样报 id，不编造名字。
FMT_ID = {
    0x02: ("R8", 1, 1, 1),
    0x07: ("RG8", 2, 1, 1),
    0x09: ("R16", 2, 1, 1),
    0x0B: ("RGBA8", 4, 1, 1),
    0x0F: ("RGB10A2", 4, 1, 1),
    0x14: ("R32", 4, 1, 1),
    0x15: ("RG16", 4, 1, 1),
    0x16: ("RGBA16", 8, 1, 1),
    0x1A: ("BC1", 8, 4, 4),
    0x1B: ("BC2", 16, 4, 4),
    0x1C: ("BC3", 16, 4, 4),
    0x1D: ("BC4", 8, 4, 4),
    0x1E: ("BC5", 16, 4, 4),
    0x1F: ("BC6H", 16, 4, 4),
    0x20: ("BC7", 16, 4, 4),
    0x2D: ("ASTC_4x4", 16, 4, 4),
    0x2E: ("ASTC_5x4", 16, 5, 4),
    0x2F: ("ASTC_5x5", 16, 5, 5),
    0x30: ("ASTC_6x5", 16, 6, 5),
    0x31: ("ASTC_6x6", 16, 6, 6),
    0x32: ("ASTC_8x5", 16, 8, 5),
    0x33: ("ASTC_8x6", 16, 8, 6),
    0x34: ("ASTC_8x8", 16, 8, 8),
    0x35: ("ASTC_10x5", 16, 10, 5),
    0x36: ("ASTC_10x6", 16, 10, 6),
    0x37: ("ASTC_10x8", 16, 10, 8),
    0x38: ("ASTC_10x10", 16, 10, 10),
    0x39: ("ASTC_12x10", 16, 12, 10),
    0x3A: ("ASTC_12x12", 16, 12, 12),
}
TYPE_NAME = {
    0x01: "UNORM", 0x02: "SNORM", 0x03: "UINT", 0x04: "SINT",
    0x05: "FLOAT", 0x06: "SRGB", 0x07: "UNORM_FORCE", 0x08: "UI2F",
}


def iter_bntx(blob):
    """在一个 .bfres 里找内嵌的 BNTX，产出每张贴图的参数。

    只读我们要的那几项，不做完整解析 —— 偏移都是实测对出来的（见模块注释）。
    """
    i = blob.find(b"BNTX")
    if i < 0:
        return
    x = blob[i:]
    if len(x) < 0x40 or x[0x20:0x24] != b"NX  ":
        return
    cnt, info_ptr = struct.unpack_from("<IQ", x, 0x24)
    for k in range(min(cnt, 4096)):
        p = info_ptr + k * 8
        if p + 8 > len(x):
            return
        o = struct.unpack_from("<Q", x, p)[0]
        if o + 0x70 > len(x) or x[o:o + 4] != b"BRTI":
            continue
        dim = x[o + 0x11]
        tile = struct.unpack_from("<H", x, o + 0x12)[0]
        mips = struct.unpack_from("<H", x, o + 0x16)[0]
        fmt = struct.unpack_from("<I", x, o + 0x1C)[0]
        w, h, d, arr, layout = struct.unpack_from("<IIIII", x, o + 0x24)
        yield {
            "w": w, "h": h, "d": d, "arr": arr, "mips": mips, "dim": dim,
            "fmt_id": fmt >> 8, "fmt_ty": fmt & 0xFF,
            "tile": tile,
            # layout 的低 3 位是 block height 的 log2（GOB 为单位）—— 这正是
            # deko3d 的 m_tileH。survey 只统计分布，不在这里验证语义。
            "block_h_log2": layout & 7,
        }


def main():
    argv = sys.argv
    sample = 300
    if "--sample" in argv:
        sample = int(argv[argv.index("--sample") + 1])
    if "--all" in argv:
        sample = 0
    tsv = argv[argv.index("--tsv") + 1] if "--tsv" in argv else None

    if not os.path.isdir(ROMFS):
        print(f"缺 {ROMFS} —— 先跑 nsp_extract.py romfs")
        return 2

    files = []
    for dp, _, fn in os.walk(ROMFS):
        for f in fn:
            if f.endswith(".szs"):
                files.append(os.path.join(dp, f))
    files.sort()
    if sample and sample < len(files):
        import random
        random.seed(1)                       # 固定种子：抽样结果可复现
        pick = sorted(random.sample(files, sample))
    else:
        pick = files
    print(f"# .szs 共 {len(files)} 个，本次扫 {len(pick)} 个"
          f"{'（抽样，种子 1）' if len(pick) < len(files) else '（全量）'}")

    fmts = collections.Counter()
    blockh = collections.Counter()
    tiles = collections.Counter()
    dims = collections.Counter()
    sizes = collections.Counter()
    ntex = narch = nbntx = 0
    rows = []
    for p in pick:
        try:
            d = open(p, "rb").read()
            if d[:4] == b"Yaz0":
                d = na.yaz0_decompress(d)
            if d[:4] != b"SARC":
                continue
            s = na.Sarc(d)
            narch += 1
            for name, off, size in s.entries:
                if not name.lower().endswith(".bfres"):
                    continue
                blob = bytes(s.d[off:off + size])
                got = list(iter_bntx(blob))
                if got:
                    nbntx += 1
                for t in got:
                    ntex += 1
                    fid, fty = t["fmt_id"], t["fmt_ty"]
                    fn_ = FMT_ID.get(fid, (f"id{fid:#x}", 0, 1, 1))[0]
                    fmts[f"{fn_}/{TYPE_NAME.get(fty, fty)}"] += 1
                    blockh[t["block_h_log2"]] += 1
                    tiles[t["tile"]] += 1
                    dims[t["dim"]] += 1
                    sizes[f'{t["w"]}x{t["h"]}'] += 1
                    if tsv:
                        rows.append((os.path.basename(p), name, fn_,
                                     TYPE_NAME.get(fty, str(fty)), t["w"], t["h"],
                                     t["d"], t["arr"], t["mips"],
                                     t["block_h_log2"], t["tile"], t["dim"]))
        except Exception:
            continue

    print(f"# 解析 {narch} 个归档，其中 {nbntx} 个 .bfres 带 BNTX，共 {ntex} 张贴图\n")
    bc = sum(c for k, c in fmts.items() if k.startswith("BC"))
    astc = sum(c for k, c in fmts.items() if k.startswith("ASTC"))
    print(f"## 格式分布（{len(fmts)} 种）")
    for k, c in fmts.most_common(30):
        print(f"  {c:7d}  {100.0*c/ntex:5.1f}%  {k}")
    print(f"\n## 关键结论")
    print(f"  BC 系列：{bc} 张（{100.0*bc/ntex:.1f}%）"
          f" ← 苹果 GPU 不支持，必须 CPU 解码（bcdec）")
    print(f"  ASTC：   {astc} 张（{100.0*astc/ntex:.1f}%）← 苹果原生支持")
    print(f"\n## block height（log2，GOB 为单位 = deko3d 的 m_tileH）")
    for k in sorted(blockh):
        print(f"  {blockh[k]:7d}  2^{k} = {1 << k} GOB")
    print(f"\n## tile mode：{dict(tiles)}（0 = 块线性 / 1 = 线性 pitch）")
    print(f"## 维度数：{dict(dims)}")
    print(f"\n## 最常见尺寸")
    for k, c in sizes.most_common(10):
        print(f"  {c:7d}  {k}")

    if tsv:
        with open(tsv, "w", encoding="utf-8", newline="\n") as f:
            f.write("# 归档\t贴图资源\t格式\t分量类型\t宽\t高\t深\t层\tmip\t"
                    "block_h_log2\ttile\t维度\n")
            for r in rows:
                f.write("\t".join(str(v) for v in r) + "\n")
        print(f"\n写出 {tsv}：{len(rows)} 行")
    return 0


if __name__ == "__main__":
    sys.exit(main())
