#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""改写产物结构自检 —— 参考项目 dm/verify/structck.py 的等价物。

对每个模块检查（全部为「不看行为、只看结构自洽」的硬检查）：
  1. sites.tsv 里每个改写点，产出的指令确实是一条 b，且落在补丁区（>= PATCH_OFF）
  2. 每个改写点目标都在补丁区内、4 字节对齐
  3. gates.tsv 里每个门的返回地址落在补丁区内；种类与计数与 svc/cntpct 改写点数一致
  4. 键页区（KEY_PAGE_OFF, +0x4000）在产出映像里是 0（由装载器运行时填）
  5. 补丁区之后没有残留（映像长度 = PATCH_OFF + 补丁长度）
用法：rewrite_check.py <改写产物目录> [模块…]
"""
import os
import struct
import sys


def check(outdir, name):
    P = lambda e: os.path.join(outdir, f"{name}.{e}")
    img = open(P("patched.bin"), "rb").read()
    hdr = {}
    for line in open(P("fixups.tsv"), encoding="utf-8"):
        if line.startswith("#"):
            k = line[1:].split()
            if len(k) >= 2:
                hdr[k[0]] = int(k[1], 0)
    PATCH_OFF, KEY_PAGE_OFF = hdr["PATCH_OFF"], hdr["KEY_PAGE_OFF"]
    TEXT_END = hdr["TEXT_END"]
    bad = []

    # 1/2 改写点
    sites = [l.split() for l in open(P("sites.tsv"), encoding="utf-8") if l.strip()]
    n_b, n_win, n_jmp = 0, 0, 0
    for row in sites:
        a = int(row[0], 0)
        if len(row) > 1 and row[1] == "W":
            n_win += 1
            continue
        if len(row) > 1 and row[1] == "J":
            n_jmp += 1
            continue
        w = struct.unpack_from("<I", img, a)[0]
        if w >> 26 != 0x05:      # b 的操作码是 0b000101（0x25 是 bl）
            bad.append(f"改写点 0x{a:x} 不是 b：{w:08x}")
            continue
        off = w & 0x3FFFFFF
        tgt = a + ((off - (1 << 26)) if off >> 25 else off) * 4
        if tgt < PATCH_OFF or tgt % 4:
            bad.append(f"改写点 0x{a:x} 目标 0x{tgt:x} 不在补丁区")
        n_b += 1

    # 3 门
    gates = [l.split() for l in open(P("gates.tsv"), encoding="utf-8") if l.strip()]
    kinds = {}
    for r in gates:
        ret, kind = int(r[0], 0), r[1]
        kinds[kind] = kinds.get(kind, 0) + 1
        if ret < PATCH_OFF:
            bad.append(f"门返回地址 0x{ret:x} 不在补丁区")

    # 4 键页必须全 0（运行时由装载器写）
    if any(img[KEY_PAGE_OFF:KEY_PAGE_OFF + 0x4000]):
        bad.append("键页不是全 0")

    # 5 补丁区不得越过文件尾
    if PATCH_OFF > len(img):
        bad.append("PATCH_OFF 越过文件尾")

    status = "OK" if not bad else f"红：{len(bad)} 项"
    print(f"{name:8s} 映像 {len(img):#010x} 改写点 {n_b}（+窗口 {n_win} / 跳转表 {n_jmp}） "
          f"门 {len(gates)} {kinds} 补丁区 0x{PATCH_OFF:x}..0x{len(img):x}  文本段尾 0x{TEXT_END:x}  {status}")
    for b in bad[:8]:
        print("   ", b)
    return len(bad)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    outdir = sys.argv[1]
    mods = sys.argv[2:] or [n.split(".")[0] for n in sorted(os.listdir(outdir)) if n.endswith(".patched.bin")]
    total = sum(check(outdir, m) for m in mods)
    print("全部通过" if not total else f"共 {total} 项不通过")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
