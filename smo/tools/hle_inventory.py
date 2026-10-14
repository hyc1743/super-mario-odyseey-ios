#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HLE 工作清单：SMO 的每个 import 该由谁提供 —— 这就是宿主真正的工作量清单。

参考项目 `dm/tools/hle_inventory.py` 是从**它自己的源码**里抽出 HLE 条目（事后审计）。
本工具做的是**反过来**的事：从事先的游戏文件里算出**必须实现什么**（事前规划）。

判定分三步：
  1. **别的模块导出** → 跨模块重定位，宿主不用管（例如 main 用的 nn::os 由 sdk 提供）
  2. **服务层 nn::** → 宿主强制接管（HLE_FORCE）。理由见下：SDK 的客户端实现要走 IPC，
     而宿主里没有服务进程；接管在 API 层实现比实现 IPC + 服务进程现实得多
  3. **本地库 / libc / C++ 运行时** → 原样跑或走宿主 libc
另外把 `sdk` 里的 svc 与 cntpct 数量列出来 —— 那是内核层要替换的量。

用法：
  hle_inventory.py <ExeFS 目录> [输出 tsv]        # 默认写 smo/data/hle_imports.tsv
  hle_inventory.py <ExeFS 目录> --summary         # 只打印汇总
"""
import collections
import re
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso_info

# ---- 策略表：哪些 nn:: 家族是「服务客户端」（宿主必须接管），哪些可以原样跑 ----
# 依据：这些家族在 Switch 上都是「客户端库 + 系统进程」的形态；宿主里没有那些进程，
# 所以在其 API 层接管。本地库（os/util/diag/mem）是纯计算 + 内核调用，可以原样跑。
SERVICE_FAMILIES = {
    # 文件 / 资源
    "fs", "aoc", "savedata", "content", "ns", "romfs",
    # 输入 / 输出设备
    "hid", "irs", "nfc", "nfp", "mii", "am", "vi", "disp", "lbl", "grcd",
    # 运行环境 / 账号 / 设置
    "oe", "account", "settings", "pctl", "npns", "friends", "prepo", "bcat", "news", "notification",
    # 音频 / 图像 / 字体 / 解码
    "audio", "audren", "codec", "image", "font", "fontll", "web", "nvjpg", "capsrv",
    # 网络
    "socket", "ssl", "nifm", "ldn", "bsd", "http", "dns",
    # 安全 / 系统信息
    "spl", "crypto", "spsm", "tc", "pm", "fatal", "erpt",
    # 其它服务型
    "time", "tcap", "pdm", "pcm", "htcs", "pl", "lmem", "pcv", "clkrst", "i2c", "pcie",
    "usb", "sdmmc", "smm", "uart", "gpio", "pinmux", "psc", "pwm", "i2c", "spi", "ts",
    "nvnflinger", "nvdrv", "nvmap", "nvhost", "nvgfx", "gpu", "pwm", "avm",
}
# 这些是**本地库**（纯计算，跑在内核调用之上）：可以原样执行
LOCAL_LIBRARIES = {"os", "util", "diag", "mem", "search", "nnex", "libc", "rtld", "gfx", "ro", "profiler"}


def family_of(sym):
    """抽「顶层命名空间」：改编名第一个嵌套名组件（`_ZN2nn…` → nn、`_ZN2nv…` → nv、
    `_ZN7android…` → android、`_ZNSt3__1…` → std）。

    这里不能用 `startswith("_ZN2nn")` —— 常量成员函数是 `_ZNK2nn…`、虚表是 `_ZTVN2nn…`，
    那样会漏掉一大批（实测漏了 774 条，其中包括 nv(NVN) 与 android(媒体框架)）。
    """
    i = sym.find("2nn")
    if i > 0:
        p = j = i + 3
        while j < len(sym) and sym[j].isdigit():
            j += 1
        if j > p:
            return sym[j:j + int(sym[p:j])]
    # 非 nn：解析 `_ZN<len><name>` / `_ZNK<len><name>` / `_ZTV<len><name>` / `_ZTv0_…`
    if "St3__" in sym[:28] or sym.startswith("_ZSt"):
        return "std(libc++)"            # `St` 是 ABI 里 `std::` 的替代码（_ZNKSt3__1 / _ZTINSt3__1 都要认）
    if "7android" in sym[:20] or sym.startswith("android_"):
        return "android"
    if sym.startswith(("NvRm", "NvOs", "NvError")):
        return "nvidia-driver"
    if sym.startswith("_ZTv") or sym.startswith("_ZTh"):
        return "thunk"                   # 虚/非虚调用 thunk，模块内粘合代码
    m = re.match(r"_Z(?:TV)?N?[KVR]*?[0-9]", sym)
    if not m:
        return None
    k = m.end() - 1
    j = k
    while j < len(sym) and sym[j].isdigit():
        j += 1
    if j == k:
        return None
    n = int(sym[k:j])
    name = sym[j:j + n]
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        return None
    return {"St": "std(libc++)"}.get(name, name)


def load_modules(exefs):
    mods = {}
    for name in ("rtld", "sdk", "subsdk0", "main"):
        p = os.path.join(exefs, name)
        if not os.path.exists(p):
            continue
        img, info = nso_info.load_nso(p)
        nso_info._IMG = img
        dyn, _ = nso_info.parse_dynamic(img)
        syms = nso_info.symbols(img, dyn)
        mods[name] = {
            "info": info,
            "imports": {n for n, v, s, z in syms if s == 0 and n},
            "exports": {n for n, v, s, z in syms if s != 0 and n},
        }
    return mods


def verdict(sym, provided_by):
    fam = family_of(sym)
    # 非 nn 命名空间：这几个是实测查出来的，列出来因为它们不是「服务客户端」那套
    if fam == "nv":
        return "图形(NVN)，宿主接管", fam           # nv::InitializeGraphics / SetGraphicsAllocator
    if fam in ("android", "movie"):
        return "媒体框架(模块内，原样跑)", fam      # SMO 的过场视频走的是 Android 媒体框架
    if fam == "std(libc++)":
        return "C++ 标准库(模块内)", fam
    # C 风格的 C 库 / 运行时 / SDK C API：不是「服务客户端」。不先拦这批的话，
    # 它们会因为「被别的模块导出且不在本地库名单里」而被错归到服务客户端（实测错 275 条）。
    if (not sym.startswith("_Z")) or sym.startswith(("__cxa", "__errno", "__nnmusl", "__nnDetail")):
        if sym.startswith(("nnsocket",)):
            return "网络(宿主:离线桩或BSD socket)", "socket"
        if sym.startswith(("nnssl",)):
            return "网络(宿主:离线桩)", "ssl"
        return "C 库/运行时(模块内或宿主 libc)", fam
    if fam in ("thunk", "nvidia-driver"):
        return ("模块内(粘合/驱动层，通常被更上层绕过)", fam)
    if fam == "android":
        return "媒体框架(模块内，原样跑)", fam
    if sym.startswith("nnsocket"):
        return "网络(宿主:离线桩或BSD socket)", "socket"
    if sym.startswith("nnssl"):
        return "网络(宿主:离线桩)", "ssl"
    if provided_by:
        return ("模块内" if fam in LOCAL_LIBRARIES else "模块内(服务客户端，仍需接管)"), fam
    if fam in SERVICE_FAMILIES:
        return "宿主接管(HLE_FORCE)", fam
    if fam in LOCAL_LIBRARIES:
        return "宿主HLE(库)", fam
    if sym.startswith("_Z") or sym.startswith("__cxa") or sym.startswith("_ZTV") or sym.startswith("_ZSt"):
        return "C++/运行时(模块内或宿主libc++)", fam
    if sym.startswith("__rel") or sym.startswith("__got") or sym.startswith("__EX") or sym.startswith("__tbss") or sym.startswith("__tdata"):
        return "装载器符号(宿主提供)", fam
    if sym in ("memcpy", "memset", "memmove", "strlen", "strcmp", "strncpy", "sqrtf", "cosf", "sinf", "acosf",
               "asinf", "atan2f", "tanf", "powf", "expf", "logf", "atanf", "floorf", "ceilf", "fmodf"):
        return "libc(宿主直通)", fam
    if sym.startswith("glslc") or sym.startswith("nnos") or sym.startswith("__nnDetail"):
        return "SDK 内部/死引用", fam
    return "未分类", fam


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    exefs = sys.argv[1]
    summary_only = "--summary" in sys.argv
    out = next((a for a in sys.argv[2:] if not a.startswith("--")), None) or \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "hle_imports.tsv")
    mods = load_modules(exefs)
    rows = []
    for m in ("main", "subsdk0", "sdk", "rtld"):
        if m not in mods:
            continue
        for sym in sorted(mods[m]["imports"]):
            prov = [o for o in ("main", "subsdk0", "sdk", "rtld")
                    if o != m and o in mods and sym in mods[o]["exports"]]
            v, fam = verdict(sym, prov)
            rows.append((m, sym, fam or "-", ",".join(prov) or "-", v))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    if not summary_only:
        with open(out, "w", encoding="utf-8") as f:
            f.write("# 模块\timport\t家族\t由谁导出\t宿主要做什么\n")
            for r in rows:
                f.write("\t".join(r) + "\n")
        print(f"已写出 {out}（{len(rows)} 条）")

    print("\n== 按「宿主要做什么」统计 ==")
    for k, v in collections.Counter(r[4] for r in rows).most_common():
        print(f"  {v:6d}  {k}")
    print("\n== 需要宿主接管的服务家族（按条目数） ==")
    # 注意：服务客户端**即使被 sdk/subsdk0 导出，也要接管**（SDK 的实现要走 IPC，
    # 而宿主里没有服务进程），所以这里把「模块内(服务客户端，仍需接管)」也算进来 ——
    # 这才是宿主真正的工作清单。
    svc = collections.Counter(r[2] for r in rows
                              if r[4] in ("宿主接管(HLE_FORCE)", "宿主HLE(库)",
                                          "模块内(服务客户端，仍需接管)"))
    for k, v in svc.most_common(40):
        print(f"  {v:6d}  nn::{k}")
    print(f"\n  服务家族合计 {sum(svc.values())} 条，共 {len(svc)} 个家族")
    print("\n== 内核层（改写器在各模块里数出来的） ==")
    for m in ("sdk", "subsdk0", "main", "rtld"):
        p = os.path.join(os.path.dirname(os.path.abspath(out)), "..", "..", "work", "rewrite", f"{m}.gates.tsv")
        if os.path.exists(p):
            kinds = collections.Counter(l.split("\t")[1] for l in open(p, encoding="utf-8") if l.strip())
            print(f"  {m:8s} 宿主门 {dict(kinds)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
