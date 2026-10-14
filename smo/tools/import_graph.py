#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""模块解析图：每个 import 由哪个模块导出，谁都没有导出 ⇒ 必须由宿主 HLE。

参考项目 host/loader.c 的 resolve_import 有三条路：
  1. 别的模块（sdk / subsdk0 / rtld）导出了 → 跨模块重定位
  2. 宿主 HLE_FORCE 强制接管（服务层 nn::xxx 的客户端）
  3. 宿主 HLE 补上（没有模块定义的）
本文件算出这三类的**确切清单**，也就是这个移植项目真正的工作量清单。

用法：import_graph.py <ExeFS 目录> [输出 tsv]
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso_info


def analyse(d):
    mods = {}
    for name in ("rtld", "sdk", "subsdk0", "main"):
        p = os.path.join(d, name)
        if not os.path.exists(p):
            continue
        img, info = nso_info.load_nso(p)
        nso_info._IMG = img
        dyn, _ = nso_info.parse_dynamic(img)
        syms = nso_info.symbols(img, dyn)
        imports = {n: v for (n, v, s, z) in syms if s == 0 and n}
        exports = {n: v for (n, v, s, z) in syms if s != 0 and n}
        used = nso_info.reloc_targets(img, dyn)
        mods[name] = dict(info=info, imports=imports, exports=exports, nused=len(used), nsyms=len(syms))
        print(f"{name:8s} build_id={info['build_id'][:16]} 符号={len(syms):6d} import={len(imports):5d} "
              f"export={len(exports):6d} 重定位引用={len(used)} 映像=0x{len(img):x}")
    return mods


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    d = sys.argv[1]
    mods = analyse(d)
    order = ["main", "subsdk0", "sdk", "rtld"]
    rows = []
    live = {m: mods[m]["imports"] for m in mods}
    for m in order:
        if m not in mods:
            continue
        for sym in sorted(mods[m]["imports"]):
            prov = [o for o in order if o != m and o in mods and sym in mods[o]["exports"]]
            rows.append((m, sym, ",".join(prov) or "-"))
    print(f"\n== 谁来提供这些 import ==")
    stat = collections.Counter((r[0], "模块内部" if r[2] != "-" else "必须宿主 HLE") for r in rows)
    for k in sorted(stat):
        print(f"  {k[0]:8s} {k[1]:14s} {stat[k]:5d}")
    nobody = [r for r in rows if r[2] == "-"]
    print(f"\n== 无人导出 ⇒ 宿主必须实现/接管：{len(nobody)} 个 ==")
    g = collections.Counter()
    for m, sym, _ in nobody:
        ns = sym
        if sym.startswith("_ZN2nn"):
            i = sym.find("2nn"); p = i + 3; j = p
            while j < len(sym) and sym[j].isdigit():
                j += 1
            ns = "nn::" + sym[j:j + int(sym[p:j] or 0)]
        elif sym.startswith("_ZNSt") or sym.startswith("_ZSt"):
            ns = "libstdc++"
        elif sym.startswith("_Z"):
            ns = "C++ (去符号名)"
        g[ns] += 1
    for k, c in g.most_common():
        print(f"  {c:5d}  {k}")
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w", encoding="utf-8") as f:
            f.write("模块\timport\t由谁导出\n")
            for r in rows:
                f.write("\t".join(r) + "\n")
        print(f"\n已写出 {sys.argv[2]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
