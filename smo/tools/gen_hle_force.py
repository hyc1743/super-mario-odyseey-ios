#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 `smo/data/hle_imports.tsv` 生成宿主的强制接管表（`smo/host/hle_force_gen.c`）。

为什么生成而不是手写：服务层要接管的函数是从**游戏自己的 import 清单**里算出来的
（`hle_inventory.py`），共有几百条；手抄必错，而且游戏版本一变就对不上。
生成的文件带一行「自动生成，不要手改」与来源，随时可重跑。

每条的目的（`why` 字段）就是它被接管的**理由**，直接写进表里 —— 与参考项目
「每条必须写接管理由」的规矩一致。

当前阶段：所有条目都接到**通用桩**（打印一行带名字与理由的日志、返回 0），
因为真实现要按家族一个个写（先从启动路径上的 fs / hid / oe 开始）。
桩返回 0 与参考项目对「完全没有实现的导入」的处理一致。

用法：gen_hle_force.py [--check]
  --check：只校验生成结果与仓内文件一致（CI 用；不一致就报错退出）
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TSV = os.path.join(ROOT, "smo", "data", "hle_imports.tsv")
OUT = os.path.join(ROOT, "smo", "host", "hle_force_gen.c")

HEADER = """// 【自动生成，不要手改】由 smo/tools/gen_hle_force.py 从 smo/data/hle_imports.tsv 生成。
//
// 这张表是「宿主必须接管的 import」：服务层 nn:: 的客户端（即使 SDK 里有定义也要覆盖，
// 因为 SDK 的实现要走 IPC 到不存在的服务进程）+ 无人提供的图形入口。
// 每条的理由写在 why 字段里。当前阶段都接到通用桩（打印一次、返回 0）；
// 真实现按家族逐个替换掉对应的桩。
#include "hle.h"

// 通用桩：第一次调用打印一行，返回 0（与参考项目对「没有实现的导入」的处理一致）
static unsigned g_stub_seen[%d];
static uint64_t hle_stub_default(const char *what, const char *why, unsigned i) {
    if (!g_stub_seen[i]++) LOG("HLE 桩被调用：%s —— %s", what, why);
    return 0;
}

"""


# 显式追加：图形层的唯一入口。它不属于任何服务家族，但**必须**接管 ——
# 宿主接管整个 NVN 接口（469 个函数）就靠它（机制见 STATUS.md 2.46/2.51）。
EXPLICIT = {"nvnBootstrapLoader"}

# These are SDK-internal relocations, absent from the cross-module import TSV.
# d2r-mobile takes over service startup at this level rather than connecting to
# Horizon services. Names below were checked against this SMO SDK's exports/GOT.
SDK_STARTUP = {
    "_ZN2nn2os14FlushDataCacheEPKvm": "Host coherent CPU memory; avoid Horizon cache instructions",
    "_ZN2nn2os20FlushEntireDataCacheEv": "Host coherent CPU memory ordering",
    "__clear_cache": "Host instruction cache invalidation",
    "nndiagStartup": "诊断输出由宿主提供，不连接 lm",
    "_ZN2nn3aoc10InitializeEv": "追加内容查询由宿主提供，不连接 aoc",
    "_ZN2nn4pctl10InitializeEv": "宿主不连接家长控制服务",
    "_ZN2nn4diag34InitializeApplicationAbortObserverEv": "崩溃由宿主信号处理器记录",
    "_ZN2nn4diag6detail9AbortImplEPKcS3_S3_i": "原始致命中止由宿主记录，避免客体回溯递归中止",
    "_ZN2nn4diag6detail9AbortImplEPKcS3_S3_iPKNS_6ResultES3_z": "宿主记录致命错误码和原始调用栈",
}


def load_rows():
    rows = []
    with open(TSV, encoding="utf-8") as f:
        next(f)                                   # 表头
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) != 5:
                continue
            mod, sym, fam, prov, what = p
            # 判定口径（与 STATUS.md 2.24/2.29 的结论一致）：
            #   · 服务客户端：即使被 sdk 导出，也必须接管（SDK 的实现要走 IPC）——
            #     它们的判定文本是「模块内(服务客户端，仍需接管)」，不是「宿主接管…」，别筛漏了
            #   · 无人提供的图形入口（nv::*）与网络（socket/ssl）
            if (("服务客户端" in what) or what.startswith(("宿主接管", "宿主HLE", "网络"))
                    or fam == "nv" or sym in EXPLICIT):
                rows.append((mod, sym, fam, prov, what))
    for sym, why in SDK_STARTUP.items():
        rows.append(("sdk", sym, "sdkinit", "sdk", "宿主接管 SDK 初始化：" + why))
    # 去重（同一符号可能被多个模块引用；表里只能有一条）
    seen, uniq = set(), []
    for r in rows:
        if r[1] in seen:
            continue
        seen.add(r[1])
        uniq.append(r)
    return uniq


def generate(rows):
    out = [HEADER.replace("[%d]", f"[{len(rows)}]")]
    for i, (mod, sym, fam, prov, what) in enumerate(rows):
        why = f"{what}；家族 nn::{fam}；{mod} 引用" if fam and fam != "nv" else f"{what}；{mod} 引用"
        out.append(f'static uint64_t hle_s{i}(void);')
    out.append("")
    for i, (mod, sym, fam, prov, what) in enumerate(rows):
        why = f"{what}；家族 nn::{fam}；{mod} 引用" if fam and fam != "nv" else f"{what}；{mod} 引用"
        out.append(f'static uint64_t hle_s{i}(void) {{ return hle_stub_default("{sym}", "{why}", {i}); }}')
    out.append("")
    out.append("const hle_force_ent g_hle_force[] = {")
    for i, (mod, sym, fam, prov, what) in enumerate(rows):
        why = f"{what}；家族 nn::{fam}；{mod} 引用" if fam and fam != "nv" else f"{what}；{mod} 引用"
        out.append(f'    {{"{sym}", (void *)(uintptr_t)hle_s{i}, "{why}"}},')
    out.append("};")
    out.append(f"const unsigned g_hle_force_n = {len(rows)};")
    out.append("")
    return "\n".join(out)


def main():
    rows = load_rows()
    text = generate(rows)
    if "--check" in sys.argv:
        cur = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if cur != text:
            print(f"🔴 {OUT} 与 hle_imports.tsv 不一致：重跑 gen_hle_force.py")
            return 1
        print(f"✓ {OUT} 与清单一致（{len(rows)} 条）")
        return 0
    open(OUT, "w", encoding="utf-8", newline="\n").write(text)
    print(f"已生成 {OUT}（{len(rows)} 条接管）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
