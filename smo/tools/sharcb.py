#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 agl 着色器归档（`.sharcb`）里取出每个着色器，生成翻译作业。

格式（实测 + 逐条验证，见 STATUS.md 第三轮）：
    `.szs`（Yaz0）→ SARC → `X.sharc`（定义，magic "AAHS"）+ `X.sharcb`（二进制，magic "BAHS"）
    `.sharcb` 里每个着色器一个槽位：
        +0x00  u32 `0x12345678` 槽位标记
        +0x04  0x2C 字节（实测全零）
        +0x30  **SPH 0x50 字节**（标准 Maxwell 着色器程序头：word0 的 bit[10:13] 是 stage）
        +0x80  Maxwell 机器码，直到下一个标记
    stage 的取值沿用 Ryujinx 的 `ShaderStage` 枚举：Compute=0, **Vertex=1**, TessCtrl=2, TessEval=3,
    Geometry=4, **Fragment=5**（这个枚举从 0 起的假设错过一次，见 STATUS.md 里的记录）。
    翻译器（dmsh_xlate）要的输入就是「SPH + 指令」，**原样喂给它即可**，不需要合成 SPH、也不需要 c1。

输出（`<输出目录>`）：
    `<键>/sph.bin`、`<键>/code.bin`   —— 单个着色器的 SPH 与机器码
    `jobs.json`                        —— 交给 dmsh_xlate 的作业（键 = SPH+机器码 的 sha256 前 16 位）
    `index.tsv`                        —— 键、来源归档、槽位偏移、stage、SPH sha256、机器码 sha256、字节数
键的口径：宿主运行时在 `nvnProgramSetShaders` 拿到的就是「SPH + 机器码」，所以键按
sha256(SPH + 机器码) 取前 16 位；同时把「只按机器码」的 sha256 也记在 index.tsv 里，
宿主若发现游戏传进来的只有机器码，用那一列也能对上。

用法：
  sharcb.py <sharcb 文件或目录> <输出目录> [--stage-filter vs|ps]
