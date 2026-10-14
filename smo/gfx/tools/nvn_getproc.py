#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""挖出「游戏在哪些地方解析了哪个 NVN 函数」—— 图形层最硬的那块证据。

背景（实测）：
  main 只 import 一个图形入口 `nvnBootstrapLoader`，3 个调用点。第一个调用点里
  `nvnBootstrapLoader("nvnDeviceGetProcAddress")` 拿到 nvnDeviceGetProcAddress，
  之后**其余全部 NVN 函数都是用它按名字取的**（这就是为什么只有 3 个 bootstrap 调用点）。
  所以「谁被取过」在二进制里表现为：`adrp/add` 装出一个以 "nvn" 开头的字符串地址，
  紧接着的若干条指令里有一次间接调用。

本工具做的事：扫全 `.text`，找所有 adrp+add（同一个寄存器）算出「指向以 nvn 开头的
NUL 结尾 ASCII 串」的位置，记 (名字, 装着名字的地址, 装载点)。这就是「游戏要了哪些接口、
在哪里要的」的原始证据。

用法：
  nvn_getproc.py [--mod main] [--stats] [--name <精确名或前缀>] [--tsv 输出]
默认打印统计与每个名字的装载点个数；--tsv 写出 `名字 TAB 装载点 TAB 字符串地址`。
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))          # smo/gfx/tools
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))  # 仓库根
WORK = os.path.join(ROOT, "work", "game")


def sx(v, bits):
    return v - (1 << bits) if v >> (bits - 1) else v


def find_name_loads(img, text_end):
    """返回 [(装载点, 字符串地址, 字符串)]；装载点 = adrp 的地址。"""
    n = text_end // 4
    w = struct.unpack_from(f"<{n}I", img, 0)
    out = []
    for i in range(n):
        a0 = w[i]
        if (a0 & 0x9F000000) != 0x90000000:            # adrp
            continue
        rd = a0 & 0x1F
        page = ((i * 4) & ~0xFFF) + (sx(((a0 >> 5) & 0x7FFFF) << 2 | ((a0 >> 29) & 3), 21) << 12)
        # 往后最多 6 条指令找 add Rd, Rd, #imm（LSL 0 或 12）
        for j in range(i + 1, min(i + 7, n)):
            a1 = w[j]
            if (a1 & 0x7F800000) != 0x11000000:        # add (imm), 64 位
                continue
            if (a1 & 0x1F) != rd or ((a1 >> 5) & 0x1F) != rd:
                continue
            imm = ((a1 >> 10) & 0xFFF) << (12 if (a1 >> 22) & 1 else 0)
            va = page + imm
            if va + 4 >= len(img):
                break
            if img[va:va + 3] != b"nvn":
                break
            e = img.find(b"\x00", va, va + 96)
            if e < 0:
                break
            s = img[va:e]
            if all(32 <= c < 127 for c in s):
                out.append((i * 4, va, s.decode()))
            break
    return out


def main():
    mod = "main"
    if "--mod" in sys.argv:
        mod = sys.argv[sys.argv.index("--mod") + 1]
    img = open(os.path.join(WORK, mod + ".bin"), "rb").read()
    nso = open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20)
    text_end = struct.unpack_from("<I", nso, 0x18)[0]

    loads = find_name_loads(img, text_end)
    by_name = collections.defaultdict(list)
    for site, va, s in loads:
        by_name[s].append((site, va))

    want = None
    if "--name" in sys.argv:
        want = sys.argv[sys.argv.index("--name") + 1]
    if want:
        for nm in sorted(by_name):
            if nm == want or nm.startswith(want):
                sites = ", ".join(f"0x{s:x}" for s, _ in by_name[nm])
                print(f"{nm}  x{len(by_name[nm])}  @ {sites[:160]}")
        return 0

    if "--tsv" in sys.argv:
        out = sys.argv[sys.argv.index("--tsv") + 1]
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("# 名字\t装载点\t字符串地址\n")
            for site, va, s in sorted(loads, key=lambda t: (t[2], t[0])):
                f.write(f"{s}\t0x{site:x}\t0x{va:x}\n")
        print(f"写出 {out}：{len(loads)} 条")
        return 0

    print(f"# {mod}: 找到 {len(loads)} 处「装名字字符串」，涉及 {len(by_name)} 个不同名字")
    multi = {k: v for k, v in by_name.items() if len(v) > 1}
    print(f"# 被多处装载的名字：{len(multi)}")
    hist = collections.Counter(len(v) for v in by_name.values())
    for k in sorted(hist):
        print(f"  装载 {k} 次的名字：{hist[k]} 个")
    print("# 装载点最多的 10 个：")
    for nm, v in sorted(by_name.items(), key=lambda t: -len(t[1]))[:10]:
        print(f"  x{len(v):<3} {nm}")
    print("# 只装载一次的前 10 个：")
    once = [nm for nm, v in by_name.items() if len(v) == 1]
    for nm in sorted(once)[:10]:
        print(f"       {nm}  @ 0x{by_name[nm][0][0]:x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
