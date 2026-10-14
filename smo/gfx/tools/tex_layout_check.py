#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用游戏自己的贴图核对块线性布局算法 —— 这一层最硬的判据。

单测能证明「算法自洽」（GOB 双射、解交错可逆、尺寸单调），但证明不了
「算出来的大小和 Nintendo 工具链算的一样」。后者需要真实数据：

  BNTX 的每个 BRTI 块里记着这张贴图**实际占用的字节数**（它是打包工具按
  真实布局算出来写进去的）。拿我们的 nvn_tex.c 算一遍，和它比。对得上，
  说明移植自 deko3d 的尺寸算法与真机一致；对不上，就是算法还有缺口。

这是本机能做到的最强验证 —— 不需要 Metal、不需要真机。

用法：
  tex_layout_check.py [--sample N] [--all] [--verbose]
退出码：0 = 全部对上；1 = 有不一致（会列出前若干条）。
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
from bntx_survey import FMT_ID, iter_bntx                           # noqa: E402

ROMFS = os.path.join(ROOT, "work", "game", "RomFS")
GFX = os.path.join(ROOT, "smo", "gfx", "nvn")


class Layout(ctypes.Structure):
    """对位 nvn_tex.h 的 nvn_tex_layout（字段顺序必须一致）。"""
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
    """把 nvn_tex.c 编成共享库，用 ctypes 直接调 —— 测的就是会进包的那份 C 代码，
    而不是在 Python 里重写一遍（重写就只是在验我的 Python）。"""
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
    tmp = tempfile.mkdtemp(prefix="texchk_")
    so = os.path.join(tmp, "nvn_tex.dll" if os.name == "nt" else "libnvn_tex.so")
    r = subprocess.run([cc, "-O2", "-shared", "-fPIC", f"-I{GFX}",
                        os.path.join(GFX, "nvn_tex.c"), "-o", so],
                       capture_output=True, text=True)
    if r.returncode:
        print("编译 nvn_tex.c 失败：\n" + r.stderr[:2000])
        return None
    lib = ctypes.CDLL(so)
    lib.nvn_tex_storage_size.argtypes = [ctypes.POINTER(Layout)]
    lib.nvn_tex_storage_size.restype = ctypes.c_uint64
    lib.nvn_tex_default_tile_h.argtypes = [ctypes.c_uint32] * 3
    lib.nvn_tex_default_tile_h.restype = ctypes.c_uint8
    return lib


def brti_sizes(blob):
    """从 .bfres 里内嵌的 BNTX 读出每张贴图「工具链算出的实际大小」。

    BRTI 里「数据总大小」在 **+0x50**（u32）。这个偏移是逐字段 dump 对出来的，不是猜的：
    +0x20 看着像（值 32）但那是别的字段 —— 它对每张贴图恒为 32，一开始误用了它，
    于是所有样本都"差 +177120"这种离谱的量，反而暴露了取错字段。
    CoinCollectM 的 128×128 BC1/mips=8 在 +0x50 处是 0x3400 = 13312，与我们算的一致。

    名字指针在 +0x60（u64 → u16 长度 + UTF-8）。
    """
    i = blob.find(b"BNTX")
    if i < 0:
        return []
    x = blob[i:]
    if len(x) < 0x40 or x[0x20:0x24] != b"NX  ":
        return []
    cnt, info_ptr = struct.unpack_from("<IQ", x, 0x24)
    out = []
    for k in range(min(cnt, 4096)):
        p = info_ptr + k * 8
        if p + 8 > len(x):
            break
        o = struct.unpack_from("<Q", x, p)[0]
        if o + 0x70 > len(x) or x[o:o + 4] != b"BRTI":
            continue
        total = struct.unpack_from("<I", x, o + 0x50)[0]
        name = ""
        nameptr = struct.unpack_from("<Q", x, o + 0x60)[0]
        if 0 < nameptr < len(x) - 2:
            nl = struct.unpack_from("<H", x, nameptr)[0]
            if nl < 256:
                name = x[nameptr + 2:nameptr + 2 + nl].decode(errors="replace")
        out.append((total, name))
    return out


def main():
    argv = sys.argv
    sample = 150
    if "--sample" in argv:
        sample = int(argv[argv.index("--sample") + 1])
    if "--all" in argv:
        sample = 0
    verbose = "--verbose" in argv

    if not os.path.isdir(ROMFS):
        print(f"缺 {ROMFS} —— 先跑 nsp_extract.py romfs")
        return 2
    lib = build_lib()
    if not lib:
        return 2

    files = []
    for dp, _, fn in os.walk(ROMFS):
        for f in fn:
            if f.endswith(".szs"):
                files.append(os.path.join(dp, f))
    files.sort()
    if sample and sample < len(files):
        import random
        random.seed(1)
        files = sorted(random.sample(files, sample))
    print(f"# 扫 {len(files)} 个 .szs，核对「我们算的大小」vs「BNTX 里记的大小」")

    ok = bad = skipped = 0
    bad_rows = []
    by_fmt_bad = collections.Counter()
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
                texs = list(iter_bntx(blob))
                sizes = brti_sizes(blob)
                if len(texs) != len(sizes):
                    skipped += len(texs)
                    continue
                for t, (want, tname) in zip(texs, sizes):
                    info = FMT_ID.get(t["fmt_id"])
                    if not info:
                        skipped += 1          # 没见过的格式：不编造参数
                        continue
                    _fn, bpb, bw, bh = info
                    L = Layout()
                    L.width, L.height, L.depth = t["w"], t["h"], max(t["d"], 1)
                    L.layers = max(t["arr"], 1)
                    L.mip_levels = max(t["mips"], 1)
                    L.bytes_per_block, L.block_w, L.block_h = bpb, bw, bh
                    L.samples_x = L.samples_y = 1
                    L.dims = max(t["dim"], 2)
                    L.tile_h_log2 = t["block_h_log2"]
                    L.tile_d_log2 = 0
                    L.linear = 1 if t["tile"] else 0
                    L.stride = 0
                    got = lib.nvn_tex_storage_size(ctypes.byref(L))
                    if got == want:
                        ok += 1
                    else:
                        bad += 1
                        by_fmt_bad[f'{_fn} {t["w"]}x{t["h"]} mips={t["mips"]}'] += 1
                        if len(bad_rows) < 20:
                            bad_rows.append(
                                f'{os.path.basename(p)}::{tname or name} {_fn} '
                                f'{t["w"]}x{t["h"]} arr={t["arr"]} mips={t["mips"]} '
                                f'tile_h={t["block_h_log2"]}  '
                                f'我们 {got}  BNTX {want}  差 {got - want:+d}')
        except Exception as e:
            if verbose:
                print(f"  跳过 {os.path.basename(p)}: {e}")
            continue

    tot = ok + bad
    print(f"\n# 对上 {ok}/{tot}"
          f"{f'（{100.0*ok/tot:.1f}%）' if tot else ''}"
          f"，不一致 {bad}，跳过 {skipped}（格式未知或条目数不匹配）")
    if bad_rows:
        print("\n# 前若干条不一致：")
        for r in bad_rows:
            print("  " + r)
        print("\n# 不一致最多的形状：")
        for k, c in by_fmt_bad.most_common(10):
            print(f"  x{c:<4} {k}")
    if bad == 0 and ok > 0:
        print("\n所有贴图的大小都与 BNTX 记录一致 —— 尺寸算法与 Nintendo 工具链对得上。")
        return 0
    if tot == 0:
        print("\n没有可核对的样本。")
        return 1
    return 1


if __name__ == "__main__":
    sys.exit(main())
