#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三段切分的 x0 边界：gen_embed.py 与 guest_dylib.py 共用，保证两条出包路线切法一致。

**x0 向上取整到 16K**（与参考项目 d2r-mobile 一致）：

    x0 = [0, 圆整16K(TEXT_END))        → RX（签名页，可执行、不可写）
    W  = [圆整16K(TEXT_END), PATCH_OFF) → RW（写时复制，可写、不可执行）

TEXT_END 不页对齐时，代码尾与 ro 的开头同在一页。这一页只能二选一：
  · 给 x0（RX）：代码尾能执行；ro 开头只读 —— 前提是**那里没有重定位要写**
  · 给 W（RW）：能写；但代码尾**不能执行**，游戏一跳进 .text 末尾的函数就崩

不存在第三种：iOS 普通 App 拿不到 RWX 页（W 段的 -segprot 也只有 rw,rw）。
早先我们选了「给 W + mprotect 成 RWX」—— macOS/CI 上能跑，真机上 mprotect 必失败。

所以这里在**出包时**就核对「边界页里没有重定位」，有就拒绝出包，而不是把问题留到真机。
实测四个模块（第二十六轮）：sdk / subsdk0 / main 的边界页里 0 条重定位；
rtld 有 32 条 —— 这正是不装 rtld 的原因之一（宿主自己扮演 rtld，参考项目也只装 main + sdk）。

用作脚本：x0_layout.py <产物目录> <模块名...>  逐个报告边界页里的重定位数。
"""
import os
import struct
import sys

PG = 0x4000

R_AARCH64_RELATIVE = 1027


def r16(x):
    return (x + PG - 1) & ~(PG - 1)


def x0_end(text_end):
    """x0 段的终点：TEXT_END 向上取整到 16K。"""
    return r16(text_end)


def reloc_offsets(img):
    """改写产物里全部重定位的写入偏移（RELA + JMPREL）。

    只读 MOD0 → 动态段 → DT_RELA/DT_RELASZ/DT_JMPREL/DT_PLTRELSZ，与装载器的 relocate() 同一口径。
    改写只动代码区的指令字，不动动态段与重定位表，所以对 patched.bin 读与对原映像读结果相同。
    """
    if len(img) < 8:
        return []
    m = struct.unpack_from("<I", img, 4)[0]
    if img[m:m + 4] != b"MOD0":
        raise ValueError("找不到 MOD0")
    o = m + struct.unpack_from("<i", img, m + 4)[0]
    dyn = {}
    while True:
        tag, val = struct.unpack_from("<qQ", img, o)
        o += 16
        if tag == 0:
            break
        dyn.setdefault(tag, val)
    out = []
    for off_tag, sz_tag in ((7, 8), (23, 2)):           # (DT_RELA, DT_RELASZ), (DT_JMPREL, DT_PLTRELSZ)
        if off_tag in dyn and sz_tag in dyn:
            base, size = dyn[off_tag], dyn[sz_tag]
            for i in range(0, size, 24):
                r_off, r_info, _ = struct.unpack_from("<QQq", img, base + i)
                out.append((r_off, r_info & 0xFFFFFFFF))
    return out


def boundary_relocs(img, text_end):
    """落在 [TEXT_END, 圆整16K(TEXT_END)) 的重定位 —— 这一页归 x0（只读），不能有要写的槽。"""
    lo, hi = text_end, x0_end(text_end)
    return [(o, t) for o, t in reloc_offsets(img) if lo <= o < hi]


def check_x0(name, img, text_end):
    """出包前的硬检查。返回 None 表示可以出包；否则返回原因（调用方打印后拒绝出包）。"""
    bad = boundary_relocs(img, text_end)
    if not bad:
        return None
    lo, hi = min(o for o, _ in bad), max(o for o, _ in bad)
    return (f"{name}: 边界页 [0x{text_end:x}, 0x{x0_end(text_end):x}) 里有 {len(bad)} 条重定位"
            f"（0x{lo:x}..0x{hi:x}）。这一页归代码段（只读+执行），写不了 —— "
            f"iOS 上没有 RWX 页可用。这个模块不能按三段内嵌出包"
            f"（rtld 就是这种情况：宿主自己扮演 rtld，不装它）。")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    out, names = sys.argv[1], sys.argv[2:]
    rc = 0
    for n in names:
        hdr = {}
        for line in open(os.path.join(out, f"{n}.fixups.tsv"), encoding="utf-8"):
            if not line.startswith("# "):
                continue
            p = line[2:].split()
            if len(p) == 2:
                hdr[p[0]] = p[1]
        te = int(hdr["TEXT_END"], 16)
        img = open(os.path.join(out, f"{n}.patched.bin"), "rb").read()
        bad = boundary_relocs(img, te)
        state = "OK" if not bad else "拒绝出包"
        print(f"{n:8s} TEXT_END=0x{te:x} x0 终点=0x{x0_end(te):x} 边界页重定位 {len(bad)} 条  {state}")
        if bad:
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
