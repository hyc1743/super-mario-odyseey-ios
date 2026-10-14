#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""探测 .sharcb 里着色器 blob 的指令起始偏移 —— 用**翻译器能不能把它译成 SPIR-V** 当判据。

背景：
  `.sharcb`（magic "BAHS"）里，每个着色器 blob 以 `0x12345678` 标记开头（实测 0x6000 / 0x6300 / 0x6800…）。
  参考项目的 Ryujinx 翻译器约定：它从代码起始跳过 0x50 字节的 SPH，再开始解指令（Translator.cs HeaderSize = 0x50）。
  所以只要知道 blob 里指令的真实起点，就能造出「0x50 字节占位 + 指令」喂给它。

做法：对每个候选起点造一个作业，跑 dmsh_xlate，看是否译出 SPIR-V（不抛异常且 .spv 非空）。
     哪个偏移能译出来，哪个就是指令起点 —— 这是判据，不是猜。

用法：probe_sharcb.py <sharcb 文件> [--blob 序号] [--stage vs|ps|cs]
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

MARKER = 0x12345678
XLATE = os.environ.get("XLATE", "C:/toolchain/dmsh_xlate/dmsh_xlate.dll")
DOTNET = os.environ.get("DOTNET", "C:/toolchain/dotnet10/dotnet.exe")


def blobs(d):
    """标记 0x12345678 的位置即 blob 起点；终点是下一个标记或文件里该段数据的末尾。"""
    pos = [i for i in range(0, len(d) - 4, 4) if struct.unpack_from("<I", d, i)[0] == MARKER]
    out = []
    for k, p in enumerate(pos):
        # 不裁尾部零：blob 是等距槽位（0x12345678 到下一个 0x12345678），裁掉会把指令截短
        end = pos[k + 1] if k + 1 < len(pos) else len(d)
        out.append((p, end))
    return out


def run_job(workdir, code_path, stage, key):
    job = [{"key": key, "stage": stage, "code": code_path, "debug": False,
            "tex": {}, "img": {}, "attr": {}, "fout": {}, "local": [1, 1, 1]}]
    jp = os.path.join(workdir, "job.json")
    json.dump(job, open(jp, "w"))
    r = subprocess.run([DOTNET, XLATE, jp, workdir], capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=120)
    return r


def reflect(workdir, key):
    """读回翻译器的反射结果。判据用：译出来的东西像不像真着色器。

    「SPIR-V 非空」不行 —— 垃圾输入也能译出东西（平移错位只是换一种垃圾）。
    真着色器应该：用到贴图 / 常量缓冲 / 有片元输出，而且与 .sharc 里的变量名对得上。
    """
    p = os.path.join(workdir, key + ".res.json")
    if not os.path.exists(p):
        return None
    j = json.load(open(p, encoding="utf-8"))
    return dict(tex=len(j.get("textures", [])), img=len(j.get("images", [])),
                cbuf=len(j.get("cbufs", [])), sbuf=len(j.get("sbufs", [])),
                fout=j.get("frag_out_map"), fc=j.get("uses_frag_coord"),
                notes=j.get("notes", []))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    d = open(sys.argv[1], "rb").read()
    bs = blobs(d)
    print(f"{os.path.basename(sys.argv[1])}: {len(d)} 字节，找到 {len(bs)} 个 blob")
    for k, (p, e) in enumerate(bs):
        print(f"  blob{k}: 0x{p:x}..0x{e:x}  ({e - p} 字节)")
    which = int(sys.argv[sys.argv.index("--blob") + 1]) if "--blob" in sys.argv else None
    stage = sys.argv[sys.argv.index("--stage") + 1] if "--stage" in sys.argv else "ps"
    todo = [which] if which is not None else list(range(len(bs)))
    with tempfile.TemporaryDirectory() as td:
        print(f"\n=== 探测指令起点，stage={stage}；判据 = 译出的反射要像个真着色器 ===")
        for k in todo:
            p, e = bs[k]
            blob = d[p:e]
            print(f"-- blob{k} 0x{p:x}（{len(blob)} 字节）")
            for delta in range(0, min(len(blob) - 0x50, 0xC0), 4):
                code = bytes(0x50) + blob[delta:len(blob) - (len(blob) - delta) % 8]
                cp = os.path.join(td, "code.bin")
                open(cp, "wb").write(code)
                r = run_job(td, cp, stage, "probe")
                spv = os.path.join(td, "probe.spv")
                size = os.path.getsize(spv) if os.path.exists(spv) else 0
                if os.path.exists(spv):
                    os.unlink(spv)
                re_ = reflect(td, "probe")
                if os.path.exists(os.path.join(td, "probe.res.json")):
                    os.unlink(os.path.join(td, "probe.res.json"))
                if re_ is None:
                    print(f"   +0x{delta:02x}: 翻译失败 {(r.stderr or '').strip()[-80:]}")
                    continue
                mark = "✓" if (re_["tex"] or re_["sbuf"] or re_["cbuf"]) and re_["fout"] is not None else " "
                print(f"   {mark} +0x{delta:02x}: spv {size:5d}  贴图 {re_['tex']} 图像 {re_['img']} "
                      f"cBuf {re_['cbuf']} sBuf {re_['sbuf']} 片元输出 {re_['fout']} 用FragCoord {re_['fc']}"
                      + (f"  注: {re_['notes'][:1]}" if re_["notes"] else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
