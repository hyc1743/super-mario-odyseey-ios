#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把离线译好的着色器编成运行时索引：`blob(FNV-1a 64) → stage + MSL 文件名`。

为什么需要这一层：游戏运行时通过 `nvnProgramSetShaders(program, size, control)` 交来控制段，
里面是 Maxwell 着色器 blob（SPH 头 0x50 + 机器码）。宿主必须**按同样的算法**算键，才能查到
第七轮离线译好的那 18470 个着色器。算法就一个：FNV-1a 64 覆盖整个 sph+code。

离线那轮用 sha256 前 16 位当**目录名**（给文件系统用的，见 smo/tools/sharcb.py），
这里另外算一份 64 位运行期用键 —— 两者不冲突：索引里同时记着这两者。

用法：
  shader_index.py [--unpack work/shader_unpack] [--out work/shaders.idx] [--check] [--stats]
产出（文本，便于 diff 与人眼核对）：
  # 注释
  <fnv64 十六进制>\t<stage 数字>\t<sha256 前16位>.metal
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "smo", "tools"))
import sharcb                                                        # noqa: E402

FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def fnv64(data: bytes) -> int:
    h = FNV_OFFSET
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & MASK64
    return h


def build(root: str):
    """递归找 `*/<sha256前16位>/code_full.bin`（离线产物有两种层次：扁平与按归档分目录）。"""
    rows, seen = [], set()
    for dirpath, dirnames, filenames in os.walk(root):
        if "code_full.bin" not in filenames:
            continue
        key = os.path.basename(dirpath)
        if key in seen:
            continue
        data = open(os.path.join(dirpath, "code_full.bin"), "rb").read()
        if len(data) < 0x58:
            continue
        stage = sharcb.sph_stage(data[:0x50])
        if stage is None:
            continue
        seen.add(key)
        rows.append((fnv64(data), stage, key + ".metal"))
    return rows


def render(rows):
    out = ["# 着色器运行期索引：<FNV-1a 64>\t<stage>\t<MSL 文件名>",
           "# 由 smo/gfx/shader_index.py 从 work/shader_unpack/*/code_full.bin 生成",
           "# stage：0=cs 1=vs 2=ts 3=te 4=gs 5=ps（与 Ryujinx ShaderStage 同号）"]
    for k, stage, msl in sorted(rows):
        out.append(f"{k:016x}\t{stage}\t{msl}")
    return "\n".join(out) + "\n"


def main():
    unpack = os.path.join(ROOT, "work", "shaders_all")
    if "--unpack" in sys.argv:
        unpack = sys.argv[sys.argv.index("--unpack") + 1]
    outp = os.path.join(ROOT, "work", "shaders.idx")
    if "--out" in sys.argv:
        outp = sys.argv[sys.argv.index("--out") + 1]
    if not os.path.isdir(unpack):
        print(f"🔴 没有 {unpack}（先跑 smo/tools/sharcb.py 与 dmsh_xlate）")
        return 2
    rows = build(unpack)
    if not rows:
        print(f"🔴 {unpack} 里没有可用的 code_full.bin")
        return 2
    text = render(rows)
    if "--check" in sys.argv:
        cur = open(outp, encoding="utf-8").read() if os.path.exists(outp) else ""
        if cur != text:
            print(f"🔴 {outp} 与离线产物不一致：重跑 shader_index.py")
            return 1
        print(f"✓ {outp} 一致（{len(rows)} 条）")
        return 0
    open(outp, "w", encoding="utf-8", newline="\n").write(text)
    print(f"已生成 {outp}：{len(rows)} 条着色器")
    if "--stats" in sys.argv:
        import collections
        c = collections.Counter(s for _, s, _ in rows)
        names = {0: "cs", 1: "vs", 2: "ts", 3: "te", 4: "gs", 5: "ps"}
        for st, n in sorted(c.items()):
            print(f"  {names.get(st, st)}: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
