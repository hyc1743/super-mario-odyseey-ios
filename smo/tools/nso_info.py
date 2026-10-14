#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NSO 侦察：解压分段、读 MOD0/动态段、导出 import / export 清单。

Switch 的 NSO 就是「aarch64 的 ELF 映像 + 一个固定头的容器」：代码段与数据段用 LZ4 压缩，
装载时按 memory_offset 放到固定虚拟地址。参考项目 dm/tools/nso.py 做的就是把它摊平成 main.bin。
本文件在此基础上加上动态段解析，用来回答移植的第一问题：**这个游戏 import 了哪些 Horizon 服务。**

用法：
  nso_info.py <nso 文件> [--dump <输出 .bin>] [--imports] [--exports] [--group]
"""
import collections
import struct
import sys
import lz4.block

# NSO 头（0x00..0x70）
#   0x00 magic "NSO0"      0x0C flags（bit i = 第 i 段 LZ4 压缩）
#   0x10/0x20/0x30 三段: 文件偏移, 内存偏移, 解压后大小；每段后紧跟 module_name 偏移/大小、bss 大小
#   0x40 构建号（module_id，0x20 字节）
NSO_HDR = struct.Struct("<4sIII" + "III" * 3 + "II" + "II" + "I")

DT = {0: "NULL", 2: "PLTRELSZ", 4: "HASH", 5: "STRTAB", 6: "SYMTAB", 7: "RELA", 8: "RELASZ",
      9: "RELAENT", 10: "STRSZ", 11: "SYMENT", 20: "PLTREL", 23: "JMPREL"}


def load_nso(path):
    """返回 (图像 bytes, 信息 dict)。图像按 memory_offset 摊平，等价于参考项目的 main.bin。"""
    d = open(path, "rb").read()
    assert d[:4] == b"NSO0", f"不是 NSO0：{d[:4]!r}"
    flags = struct.unpack_from("<I", d, 0xC)[0]
    segs = [struct.unpack_from("<III", d, 0x10 + i * 0x10) for i in range(3)]
    name_off, name_size = struct.unpack_from("<II", d, 0x1C)
    bss = struct.unpack_from("<I", d, 0x3C)[0]
    build_id = d[0x40:0x60]
    fsz = struct.unpack_from("<III", d, 0x60)
    img = bytearray(segs[2][1] + segs[2][2] + bss)
    for i, (fo, mo, sz) in enumerate(segs):
        raw = d[fo:fo + fsz[i]]
        if flags >> i & 1:
            raw = lz4.block.decompress(raw, uncompressed_size=sz)
        assert len(raw) == sz, f"第 {i} 段解压长度不符 {len(raw)} != {sz}"
        img[mo:mo + sz] = raw
    name = d[name_off:name_off + name_size].split(b"\0")[0].decode(errors="replace") if name_size else ""
    info = dict(flags=flags, segs=segs, bss=bss, build_id=build_id.hex(), name=name,
                file_sizes=fsz, text_end=segs[0][2], ro_off=segs[1][1], data_off=segs[2][1])
    return bytes(img), info


def parse_dynamic(img):
    """读 MOD0 → 动态段 → (dyn dict, 符号表 bytes, 字符串表 bytes)。"""
    m = struct.unpack_from("<I", img, 4)[0]
    assert img[m:m + 4] == b"MOD0", "找不到 MOD0"
    dyn_off = m + struct.unpack_from("<i", img, m + 4)[0]
    dyn = {}
    o = dyn_off
    while True:
        tag, val = struct.unpack_from("<qQ", img, o)
        o += 16
        if tag == 0:
            break
        dyn.setdefault(tag, val)
    return dyn, dyn_off


def symbols(img, dyn):
    """返回 [(名字, 值, shndx, 大小)]，索引即符号下标。"""
    symtab, strtab, strsz = dyn[6], dyn[5], dyn.get(10, 0)
    out = []
    for i in range(0, symsz(dyn), 24):
        st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from("<IBBHQQ", img, symtab + i)
        end = img.index(b"\0", strtab + st_name) if st_name else strtab
        nm = img[strtab + st_name:end].decode(errors="replace") if st_name else ""
        out.append((nm, st_value, st_shndx, st_size))
    return out


def symsz(dyn):
    """符号表大小：MOD0 的动态段没有 DT_SYMTAB 大小，用 DT_HASH 的 nchain，退化到 (STRTAB-SYMTAB)。"""
    if 4 in dyn:
        return struct.unpack_from("<I", _IMG, dyn[4] + 4)[0] * 24
    return dyn[5] - dyn[6]


_IMG = b""


def reloc_targets(img, dyn):
    """被实际引用的符号下标集合（JUMP_SLOT 1026 / GLOB_DAT 1025 / ABS64 257）—— 装载器要解析的就是这些。"""
    def scan(off, size):
        idx = set()
        for i in range(0, size, 24):
            r_off, r_info, r_add = struct.unpack_from("<QQq", img, off + i)
            t = r_info & 0xFFFFFFFF
            if t in (257, 1025, 1026):
                idx.add(r_info >> 32)
        return idx
    s = set()
    if 7 in dyn and 8 in dyn:
        s |= scan(dyn[7], dyn[8])
    if 23 in dyn and 2 in dyn:
        s |= scan(dyn[23], dyn[2])
    return s


def main():
    global _IMG
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = sys.argv[1]
    img, info = load_nso(path)
    _IMG = img
    dyn, dyn_off = parse_dynamic(img)
    syms = symbols(img, dyn)
    imports = [(n, v, s, z) for (n, v, s, z) in syms if s == 0 and n]
    exports = [(n, v, s, z) for (n, v, s, z) in syms if s != 0 and n]
    used = reloc_targets(img, dyn)

    print(f"文件      : {path}")
    print(f"模块名    : {info['name']}")
    print(f"构建号    : {info['build_id']}")
    print(f"压缩标志  : {info['flags']:#x}   段: " +
          "  ".join(f"[0x{mo:x}+0x{sz:x} (文件 {fo:x})]" for fo, mo, sz in info['segs']))
    print(f"bss       : 0x{info['bss']:x}   映像总长 0x{len(img):x}   代码段尾 0x{info['text_end']:x}")
    print(f"MOD0@0x{struct.unpack_from('<I', img, 4)[0]:x}  动态段@0x{dyn_off:x}  " +
          "  ".join(f"DT_{DT.get(k, k)}=0x{v:x}" for k, v in sorted(dyn.items())))
    print(f"符号      : 共 {len(syms)}，其中 import {len(imports)}，export {len(exports)}；"
          f"重定位引用的符号 {len(used)}")

    if "--dump" in sys.argv:
        out = sys.argv[sys.argv.index("--dump") + 1]
        open(out, "wb").write(img)
        print(f"已写出摊平映像: {out}")

    if "--imports" in sys.argv or "--group" in sys.argv:
        groups = collections.Counter()
        for n, _v, _s, _z in imports:
            key = n.split("::")[0] if "::" in n else n.split("_")[0]
            if n.startswith("_ZN2nn"):
                parts = n.split("3")  # 粗略：mangled
                key = "nn::?"
            groups[key] += 1
        # 更靠谱的按 nn::X 分组：从 mangled 名里抽
        g2 = collections.Counter()
        for n, _v, _s, _z in imports:
            if n.startswith("_ZN2nn"):
                # _ZN2nn3fs... → 取 "nn::" 后第一段
                i = n.find("2nn")
                seg = ""
                if i >= 0:
                    p = i + 3
                    j = p
                    while j < len(n) and n[j].isdigit():
                        j += 1
                    ln = int(n[p:j]) if j > p else 0
                    seg = n[j:j + ln]
                g2["nn::" + seg] += 1
            else:
                g2[n.split("_")[0] or n] += 1
        print("\n== import 按命名空间分组（这就是宿主 HLE 的工作面）==")
        for k, c in g2.most_common():
            print(f"  {c:5d}  {k}")

    if "--imports" in sys.argv:
        print("\n== import 全表 ==")
        for n, _v, _s, _z in sorted(imports):
            print(f"  {n}")

    if "--exports" in sys.argv:
        print("\n== export 全表 ==")
        for n, v, _s, _z in sorted(exports, key=lambda x: x[1]):
            print(f"  0x{v:08x}  {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
