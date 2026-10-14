#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 `.sharc` 里内嵌的 GLSL 源码推出翻译器需要的「facts」。

参考项目的 facts 来自 **PC 版**的反射（同槽号 = 同资源）；SMO 没有 PC 版，但这些事实
其实就写在游戏自己的 GLSL 里 —— 而且是**同源**的（编译成 Maxwell 的就是它）：

| facts | GLSL 里的出处 | 例子 |
|---|---|---|
| 贴图维度类型 | `uniform sampler2D/3D/Cube/2DArray/…` | `samplerCube` → `cube` |
| 图像分量类型 | `uniform image2D / uimage2D / iimage2D` | `uimage2D` → 无符号整型 |
| 顶点属性分量类型 | `layout(location=N) in vec4/ivec4/uvec4` | `ivec4` → sint |
| 片元输出分量类型 | `layout(location=N) out vec4/ivec4/uvec4` | `uvec4` → uint |
| 计算线程组 | `layout(local_size_x = N, …) in` | — |
| 共享内存 | `shared` 声明 | — |

**哪些事实不需要推**：浮点贴图的**像素格式**（rgba8/unorm/…）不参与着色器翻译 ——
MSL 里是 `texture2d<float>`，格式属于贴图对象、由宿主在运行时给出。所以这里不推像素格式。

两个实现要点（都踩过）：
  1. **先展开宏**：binding 写在宏里（`#define BINDING_SAMPLER_SRC_TEX layout(binding = 0)`），
     不展开就一个贴图都读不出来；
  2. **`#if`/`#endif` 要按嵌套配对**：里面还有嵌套的条件块，按「到第一个 #endif 为止」切会把
     声明切掉一半（顶点属性就是这么丢的）。

另外把变体轴（`// @@ id="cSamplingType" choice="0,1,2,3"` 这类注解）也读出来。

用法：
  glsl_facts.py <sharc 文件> [--json 输出.json]
  glsl_facts.py --dir <解包后的目录> [--json 输出.json]
"""
import json
import os
import re
import sys

SAMPLER_DIM = {
    "sampler1D": "1d", "sampler2D": "2d", "sampler3D": "3d", "samplerCube": "cube",
    "sampler1DArray": "1darray", "sampler2DArray": "2darray", "samplerCubeArray": "cubearray",
    "sampler2DMS": "2dms", "sampler2DMSArray": "2dmsarray", "samplerBuffer": "buffer",
    "sampler2DShadow": "2d", "samplerCubeShadow": "cube", "sampler2DArrayShadow": "2darray",
    "sampler1DShadow": "1d",
}
IMAGE_DIM = {"image1D": "1d", "image2D": "2d", "image3D": "3d", "imageCube": "cube",
             "image1DArray": "1darray", "image2DArray": "2darray", "imageCubeArray": "cubearray",
             "image2DMS": "2dms", "image2DMSArray": "2dmsarray", "imageBuffer": "buffer"}
COMPONENT = {"vec": "float", "ivec": "sint", "uvec": "uint", "float": "float",
             "int": "sint", "uint": "uint", "double": "float", "bool": "float"}


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def expand_defines(text, rounds=6):
    """展开对象式宏（`#define NAME 值`）。binding 就是藏在宏里的，不展开读不到贴图。"""
    defs = {}
    out_lines = []
    for line in text.split("\n"):
        m = re.match(r"\s*#\s*define\s+(\w+)[ \t]+(.+?)\s*$", line)
        if m and "(" not in m.group(1):
            defs[m.group(1)] = m.group(2)
            out_lines.append("")                      # 宏定义行本身去掉，避免被当成声明
        else:
            out_lines.append(line)
    out = "\n".join(out_lines)
    for _ in range(rounds):
        before = out
        for k, v in defs.items():
            out = re.sub(r"\b" + re.escape(k) + r"\b", lambda _m, v=v: v, out)
        if out == before:
            break
    return out


def take_branches(text, macro):
    """取 `#if defined( macro )` 对应的整段（按嵌套配对到 #endif）。返回 [片段]。"""
    out = []
    for m in re.finditer(r"#\s*if(?:def)?[^\n]*\b" + macro + r"\b", text):
        i = m.end()
        depth = 1
        for tok in re.finditer(r"#\s*(if|ifdef|ifndef|endif)\b", text[i:]):
            depth += 1 if tok.group(1) != "endif" else -1
            if depth == 0:
                out.append(text[i:i + tok.start()])
                break
    return out


