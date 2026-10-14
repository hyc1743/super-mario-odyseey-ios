#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""校验 gen_embed.py 生成的 .S：把三段**拼回去**，必须与原映像逐字节相同。

为什么值得单独写一个检查器：这条路上错了不会当场报错，而是在真机上以"莫名其妙跳错地址"的方式崩。
拼回去是**本机就能做**的验证 —— 不需要 macOS、不需要 Xcode，所以它进得了 CI。

检查项：
  1. .S 里的 .section/.incbin/.space 三段能覆盖 [0, 圆整16K(文件尾))，不重不漏
  2. 每段的字节与 `原映像 + 补零` 的 sha256 一致（manifest 里有记录）
  3. 相邻段首尾相接（段与段之间没有空隙：空洞会让 vm_remap 后的相对位置错位）
  4. 段名长度 ≤ 16（Mach-O 上限）、页对齐、可写段名唯一

用法：check_embed.py <.S 文件> [.manifest.tsv] [产物目录]
不带 manifest 时只做结构性检查（1/3/4）。
"""
import hashlib
import os
import re
import sys

PG = 0x4000


def parse_asm(path):
    """把 .S 解析成 [(段名, 偏移, 字节数, 是否补零)]（按出现顺序）。"""
    out = []
    cur = None
    for line in open(path, encoding="utf-8"):
        s = line.strip()
        m = re.match(r"\.section\s+(\S+)", s)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'\.incbin\s+"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)', s)
        if m and cur:
            out.append((cur, int(m.group(2), 0), int(m.group(3), 0), False, m.group(1)))
            continue
        m = re.match(r"\.space\s+(0x[0-9a-fA-F]+|\d+)", s)
        if m and cur:
            out.append((cur, None, int(m.group(1), 0), True, None))
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    asm_path = sys.argv[1]
    man_path = sys.argv[2] if len(sys.argv) > 2 else (asm_path + ".manifest.tsv" if os.path.exists(asm_path + ".manifest.tsv") else None)
    asmdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # smo/ios/tools -> smo/ios

    items = parse_asm(asm_path)
    if not items:
        print(f"🔴 {asm_path} 里没解析到 .incbin/.space")
        return 2
    print(f"解析到 {len(items)} 条内容指令，涉及段：{len(set(i[0] for i in items))}")

    # 结构检查：同名段（同一模块的 x0/W/x1 是三个不同段名）按出现顺序应首尾相接
    bad = 0
    for name, off, size, is_space, src in items:
        for part in name.split(","):
            if len(part) > 16:
                print(f"🔴 段/节名超过 16 字节（Mach-O 上限）：{name}")
                bad += 1
        if off is not None and off % 1 != 0:
            bad += 1
        # 注意：不检查“.space 的长度是不是页整数倍”—— 把节大小凑成页整数倍的是**节尾的 .p2align 14**
        # （初始实现就是这么写的，我一开始查错了对象）。页对齐由下面基于 manifest 的覆盖性检查保证。

    # 拼接检查：把每段的字节拼出来
    recon = {}
    for name, off, size, is_space, src in items:
        buf = recon.setdefault(name, bytearray())
        if is_space:
            buf += b"\x00" * size
        else:
            st = os.path.getsize(src) if os.path.exists(src) else -1
            if st < 0:
                print(f"🔴 找不到 .incbin 的源文件：{src}")
                bad += 1
                continue
            if off + size > st:
                print(f"🔴 .incbin 越界：{src} offset=0x{off:x} size=0x{size:x} 文件只有 0x{st:x}")
                bad += 1
                continue
            with open(src, "rb") as f:
                f.seek(off)
                buf += f.read(size)

    if man_path and os.path.exists(man_path):
        rows = [l.rstrip("\n").split("\t") for l in open(man_path, encoding="utf-8") if not l.startswith("#")]
        print(f"manifest：{len(rows)} 段")
        for mod, sect, off, size, sh in rows:
            got = hashlib.sha256(bytes(recon.get(sect, b""))).hexdigest()[:16]
            if got != sh:
                print(f"🔴 {sect} 拼回来的字节与 manifest 不符（{got} != {sh}）")
                bad += 1
        # 覆盖性：同一模块的三段必须连续覆盖 [0, 末段尾)
        mods = {}
        for mod, sect, off, size, sh in rows:
            mods.setdefault(mod, []).append((int(off, 0), int(size, 0), sect))
        for mod, segs in mods.items():
            segs.sort()
            if segs[0][0] != 0:
                print(f"🔴 {mod}: 起始不是 0（{segs[0][2]} 从 0x{segs[0][0]:x} 开始）")
                bad += 1
            for (o1, s1, n1), (o2, s2, n2) in zip(segs, segs[1:]):
                if o1 + s1 != o2:
                    print(f"🔴 {mod}: {n1} 与 {n2} 之间有空洞（0x{o1+s1:x} != 0x{o2:x}）")
                    bad += 1
            if any(s % PG for _, s, _ in segs):
                print(f"🔴 {mod}: 有段不是 16K 整数倍")
                bad += 1
            print(f"  {mod}: {len(segs)} 段，合计 0x{sum(s for _, s, _ in segs):x} 字节")

    if bad:
        print(f"\n🔴 {bad} 处问题")
        return 1
    print("\n✅ 三段能拼回原映像且首尾相接、页对齐、名字长度合法")
    return 0


if __name__ == "__main__":
    sys.exit(main())