"""
import hashlib
import json
import os
import struct
import sys

MARKER = 0x12345678
SPH_OFF, CODE_OFF = 0x30, 0x80
STAGE_NAME = {0: "cs", 1: "vs", 2: "ts", 3: "te", 4: "gs", 5: "ps"}


def blobs(d):
    """按槽位标记切分；返回 [(偏移, 结束)]。"""
    pos = [i for i in range(0, len(d) - 4, 4) if struct.unpack_from("<I", d, i)[0] == MARKER]
    return [(p, pos[k + 1] if k + 1 < len(pos) else len(d)) for k, p in enumerate(pos)]


def sph_stage(sph):
    w0 = struct.unpack_from("<I", sph, 0)[0]
    return (w0 >> 10) & 0xF


# 图像分量类型 → dmsh_xlate 要的 TextureFormat 名。**像素格式本身不参与着色器翻译**
# （MSL 里是 texture2d<float/uint/int>，格式属于贴图对象、运行时由宿主给出），
# 所以这里只需把分量类型选对。
IMG_FMT = {"float": "R8G8B8A8Unorm", "uint": "R32Uint", "sint": "R32Int"}


def load_facts(path):
    return json.load(open(path, encoding="utf-8")) if path else {}


def facts_for(facts, sharcb_path):
    """按归档名（去掉扩展名）把 facts 配上：`X.sharcb` ↔ `X.sharc` / `X.bfsha`。"""
    stem = os.path.basename(sharcb_path)
    for ext in (".sharcb", ".sharc", ".bfsha"):
        if stem.endswith(ext):
            stem = stem[:-len(ext)]
            break
    for key in (stem + ".sharc", stem + ".bfsha", stem):
        if key in facts:
            return facts[key]
    return {}


def extract(path, outdir, stage_filter=None, facts=None):
    d = open(path, "rb").read()
    # 两种容器都收：`BAHS`（.sharcb，配套 .sharc 定义）与 `FSHA`（.bfsha，单文件）
    # 它们的**着色器槽位布局完全一样**（+0x00 标记 0x12345678 / +0x30 SPH 0x50 / +0x80 机器码），
    # 所以同一套切分即可；差别只在 .bfsha 没有 GLSL 定义（facts 拿不到，走缺省值）。
    assert d[:4] in (b"BAHS", b"FSHA"), f"{path}: 既不是 .sharcb 也不是 .bfsha（magic {d[:4]!r}）"
    jobs, rows = [], []
    for k, (p, e) in enumerate(blobs(d)):
        if e - p < CODE_OFF + 8:
            continue
        sph = d[p + SPH_OFF:p + SPH_OFF + 0x50]
        code = d[p + CODE_OFF:e]
        code = code[:len(code) - len(code) % 8]                 # 指令按 8 字节对齐
        stage = STAGE_NAME.get(sph_stage(sph))
        if stage is None:
            continue
        if stage_filter and stage != stage_filter:
            continue
        key = hashlib.sha256(sph + code).hexdigest()[:16]
        sub = os.path.join(outdir, key)
        os.makedirs(sub, exist_ok=True)
        open(os.path.join(sub, "sph.bin"), "wb").write(sph)
        open(os.path.join(sub, "code.bin"), "wb").write(code)
        # 把从 GLSL 推出来的 facts 按 stage 配上（tex 是整段的，attr/fout/local 按 stage 取）
        f = facts_for(facts or {}, path)
        sf = next((q for q in f.get("programs", []) if q["stage"] == stage), {})
        jobs.append({"key": key, "stage": stage, "code": os.path.join(sub, "code_full.bin"),
                     "debug": False,
                     "tex": f.get("tex", {}),
                     "img": {str(i): {"type": v["type"], "fmt": IMG_FMT.get(v.get("comp"), "R8G8B8A8Unorm")}
                             for i, v in enumerate(f.get("img", []))},
                     "attr": sf.get("attr", {}), "fout": sf.get("fout", {}),
                     "local": sf.get("local") or [1, 1, 1]})
        open(os.path.join(sub, "code_full.bin"), "wb").write(sph + code)
        rows.append((key, os.path.basename(path), f"0x{p:x}", stage, len(code),
                     hashlib.sha256(sph).hexdigest()[:16], hashlib.sha256(code).hexdigest()[:16]))
    return jobs, rows


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    src, outdir = sys.argv[1], sys.argv[2]
    sf = sys.argv[sys.argv.index("--stage-filter") + 1] if "--stage-filter" in sys.argv else None
    if os.path.isdir(src):      # 递归找：<输出>/<归档名>/X.sharcb
        files = [os.path.join(dp, f) for dp, _, fs in os.walk(src) for f in sorted(fs)
                 if f.endswith((".sharcb", ".bfsha"))]
    else:
        files = [src]
    facts = load_facts(sys.argv[sys.argv.index("--facts") + 1]) if "--facts" in sys.argv else {}
    all_jobs, all_rows = [], []
    for f in files:
        j, r = extract(f, outdir, sf, facts)
        all_jobs += j
        all_rows += r
    os.makedirs(outdir, exist_ok=True)
    json.dump(all_jobs, open(os.path.join(outdir, "jobs.json"), "w"), indent=1)
    with open(os.path.join(outdir, "index.tsv"), "w", encoding="utf-8") as f:
        f.write("# 键\t来源\t槽位\tstage\t机器码字节\tSPH_sha256_16\t机器码_sha256_16\n")
        for r in all_rows:
            f.write("\t".join(str(x) for x in r) + "\n")
    import collections
    c = collections.Counter(r[3] for r in all_rows)
    print(f"从 {len(files)} 个 .sharcb 取出 {len(all_rows)} 个着色器 {dict(c)} → {outdir}")
    print(f"作业文件 {os.path.join(outdir, 'jobs.json')}")


if __name__ == "__main__":
    sys.exit(main())
