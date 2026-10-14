#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NVN 名字 → 全局槽 → 调用点：把「游戏到底调了哪些 NVN 函数、各多少次」变成表。

机制（实测，见 gfx/README.md）：
  main 静态链入了 NVN 的 C 过程蹦床库。`nvnLoadCProcs(x0, getProc)`（main 里 0x720414，
  12936 字节，导出符号）是一段**展开的序列**：对 466 个名字各做一次
      adrp/add → 名字串   ; ldr → 全局槽指针 ; blr getProc ; str x0, [槽]
  之后游戏所有 NVN 调用都走这些槽：形如
      adrp x8, 0x1e59000 ; ldr x8, [x8, #off] ; ldr x8, [x8] ; blr x8

所以「谁被调用」= 「哪些 GOT 槽被这种 4 条指令模式引用」。本工具做两件事：
  1. 在 nvnLoadCProcs 里把 名字 ↔ GOT 槽 配对
  2. 全 .text 扫该槽的调用点

输出 TSV：名字 TAB GOT槽 TAB 调用点个数 TAB 前几个调用点
用法：nvn_slots.py [--mod main] [--tsv 路径] [--top N]
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
WORK = os.path.join(ROOT, "work", "game")

# nvnLoadCProcs 的范围（导出符号 st_value/st_size，由 nso_info 读出）
LOADCPROCS_VA = 0x720414
LOADCPROCS_SZ = 12936


def sx(v, bits):
    return v - (1 << bits) if v >> (bits - 1) else v


def words(img, text_end):
    n = text_end // 4
    return struct.unpack_from(f"<{n}I", img, 0)


def adrp_page(pc, w):
    return (pc & ~0xFFF) + (sx(((w >> 5) & 0x7FFFF) << 2 | ((w >> 29) & 3), 21) << 12)


def pair_in_loadcprocs(img, w):
    """在 nvnLoadCProcs 里扫出 [(名字, GOT槽地址)]。"""
    lo, hi = LOADCPROCS_VA // 4, (LOADCPROCS_VA + LOADCPROCS_SZ) // 4
    out = []
    for i in range(lo, hi):
        a0 = w[i]
        if (a0 & 0x9F000000) != 0x90000000:                      # adrp
            continue
        rd = a0 & 0x1F
        page = adrp_page(i * 4, a0)
        # ① adrp+add → 名字串
        name = None
        if (w[i + 1] & 0x7F800000) == 0x11000000 and ((w[i + 1] >> 5) & 0x1F) == rd \
           and (w[i + 1] & 0x1F) == rd:
            va = page + (((w[i + 1] >> 10) & 0xFFF) << (12 if (w[i + 1] >> 22) & 1 else 0))
            if va + 3 < len(img) and img[va:va + 3] == b"nvn":
                e = img.find(b"\x00", va, va + 96)
                if e > 0:
                    name = img[va:e].decode(errors="replace")
        if not name:
            continue
        # ② 往后 10 条内找 adrp+ldr → GOT 槽（就是存 x0 的那个）
        for j in range(i + 2, min(i + 12, hi)):
            b0 = w[j]
            if (b0 & 0x9F000000) != 0x90000000:
                continue
            rd2 = b0 & 0x1F
            if (w[j + 1] & 0xFFC00000) != 0xF9400000 or ((w[j + 1] >> 5) & 0x1F) != rd2:
                continue
            slot = adrp_page(j * 4, b0) + (((w[j + 1] >> 10) & 0xFFF) << 3)
            out.append((name, slot, i * 4))
            break
    return out


def call_sites(img, w, slot):
    """找「adrp+ldr 该槽 ; ldr ; blr」的调用点。"""
    lo = 0
    hits = []
    for i in range(lo, len(w) - 4):
        a0 = w[i]
        if (a0 & 0x9F000000) != 0x90000000:
            continue
        rd = a0 & 0x1F
        if (w[i + 1] & 0xFFC00000) != 0xF9400000 or ((w[i + 1] >> 5) & 0x1F) != rd:
            continue
        if adrp_page(i * 4, a0) + (((w[i + 1] >> 10) & 0xFFF) << 3) != slot:
            continue
        # 后面 1..4 条里应有 ldr xM,[xN] 且 blr xM
        for j in (i + 2, i + 3, i + 4, i + 5):
            if j + 1 >= len(w):
                break
            if (w[j] & 0xFFC00000) == 0xF9400000 and (w[j + 1] & 0xFFFFFC1F) == 0xD63F0000 \
               and ((w[j + 1] >> 5) & 0x1F) == (w[j] & 0x1F):
                hits.append(i * 4)
                break
    return hits


def main():
    mod = "main"
    if "--mod" in sys.argv:
        mod = sys.argv[sys.argv.index("--mod") + 1]
    img = open(os.path.join(WORK, mod + ".bin"), "rb").read()
    nso = open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20)
    text_end = struct.unpack_from("<I", nso, 0x18)[0]
    w = words(img, text_end)

    pairs = pair_in_loadcprocs(img, w)
    by_name = collections.defaultdict(list)
    for nm, slot, site in pairs:
        by_name[nm].append((slot, site))
    print(f"# {mod}: nvnLoadCProcs 里配出 {len(pairs)} 对（名字 {len(by_name)} 个）")

    # 每个 GOT 槽（去重）找调用点
    slot_calls = {}
    rows = []
    for nm in sorted(by_name):
        for slot, site in by_name[nm]:
            if slot not in slot_calls:
                slot_calls[slot] = call_sites(img, w, slot)
            rows.append((nm, slot, site, slot_calls[slot]))

    total_calls = sum(len(c) for _, _, _, c in rows)
    called = [r for r in rows if r[3]]
    print(f"# 有调用点的名字：{len(called)}/{len(rows)}；调用点合计 {total_calls}")
    top = sorted(rows, key=lambda r: -len(r[3]))
    n = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 15
    print(f"# 调用点最多的 {n} 个：")
    for nm, slot, site, cs in top[:n]:
        print(f"  {len(cs):<4} {nm:<50} 槽 0x{slot:x}")

    if "--tsv" in sys.argv:
        out = sys.argv[sys.argv.index("--tsv") + 1]
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("# 名字\tGOT槽\t解析点\t调用点数\t前几个调用点\n")
            for nm, slot, site, cs in rows:
                f.write(f"{nm}\t0x{slot:x}\t0x{site:x}\t{len(cs)}\t"
                        + " ".join(f"0x{x:x}" for x in cs[:6]) + "\n")
        print(f"写出 {out}：{len(rows)} 行")
    return 0


if __name__ == "__main__":
    sys.exit(main())
