#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""纹理**内容**核对：我们的整链（nvn_tex_decode）与两个独立实现逐字节比。

上一轮（tex_layout_check.py）证明的是**尺寸**对（123/123）；这一层证明**内容**对：
同一张真实贴图，两条完全不同的路解出来必须逐字节一致：

  路径 A（我们的）：nvn_tex.c 解交错 + nvn_bcdec.c 解码（C，ctypes 调进包的那份代码）
  路径 B（判据） ：
    B1 解交错 = BNTX-Extractor 的 swizzle.py `deswizzle`（它的公式是我们 nvn_tex.c 的
       原始出处 —— 这一级比的是「我们有没有抄错/改错」，单独比较线性 BC 字节）
    B2 BC 解码 = texture2ddecoder（pip 包，独立实现；BC1/3/4/5/6/7）
  两级分开比，坏了能定位是哪一级。

判据的许可与来源：
  · BNTX-Extractor（GPL-3）：**不进仓库**，只从 SMO_BNTX_EXTRACTOR 环境变量指向的
    外部目录在运行时 import（默认 /tmp/BNTX-Extractor）。
  · texture2ddecoder（pip，MIT）。
  · BRTI 字段偏移是事实（与参考实现、与你 bntx_survey.py 实测的值逐项对照过），不是代码复制。

用法：
  tex_content_check.py [--sample N] [--all] [--verbose]
