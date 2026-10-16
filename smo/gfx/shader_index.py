#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build a private runtime lookup index from offline SPH+code artifacts.

The first three TSV columns preserve the legacy exact-blob lookup format:
  full_hash_hex, stage, msl_filename
V2 appends:
  full_size_decimal, full_prefix_hash_hex, code_hash_hex, code_prefix_hash_hex
Prefix hashes cover min(64, span). Runtime pool lookup validates the exact
known span after a prefix match; it never derives sizes from unknown control
pointers. Candidate offsets/layouts still require actual runtime validation.
The hash uses the established repository FNV-like offset basis for compatibility.
"""
import os
import sys
import json

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


def build(root: str, spans=None):
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
        rows.append((fnv64(data), stage, key + ".metal", len(data),
                     fnv64(data[:64]), fnv64(data[0x50:]), fnv64(data[0x50:0x90])))
        if spans:
            with open(os.path.join(spans, key + ".span.json"), encoding="utf-8") as stream:
                metadata = json.load(stream)
            size = metadata["span_bytes"]
            if metadata["key"] != key or not 0x58 <= size <= len(data) or metadata["archive_bytes"] != len(data):
                raise ValueError("invalid decoded span metadata: " + key)
            if size < len(data):
                exact = data[:size]
                rows.append((fnv64(exact), stage, key + ".metal", size,
                             fnv64(exact[:64]), fnv64(exact[0x50:]), fnv64(exact[0x50:0x90])))
    return rows


def render(rows):
    out = ["# 着色器运行期索引：<FNV-1a 64>\t<stage>\t<MSL 文件名>",
           "# 由 smo/gfx/shader_index.py 从 work/shader_unpack/*/code_full.bin 生成",
           "# stage：0=cs 1=vs 2=ts 3=te 4=gs 5=ps（与 Ryujinx ShaderStage 同号）"]
    for k, stage, msl, size, prefix, code_key, code_prefix in sorted(rows):
        out.append(f"{k:016x}\t{stage}\t{msl}\t{size}\t{prefix:016x}\t{code_key:016x}\t{code_prefix:016x}")
    out.append("# v2 extra columns: full_size_dec full_prefix64_hex code_key_hex code_prefix64_hex; prefix hashes cover min(64, span)")
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
    spans = sys.argv[sys.argv.index("--spans") + 1] if "--spans" in sys.argv else None
    rows = build(unpack, spans)
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
        c = collections.Counter(row[1] for row in rows)
        names = {0: "cs", 1: "vs", 2: "ts", 3: "te", 4: "gs", 5: "ps"}
        for st, n in sorted(c.items()):
            print(f"  {names.get(st, st)}: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
