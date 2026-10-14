#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Nintendo 归档格式的读写：Yaz0 压缩 + SARC 打包 + 资源表（ResFile/ResTable）。

这三样**离线和宿主都要用**：
  - 离线：解 `RomFS/ShaderData/*.szs`，把 agl 着色器程序取出来喂给翻译链
  - 宿主：游戏运行时会读 romfs 里的 .szs，宿主必须能解开（hle_fs 层），所以这份实现之后要照抄成 C

格式依据（公开、可自证）：
  Yaz0：magic "Yaz0" + 解压后大小 + 8 字节对齐/保留，之后是变长 code 流。本文件的实现用
        「解压结果长度 == 头里声明的大小」自证（不符即抛）。
  SARC：magic "SARC" + 头长 0x14 + BOM 0xFEFF；内含 SFAT（节点表）与 SFNT（名字表）。
  ResFile/ResTable（agl 等 Nintendo 库用的表）：本文件按实际数据推断，读时逐项断言（不猜）。

用法：
  nintendo_archives.py list  <文件>            列出归档内容（.szs/.sarc 都认）
  nintendo_archives.py unpack <文件> <输出目录>
  nintendo_archives.py hexdump <文件> [偏移] [长度]
"""
import os
import struct
import sys


# --------------------------------------------------------------------------- Yaz0

def yaz0_decompress(d):
    """Yaz0 → 解压后字节。

    code 字节只提供 8 个标志位（MSB 先）：1 = 后面跟 1 个直面字节；0 = 回引用，
    再读两个字节 b1/b2：
        距离-1 = ((b1 & 0xF) << 8) | b2
        长度   = (b1 >> 4)，为 0 时再读 1 字节 + 0x12，否则再 +2
    注意：长度的高 4 位和距离的高 4 位**都在 b1 里**，code 字节不参与回引用的数值。

    这个语义不是从文字描述推的：本机按「标准描述」手推的实现自相矛盾（第一个 code 0xFF 解出的
    8 个字面量恰好是 SARC 头 `SARC\x14\x00\xff\xfe`，紧接着的回引用却越界），于是拿
    公开实现 Cuyler36/Yaz0Decoder 的 Program.cs 对照后改正。自证：
    解压长度 == Yaz0 头声明值，且解出的 SARC 里 file_size 字段也等于它、`SFAT` 落在 0x14。
    """
    assert d[:4] == b"Yaz0", f"不是 Yaz0：{d[:4]!r}"
    size = struct.unpack_from(">I", d, 4)[0]
    out = bytearray()
    p = 0x10
    while len(out) < size:
        code = d[p]
        p += 1
        for _ in range(8):
            if len(out) >= size:
                break
            if code & 0x80:
                out.append(d[p])
                p += 1
            else:
                b1, b2 = d[p], d[p + 1]
                p += 2
                dist = ((b1 & 0x0F) << 8) | b2
                n = b1 >> 4
                if n == 0:
                    n = d[p] + 0x12
                    p += 1
                else:
                    n += 2
                if dist + 1 > len(out):
                    raise ValueError(f"Yaz0 回引用越界：dist={dist + 1} 已解出 {len(out)}")
                for _ in range(n):
                    out.append(out[-dist - 1])
            code = (code << 1) & 0xFF
    assert len(out) == size, f"Yaz0 解压长度不符 {len(out)} != {size}"
    return bytes(out)


# --------------------------------------------------------------------------- SARC

class Sarc:
    """SARC 归档：读出来就是 [(名字, 数据偏移, 大小)]，数据偏移是文件里的绝对偏移。

    关于名字偏移的**真实歧义**：节点里的名字字段（低 24 位）在 SMO 的这些 `*.szs` 里是
    「字节偏移 ÷ 4」——例如 alReducedShader 里第二个名字实际在字符串表 +24 处，字段写的是 6。
    而常见的 SARC 文档写的是字节偏移。不猜：名字表本身是连续的、4 字节对齐的 NUL 结尾字符串，
    把它们按顺序读出来就得到真实偏移，再与字段值比对，哪个约定能对上全部条目就用哪个
    （见 _detect_name_scale）。实测 27 个 `*.szs`：全部对上「÷4」，而且名字与内容 magic 互相印证
    （`.sharc` → `AAHS`，`.sharcb` → `BAHS`）。
    """

    def __init__(self, d):
        self.d = d
        assert d[:4] == b"SARC", f"不是 SARC：{d[:4]!r}"
        header_size, bom = struct.unpack_from("<HH", d, 4)
        assert bom == 0xFEFF, f"SARC 字节序标记不对 {bom:#x}"
        assert header_size == 0x14, f"SARC 头长 {header_size:#x}"
        # 头布局：0x04 头长(u16) 0x06 BOM(u16) 0x08 file_size(u32) 0x0C data_offset(u32)
        # 0x10 版本(u16) 0x12 保留(u16)。（自检 selftest.py 抓到过这里从 0x0C 起读的错位 bug）
        self.file_size, self.data_offset, self.version, _ = struct.unpack_from("<IIHH", d, 0x08)
        assert d[0x14:0x18] == b"SFAT", "找不到 SFAT"
        sfat_size, node_count, self.hash_key = struct.unpack_from("<HHI", d, 0x18)
        assert sfat_size == 0xC, f"SFAT 头长 {sfat_size:#x}"
        # SFAT 头长 0xC（magic 4 + 头长 2 + 节点数 2 + hashkey 4），节点紧跟在 SFAT 头之后
        nodes_off = 0x14 + sfat_size
        self.nodes = [struct.unpack_from("<IIII", d, nodes_off + i * 0x10) for i in range(node_count)]
        sfnt = nodes_off + node_count * 0x10
        assert d[sfnt:sfnt + 4] == b"SFNT", "找不到 SFNT"
        sfnt_size, _ = struct.unpack_from("<HH", d, sfnt + 4)
        assert sfnt_size == 8, f"SFNT 头长 {sfnt_size:#x}"
        strtab = sfnt + 8
        self.name_scale = self._detect_name_scale(strtab)
        self.entries = []
        for h, name_off, start, end in self.nodes:
            p = strtab + (name_off & 0xFFFFFF) * self.name_scale
            name = d[p:d.index(b"\0", p)].decode(errors="replace")
            self.entries.append((name, self.data_offset + start, end - start))

    def _detect_name_scale(self, strtab):
        """定下名字字段的单位：1（字节偏移）还是 4（字节偏移 ÷ 4）。

        判据不依赖任何猜测：把字符串表里的连续名字逐个读出来（NUL 结尾、向前对齐到 4 字节），
        得到一串真实字节偏移，再看哪个约定能解释全部节点的字段值。
        """
        seq, p, n = [], strtab, len(self.d)
        while p < n and self.d[p] != 0 and len(seq) < max(64, len(self.nodes) * 4):
            seq.append(p - strtab)
            p = (self.d.index(b"\0", p) + 1 + 3) & ~3
        fields = [f & 0xFFFFFF for _, f, _, _ in self.nodes]
        for scale in (1, 4):
            if all(f * scale in seq for f in fields):
                return scale
        return 1

    def read(self, name):
        for n, off, size in self.entries:
            if n == name:
                return self.d[off:off + size]
        raise KeyError(name)


def unpack_szs(path, outdir):
    """Yaz0(.szs) → SARC → 展开到目录；返回归档对象。"""
    d = open(path, "rb").read()
    if d[:4] == b"Yaz0":
        d = yaz0_decompress(d)
    s = Sarc(d)
    os.makedirs(outdir, exist_ok=True)
    for name, off, size in s.entries:
        dst = os.path.join(outdir, name)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(s.d[off:off + size])
    return s


# --------------------------------------------------------------------------- 资源表（agl/ResFile 风格）

class ResTable:
    """Nintendo 库的资源表（.bfres/.baglshader 等都用它）：定长头的索引表 + 名字表 + 数据区。

    头（按实测的 .szs 内文件推断，读时逐项断言）：
        0x00 magic(4)  0x04 u32 版本  0x08 u16 字节序标记  0x0A u16 头长
        0x0C u32 文件名表偏移  0x10 u16 文件名表项数  0x12 u16 数据区对齐
        0x14 u32 数据表偏移    0x18 u32 数据表项数
    索引项 0x10 字节：0x00 u32 名字偏移(24 位 + 8 位类型/目录位)  0x04 u32 数据偏移  0x08 u32 大小
    """

    def __init__(self, d):
        self.d = d
        self.magic = d[:4]
        self.version = struct.unpack_from("<I", d, 4)[0]
        bom, hdr_size = struct.unpack_from("<HH", d, 8)
        self.bom, self.hdr_size = bom, hdr_size
        self.strtab_off, n_names, self.data_align, self.datatab_off, n_entries = struct.unpack_from("<IHHII", d, 0x0C)
        assert self.bom == 0xFEFF, f"字节序标记不对 {bom:#x}"
        self.entries = []
        for i in range(n_entries):
            name_off, data_off, size = struct.unpack_from("<III", d, self.datatab_off + i * 0x10)
            name = self._str(name_off & 0xFFFFFF)
            self.entries.append((name, data_off, size, name_off >> 24))

    def _str(self, off):
        end = self.d.index(b"\0", off)
        return self.d[off:end].decode(errors="replace")


def hexdump(d, off=0, n=256):
    for i in range(off, min(off + n, len(d)), 16):
        chunk = d[i:i + 16]
        print(f"{i:08x}  {chunk.hex(' ', 4)}  |{''.join(chr(c) if 32 <= c < 127 else '.' for c in chunk)}|")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "list":
        d = open(path, "rb").read()
        if d[:4] == b"Yaz0":
            d = yaz0_decompress(d)
        s = Sarc(d)
        print(f"{os.path.basename(path)}: SARC 版本 {s.version:#x}，{len(s.entries)} 项")
        for name, off, size in s.entries:
            print(f"  {name:40s} {size:10d} @0x{off:x}")
    elif cmd == "unpack":
        unpack_szs(path, sys.argv[3])
        print(f"已展开到 {sys.argv[3]}")
    elif cmd == "hexdump":
        d = open(path, "rb").read()
        if d[:4] == b"Yaz0":
            d = yaz0_decompress(d)
            print(f"（Yaz0 已解压，{len(d)} 字节）")
        hexdump(d, int(sys.argv[3], 0) if len(sys.argv) > 3 else 0,
                int(sys.argv[4], 0) if len(sys.argv) > 4 else 256)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
