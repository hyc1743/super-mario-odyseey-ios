#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""跨引用：import 符号 ← PLT 桩 ← bl 调用点。

参考项目里大量用到这个能力（dm/tools/xref_sdk.py 找 SDK 函数被谁调用、patch_nso.py 定 CUTS 落点）。
Switch NSO 的 import 调用经 PLT，典型 4 条指令：
    adrp x16, page ; ldr x17, [x16, #off] ; add x16, x16, #off ; br x17
page+off 即 GOT 槽地址，符号名由动态重定位表给出。这里手写指令解码（不依赖 capstone 的 detail），
用 numpy 做整段扫描。

用法：
  xref.py <模块名> --syms            被调用的 import 符号 : 调用点个数（降序）
  xref.py <模块名> --who <名字片段…> 打印这些符号的调用点
  xref.py <模块名> --stats
读取 work/game/<模块名>.bin（nso_info.py --dump 产出）。
"""
import collections
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso_info

WORK = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "work", "game"))


def load(mod):
    img = open(os.path.join(WORK, mod + ".bin"), "rb").read()
    nso_info._IMG = img
    dyn, _ = nso_info.parse_dynamic(img)
    return img, dyn


def got_map(img, dyn):
    """GOT 槽地址 → 符号名。"""
    symtab, strtab = dyn[6], dyn[5]
    m = {}

    def scan(off, size):
        for i in range(0, size, 24):
            r_off, r_info, _ = struct.unpack_from("<QQq", img, off + i)
            if (r_info & 0xFFFFFFFF) not in (257, 1025, 1026):
                continue
            si = r_info >> 32
            st_name = struct.unpack_from("<I", img, symtab + si * 24)[0]
            end = img.index(b"\0", strtab + st_name)
            m[r_off] = img[strtab + st_name:end].decode(errors="replace")
    if 7 in dyn and 8 in dyn:
        scan(dyn[7], dyn[8])
    if 23 in dyn and 2 in dyn:
        scan(dyn[23], dyn[2])
    return m


def _sx(v, bits):
    return v - (1 << bits) if v >> (bits - 1) else v


def plt_stubs(img, text_end, got):
    """{桩地址: 符号名}。"""
    w = np.frombuffer(img[:text_end], dtype="<u4")
    idx = np.nonzero((w & 0x9F00001F) == 0x90000010)[0]           # adrp x16, page
    stubs = {}
    for i in idx:
        if i + 3 >= len(w):
            continue
        w1, w2, w3 = int(w[i + 1]), int(w[i + 2]), int(w[i + 3])
        if (w1 & 0xFFC00000) != 0xF9400000 or ((w1 >> 5) & 0x1F) != 16 or (w1 & 0x1F) != 17:
            continue      # ldr x17, [x16, #off]
        if (w2 & 0xFF800000) != 0x91000000 or ((w2 >> 5) & 0x1F) != 16 or (w2 & 0x1F) != 16:
            continue      # add x16, x16, #off
        if w3 != 0xD61F0220:
            continue      # br x17
        iw0 = int(w[i])
        page = (i * 4 & ~0xFFF) + (_sx(((iw0 >> 5) & 0x7FFFF) << 2 | ((iw0 >> 29) & 3), 21) << 12)
        off = ((w1 >> 10) & 0xFFF) << 3
        nm = got.get(page + off)
        if nm:
            stubs[i * 4] = nm
    return stubs


def call_sites(img, text_end, stubs):
    w = np.frombuffer(img[:text_end], dtype="<u4")
    idx = np.nonzero((w >> 26) == 0x25)[0]                        # bl
    out = collections.defaultdict(list)
    for i in idx:
        o = i * 4
        imm = int(w[i]) & 0x3FFFFFF
        tgt = o + (_sx(imm, 26) << 2)
        nm = stubs.get(tgt)
        if nm:
            out[nm].append(o)
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    mod = sys.argv[1]
    img, dyn = load(mod)
    # 注意：dump 出来的是摊平映像，头 0x18 字节是 ELF 头而不是 NSO 头；段大小要回原 NSO 读
    text_end = struct.unpack_from("<I", open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20), 0x18)[0]
    got = got_map(img, dyn)
    stubs = plt_stubs(img, text_end, got)
    print(f"# {mod}: GOT 槽 {len(got)}  PLT 桩 {len(stubs)}  .text 尾 0x{text_end:x}", file=sys.stderr)
    if "--stats" in sys.argv:
        return 0
    sites = call_sites(img, text_end, stubs)
    print(f"# 被 bl 到的 import 符号 {len(sites)}/{len(stubs)}", file=sys.stderr)
    if "--syms" in sys.argv:
        for nm, ss in sorted(sites.items(), key=lambda kv: -len(kv[1])):
            print(f"{len(ss):6d}  {nm}")
    elif "--who" in sys.argv:
        for pat in sys.argv[sys.argv.index("--who") + 1:]:
            hit = [(nm, ss) for nm, ss in sites.items() if pat in nm]
            if not hit:
                print(f"{pat}: 没有调用点（桩未被 bl）")
            for nm, ss in hit:
                print(f"{nm}: {len(ss)} 处 -> " + " ".join(f"0x{a:x}" for a in ss))
    return 0


if __name__ == "__main__":
    sys.exit(main())
