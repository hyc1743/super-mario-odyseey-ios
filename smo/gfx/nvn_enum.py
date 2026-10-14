#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 `smo/gfx/nvn/include/nvn.h` 抽出枚举值 → `smo/gfx/nvn/enums.tsv`（C 能直接查的表）。

为什么要这一步：那份头是 **C++ 风格的重建版**（h1k421，MIT，见 include/README.md），
宿主的图形层是 C，直接 include 它不合适；而 Metal 翻译真正需要的只是**枚举的数值**
（格式、混合因子、比较函数、过滤/环绕、阶段…）。所以把头里的枚举抽成一张表，
用脚本生成 C 数组，值本身仍以那份头为准（改了头就重跑，`--check` 会报不一致）。

用法：nvn_enum.py [--check]
  → smo/gfx/nvn/enums.tsv    `枚举名\t值\t所属类型`（按名字排序，便于 diff）
  → smo/gfx/nvn/enums.h      宏定义（`#define NVNENUM_<类型>_<名字> <值>`），供 C 侧使用
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))       # smo/gfx
ROOT = os.path.dirname(os.path.dirname(HERE))
HDR = os.path.join(HERE, "nvn", "include", "nvn.h")
TSV = os.path.join(HERE, "nvn", "enums.tsv")
OUT = os.path.join(HERE, "nvn", "enums.h")


def parse(hdr_text):
    """扫 `typedef enum <类型> { ... } <名字>;` 里的 `NAME = 值,`（只收有显式值的）。"""
    out = []
    for m in re.finditer(r"enum\s+(\w+)?\s*\{(.*?)\}\s*(\w+)?\s*;", hdr_text, re.S):
        body, tag = m.group(2), (m.group(3) or m.group(1) or "?")
        for item in body.split(","):
            mm = re.match(r"\s*(NVN\w+)\s*=\s*(-?(?:0[xX][0-9a-fA-F]+|\d+))\s*$", item)
            if mm:
                out.append((mm.group(1), int(mm.group(2), 0), tag))
    return sorted(set(out))


def render(rows):
    tsv = ["# 枚举名\t值\t所属类型", "# 由 smo/gfx/nvn_enum.py 从 include/nvn.h（h1k421, MIT）抽出"]
    tsv += [f"{n}\t{v}\t{t}" for n, v, t in rows]
    h = ["// 【自动生成，不要手改】由 smo/gfx/nvn_enum.py 从 include/nvn.h（h1k421, MIT）抽出。",
         "// 只含枚举的数值：Metal 翻译（格式/混合/比较/过滤/环绕/阶段…）要用它们。",
         "#pragma once"]
    for n, v, t in rows:
        h.append(f"#define NVNENUM_{t}_{n[3:]} {v}")
    h.append(f"// 共 {len(rows)} 条")
    return "\n".join(tsv) + "\n", "\n".join(h) + "\n"


def main():
    if not os.path.exists(HDR):
        print(f"🔴 缺 {HDR}（应为 h1k421 的 MIT 重建版 nvn.h）")
        return 2
    rows = parse(open(HDR, encoding="utf-8", errors="replace").read())
    if not rows:
        print("🔴 没抽到任何带显式值的枚举")
        return 2
    tsv_text, h_text = render(rows)
    if "--check" in sys.argv:
        ok = True
        for path, text in ((TSV, tsv_text), (OUT, h_text)):
            cur = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
            if cur != text:
                print(f"🔴 {path} 与 nvn.h 不一致：重跑 nvn_enum.py")
                ok = False
        if ok:
            print(f"✓ {TSV} 与 {OUT} 都与 nvn.h 一致（{len(rows)} 条）")
        return 0 if ok else 1
    open(TSV, "w", encoding="utf-8", newline="\n").write(tsv_text)
    open(OUT, "w", encoding="utf-8", newline="\n").write(h_text)
    import collections
    c = collections.Counter(t for _, _, t in rows)
    print(f"已抽出 {len(rows)} 条枚举 → {TSV} 与 {OUT}")
    for t, n in c.most_common(8):
        print(f"  {t}: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