def parse_common(t):
    """与 stage 无关的：贴图、图像。"""
    tex, img, names = {}, [], {}
    for i, m in enumerate(re.finditer(r"\buniform\s+(?:lowp\s+|mediump\s+|highp\s+)?(sampler\w+)\s+(\w+)", t)):
        dim = SAMPLER_DIM.get(m.group(1))
        if dim is None:
            continue
        tex[str(i)] = dim
        names[m.group(2)] = str(i)
    # 有 layout(binding = N) 的按 binding 编号；注意名字可能被前面的出现顺序已经编过号
    for fallback, m in enumerate(re.finditer(
            r"layout\s*\(\s*binding\s*=\s*(\d+)\s*\)\s*uniform\s+(?:lowp\s+|mediump\s+|highp\s+)?(sampler\w+)\s+(\w+)", t)):
        slot, kind, name = m.group(1), m.group(2), m.group(3)
        dim = SAMPLER_DIM.get(kind)
        if dim is None:
            continue
        if name in names:                             # 去掉按出现顺序编的那个号
            tex.pop(names[name], None)
        tex[slot] = dim
        names[name] = slot
    for m in re.finditer(r"\buniform\s+(?:writeonly\s+|readonly\s+|coherent\s+)*([ui]?image\w*)\s+(\w+)", t):
        kind = m.group(1)
        base = kind[1:] if kind[:1] in "ui" else kind
        comp = "uint" if kind.startswith("u") else ("sint" if kind.startswith("i") else "float")
        dim = IMAGE_DIM.get(base)
        if dim:
            img.append({"type": dim, "comp": comp, "name": m.group(2)})
    return tex, img


def parse_io(t):
    """某个 stage 段里的属性/输出。"""
    attr, fout = {}, {}
    for m in re.finditer(r"layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*(?:patch\s+)?in\s+(?:lowp\s+|mediump\s+|highp\s+)?(\w+)\s+\w+", t):
        attr[m.group(1)] = COMPONENT.get(re.sub(r"\d", "", m.group(2)), "float")
    for m in re.finditer(r"layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*out\s+(?:lowp\s+|mediump\s+|highp\s+)?(\w+)\s+\w+", t):
        fout[m.group(1)] = COMPONENT.get(re.sub(r"\d", "", m.group(2)), "float")
    return attr, fout


def variant_axes(text):
    return [{"id": m.group(1), "choices": [c.strip() for c in m.group(2).split(",")],
             "default": m.group(3)}
            for m in re.finditer(r'@@\s*id="(\w+)"\s*choice="([^"]*)"(?:\s*default="([^"]*)")?', text)]


def facts_for_text(raw):
    text = raw.decode("utf-8", "replace") if isinstance(raw, bytes) else raw
    axes = variant_axes(text)
    t = expand_defines(strip_comments(text))
    tex, img = parse_common(t)
    programs = []
    for stage, macro in (("vs", "AGL_VERTEX_SHADER"), ("ps", "AGL_FRAGMENT_SHADER"),
                         ("cs", "AGL_COMPUTE_SHADER"), ("gs", "AGL_GEOMETRY_SHADER")):
        blocks = take_branches(t, macro)
        if not blocks:
            continue
        body = "\n".join(blocks)
        attr, fout = parse_io(body)
        local = None
        m = re.search(r"local_size_x\s*=\s*(\d+)[^)]*local_size_y\s*=\s*(\d+)[^)]*local_size_z\s*=\s*(\d+)", body)
        if m:
            local = [int(m.group(i)) for i in (1, 2, 3)]
        programs.append({"stage": stage, "attr": attr, "fout": fout, "local": local})
    return {"axes": axes, "tex": tex, "img": img, "programs": programs}


def facts_for_file(path):
    d = open(path, "rb").read()
    if d[:4] in (b"AAHS", b"AFHS"):                   # 已经是解出来的 .sharc / .bfsha
        return facts_for_text(d)
    raw = d
    if raw[:4] == b"Yaz0":
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from nintendo_archives import yaz0_decompress
        raw = yaz0_decompress(raw)
    if raw[:4] != b"SARC":        # .bfsha（magic "FSHA"）不是 SARC 容器，本体就是归档数据
        return facts_for_text(raw)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from nintendo_archives import Sarc
    s = Sarc(raw)
    blob = b""
    for name, off, size in s.entries:
        if name.endswith((".sharc", ".bfsha")):
            blob += raw[off:off + size]
    return facts_for_text(blob)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    out = {}
    if src == "--dir" or os.path.isdir(src):
        base = sys.argv[2] if src == "--dir" else src
        for dp, _, fs in os.walk(base):
            for f in sorted(fs):
                if f.endswith((".sharc", ".bfsha")):
                    out[f] = facts_for_file(os.path.join(dp, f))
    else:
        out[os.path.basename(src)] = facts_for_file(src)
    if "--json" in sys.argv:
        jp = sys.argv[sys.argv.index("--json") + 1]
        json.dump(out, open(jp, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        print(f"已写出 {jp}")
    for name, r in out.items():
        print(f"\n=== {name} ===")
        if r["axes"]:
            print("  变体轴:", ", ".join(f"{a['id']}({'/'.join(a['choices'])})" for a in r["axes"]))
        print(f"  贴图 {r['tex']}   图像 {r['img']}")
        for p in r["programs"]:
            print(f"  [{p['stage']}] 属性 {p['attr']}  输出 {p['fout']}  local={p['local']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