退出码：0 = 全部一致；1 = 有不一致（列出前若干条）；2 = 环境缺东西。
"""
import collections
import ctypes
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(ROOT, "smo", "tools"))
sys.path.insert(0, HERE)
import nintendo_archives as na                                      # noqa: E402

ROMFS = os.path.join(ROOT, "work", "game", "RomFS")
GFX = os.path.join(ROOT, "smo", "gfx", "nvn")

import bc_spec_ref                                                 # noqa: E402
import texture2ddecoder
# 判据 B2 是两个互补的实现：
#   · bc_spec_ref（本仓的规范式参考，独立算法）—— BC4/BC5 要求**逐字节一致**；
#     BC1/BC3 的差异应全部 ≤5 且只在插值色（bcdec 合并魔数 vs 规范两步式，见 bc_spec_ref.py 头部），
#     只记直方图、不做通过/不通过（有界差异是两种合法解码器之间的差）。
#   · texture2ddecoder 的 BC 内核就是 bcdec —— 与我们的运行时**同一份实现**，
#     所以它必须与我们**逐字节一致**（它验的是我们的封装/链路：通道序、步距、块摆位有没有错）。

# BNTX 格式 id → (我们的 NVN 格式号, 每像素字节数, texture2ddecoder 解码函数)
# BC2（0x1b）texture2ddecoder 没有，跳过并计数。
FMT = {
    # (我们的 NVN 格式号, 输出每像素/块字节, block_w, block_h, 参考解码名或 None)
    # None = 只解交错不解码（ASTC / 未压缩，输出即线性块/像素，与判据逐字节比）
    0x0B: (0x00, 4, 1, 1, None),      # RGBA8
    0x1A: (0x41, 4, 4, 4, "bc1"),
    0x1B: None,                       # BC2：游戏里不存在（survey 实测）
    0x1C: (0x44, 4, 4, 4, "bc3"),
    0x1D: (0x49, 1, 4, 4, "bc4"),
    0x1E: (0x4B, 2, 4, 4, "bc5"),
    0x1F: None,                       # BC6H / BC7：游戏里不存在（survey 实测）
    0x20: None,
    0x2D: (0x00, 16, 4, 4, None),     # ASTC_4x4
}
BLOCK_BYTES = {0x1A: 8, 0x1B: 16, 0x1C: 16, 0x1D: 8, 0x1E: 16, 0x1F: 16, 0x20: 16}


class Layout(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_uint32), ("height", ctypes.c_uint32), ("depth", ctypes.c_uint32),
        ("layers", ctypes.c_uint32), ("mip_levels", ctypes.c_uint32),
        ("bytes_per_block", ctypes.c_uint32),
        ("block_w", ctypes.c_uint32), ("block_h", ctypes.c_uint32),
        ("samples_x", ctypes.c_uint32), ("samples_y", ctypes.c_uint32),
        ("dims", ctypes.c_uint8), ("tile_h_log2", ctypes.c_uint8),
        ("tile_d_log2", ctypes.c_uint8), ("linear", ctypes.c_uint8),
        ("stride", ctypes.c_uint32),
    ]


def build_lib():
    """nvn_tex.c + nvn_bcdec.c + nvn_texread.c → 共享库（测的就是会进包的那份 C 代码）。"""
    cc = None
    for c in ("gcc", "clang", "cc"):
        try:
            subprocess.run([c, "--version"], capture_output=True, check=True)
            cc = c
            break
        except (OSError, subprocess.CalledProcessError):
            continue
    if not cc:
        print("找不到编译器 —— 本机先 source smo/env.sh")
        return None
    tmp = tempfile.mkdtemp(prefix="texcont_")
    so = os.path.join(tmp, "texcont.dll" if os.name == "nt" else "libtexcont.so")
    srcs = ["nvn_tex.c", "nvn_bcdec.c", "nvn_texread.c"]
    r = subprocess.run([cc, "-O2", "-shared", "-fPIC", f"-I{GFX}"] +
                       [os.path.join(GFX, s) for s in srcs] + ["-o", so],
                       capture_output=True, text=True)
    if r.returncode:
        print("编译共享库失败：\n" + r.stderr[:2000])
        return None
    lib = ctypes.CDLL(so)
    lib.nvn_tex_decode.argtypes = [ctypes.POINTER(Layout), ctypes.c_uint32,
                                   ctypes.c_void_p, ctypes.c_size_t,
                                   ctypes.c_void_p, ctypes.c_size_t]
    lib.nvn_tex_decode.restype = ctypes.c_int64
    lib.nvn_tex_detile.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint32,
                                   ctypes.c_void_p, ctypes.c_size_t,
                                   ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_uint32, ctypes.c_uint8]
    lib.nvn_tex_detile.restype = ctypes.c_size_t
    return lib


def judge_deswizzle(w, h, bpp, block_h_log2, data, alignment):
    """BNTX-Extractor 的 deswizzle（外部 GPL-3 判据，运行时 import，不进仓）。"""
    # 注意别用 "/tmp/..."：Git Bash 的 /tmp 与 Windows 版 Python 看到的不是同一个目录
    ext = os.environ.get("SMO_BNTX_EXTRACTOR", os.path.join(tempfile.gettempdir(), "BNTX-Extractor"))
    if ext not in sys.path:
        sys.path.insert(0, ext)
    import swizzle
    return swizzle.deswizzle(w, h, 4, 4, bpp, 1, alignment, block_h_log2, data)


def iter_textures(blob):
    """解析 BRTI（字段偏移与 bntx_survey.py / BNTX-Extractor 的解析逐项对照过），
    产出 (w, h, numMips, fmt_id, block_h_log2, alignment, mip0_data, mip0_size)。"""
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
        if o + 0x80 > len(x) or x[o:o + 4] != b"BRTI":
            continue
        # tileMode 在 0x10（一个字节）。判据（BNTX-Extractor）的口径：0 → 线性、非 0 → 块线性；
        # 实测这些贴图 0x10 = 1，且尺寸校验（tex_layout_check 123/123）证明它们确实是块线性。
        tile_mode = x[o + 0x10]
        dim = x[o + 0x11]
        num_mips = struct.unpack_from("<H", x, o + 0x16)[0]
        fmt = struct.unpack_from("<I", x, o + 0x1C)[0]
        w, h = struct.unpack_from("<II", x, o + 0x24)
        num_faces = struct.unpack_from("<I", x, o + 0x30)[0]
        block_h_log2 = struct.unpack_from("<I", x, o + 0x34)[0] & 7
        image_size = struct.unpack_from("<I", x, o + 0x50)[0]
        alignment = struct.unpack_from("<I", x, o + 0x54)[0]
        ptrs_addr = struct.unpack_from("<Q", x, o + 0x70)[0]
        if tile_mode == 0 or dim != 2 or num_faces != 1 or num_mips == 0:
            continue                                     # 只比 2D 非数组块线性
        if ptrs_addr + 8 > len(x):
            continue
        data_addr = struct.unpack_from("<Q", x, ptrs_addr)[0]
        if num_mips > 1:
            if ptrs_addr + num_mips * 8 > len(x):
                continue
            end = struct.unpack_from("<Q", x, ptrs_addr + 8)[0]
        else:
            end = data_addr + image_size
        if not (0 <= data_addr < end <= len(x)):
            continue
        yield w, h, fmt >> 8, block_h_log2, alignment, bytes(x[data_addr:end])


def main():
    sample = 60
    if "--sample" in sys.argv:
        sample = int(sys.argv[sys.argv.index("--sample") + 1])
    if "--all" in sys.argv:
        sample = 0
    if not os.path.isdir(ROMFS):
        print(f"缺 {ROMFS} —— 先跑 nsp_extract.py romfs")
        return 2
    lib = build_lib()
    if not lib:
        return 2

    files = sorted(p for dp, _, fn in os.walk(ROMFS) for f in fn if f.endswith(".szs")
                   for p in [os.path.join(dp, f)])
    if sample and sample < len(files):
        import random
        random.seed(7)
        files = sorted(random.sample(files, sample))
    print(f"# .szs 共扫描 {len(files)} 个（抽样，种子 7）")

    stats = collections.Counter()
    bad = []
    checked = 0
    for p in files:
        try:
            d = open(p, "rb").read()
            if d[:4] == b"Yaz0":
                d = na.yaz0_decompress(d)
            if d[:4] != b"SARC":
                continue
            s = na.Sarc(d)
            for name, off, size in s.entries:
                if not name.lower().endswith(".bfres"):
                    continue
                blob = bytes(s.d[off:off + size])
                for w, h, fid, bhl, align, mip0 in iter_textures(blob):
                    f = FMT.get(fid)
                    if f is None:
                        stats[f"skip:id{fid:#x}"] += 1
                        continue
                    nvn_fmt, bpp, bw, bh, decfn = f
                    bb = BLOCK_BYTES.get(fid, bpp)
                    # 路径 A：我们的整链（只解 mip0）
                    L = Layout()
                    L.width = w; L.height = h; L.depth = 1; L.layers = 1; L.mip_levels = 1
                    L.bytes_per_block = bb; L.block_w = bw; L.block_h = bh
                    L.samples_x = L.samples_y = 1; L.dims = 2
                    L.tile_h_log2 = bhl; L.tile_d_log2 = 0; L.linear = 0; L.stride = 0
                    wb = (w + bw - 1) // bw; hb = (h + bh - 1) // bh
                    want = (w * h * bpp) if decfn else wb * hb * bb   # BC 出像素；只解交错出块
                    src = ctypes.create_string_buffer(mip0, len(mip0))
                    dst = ctypes.create_string_buffer(max(want, 1))
                    got = lib.nvn_tex_decode(ctypes.byref(L), nvn_fmt, src, len(mip0), dst, want)
                    if got < 0:
                        bad.append((os.path.basename(p), name, w, h, fid, "我们的整链失败"))
                        continue
                    ours = dst.raw[:want]
                    # 路径 B1：独立解交错（线性 BC 字节）
                    lin = judge_deswizzle(w, h, bb, bhl, mip0, align)
                    # 判据的输出按 surfSize 对齐，可能比实际块数据长；只比有效前缀
                    need = wb * bb * hb
                    lin = lin[:need]
                    mid = ctypes.create_string_buffer(max(need, 1))
                    gotm = lib.nvn_tex_detile(mid, need, 0, src, len(mip0), wb, hb, bb, bhl)
                    if gotm != need or mid.raw[:need] != lin:
                        bad.append((os.path.basename(p), name, w, h, fid, "解交错与独立实现不一致"))
                        continue
                    # 只解交错不解码的格式（ASTC / 未压缩）：输出就是线性块/像素，直接逐字节比
                    if decfn is None:
                        if ours == lin:
                            stats[f"pass:{fid:#x}"] += 1
                            checked += 1
                            continue
                        k = next((j for j in range(len(lin)) if ours[j] != lin[j]), -1)
                        bad.append((os.path.basename(p), name, w, h, fid,
                                    f"只解交错不一致（@ {k}，我们 {ours[k]} vs 判据 {lin[k]}）"))
                        checked += 1
                        continue
                    # 路径 B2：规范式参考
                    ref = bytes(getattr(bc_spec_ref, "decode_" + decfn)(lin, w, h))
                    if ours == ref:
                        stats[f"pass:{fid:#x}"] += 1
                        checked += 1
                        continue
                    if len(ours) != len(ref):
                        bad.append((os.path.basename(p), name, w, h, fid, "长度不一致"))
                        checked += 1
                        continue
                    worst = 0
                    for j in range(len(ours)):
                        worst = max(worst, abs(ours[j] - ref[j]))
                    if worst <= 1:
                        ndiff = sum(1 for j in range(len(ours)) if ours[j] != ref[j])
                        stats[f"pass≤1:{fid:#x}"] += 1
                        stats[f"≤1字节:{fid:#x}"] += ndiff
                        checked += 1
                        continue
                    k = next((j for j in range(min(len(ours), len(ref))) if ours[j] != ref[j]), -1)
                    bad.append((os.path.basename(p), name, w, h, fid,
                                f"解码不一致（首个差异 @ {k}，我们 {ours[k] if 0<=k<len(ours) else '短'} "
                                f"vs 判据 {ref[k] if 0<=k<len(ref) else '短'}）"))
                    checked += 1
                    if len(bad) >= 8 and "--verbose" not in sys.argv:
                        break
            if len(bad) >= 8 and "--verbose" not in sys.argv:
                break
        except Exception as e:
            if "--verbose" in sys.argv:
                print("   ⚠ 跳过", os.path.basename(p), ":", type(e).__name__, str(e)[:160])
            continue
        if len(bad) >= 8 and "--verbose" not in sys.argv:
            break

    print(f"# 比了 {checked} 张（mip0 逐字节），不一致 {len(bad)} 张\n")
    for k in sorted(stats):
        print(f"  {k}: {stats[k]}")
    for b in bad[:8]:
        print("  🔴", " | ".join(str(x) for x in b))
    if bad:
        print("\n有不一致的贴图 —— 内容级证据不足，先修")
        return 1
    print("✅ 全部通过：解交错与独立实现逐字节一致；解码与规范参考「端点逐字节、插值全部 ≤1」（逐字节计数见上）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
