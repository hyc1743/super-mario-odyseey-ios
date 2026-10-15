#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按 D3D BC 规范写的参考解码器 —— 给纹理内容核对当**独立判据**。

为什么要自己写：texture2ddecoder（pip）的 BC 内核其实就是 bcdec（和我们运行时同一份），
用它当判据就不是独立实现了。这里按规范文本从零实现 BC1/BC3/BC4/BC5，输出布局与我们的
nvn_bcdec.c 一致（BC1/BC3 → RGBA8，BC4 → R8，BC5 → RG8），供逐字节对比。

与 bcdec 的两处**已知、有界**的差异（对不上时先看这里，别急着报错）：
  1. 565→888 扩展：bcdec 用魔数近似 `(v*527+23)>>6`；规范式是 `(v*255+15)>>5`。端点本身会差 ±1。
  2. BC1 的 2/3、1/3 插值：bcdec 把「插值+扩展」合并成一个魔数式；这里是规范式两步
     （先按分量 (2a+b+1)//3 取整，再精确扩展）。插值色会差 ±1。
  所以对比口径：**端点逐字节一致；插值产生的字节允许 ±1（全部必须 ≤1，计数报告）**。
  BC4/BC5 的 alpha 插值同理允许 ±1（bcdec 的权重表是 65536 截断近似，如
  (56173·a0+9363·a1+32768)>>16 vs 规范的 (6a0+a1+3)//7）—— 早先以为它逐字节一致，
  那是抽样没撞上不一致的组合，不是事实。

自检：`python bc_spec_ref.py --selftest`（手工构造块、期望值手算）。
"""
import sys


def _exp5(v):
    return (v << 3) | (v >> 2)          # 5 位 → 8 位：D3D 规范是**位复制**（不是除法取整）


def _exp6(v):
    return (v << 2) | (v >> 4)


def _unpack565(c):
    return (c >> 11) & 0x1F, (c >> 5) & 0x3F, c & 0x1F


def _bc1_palette(c0, c1):
    """规范 BC1 调色板（含 BC1A 的 c0<=c1 情形）→ 4 个 (r,g,b,a)。

    关键口径（曾在这里读错规范，务必保持）：端点先按**位复制**扩到 8 位，
    插值在**扩展后的 8 位空间**做整数平均取整 —— 不是先插值后扩展。
    （先插值后扩展会把 r0=31,r1=0 的 2/3 色算成 173；规范是 170。）"""
    e0 = (_exp5((c0 >> 11) & 0x1F), _exp6((c0 >> 5) & 0x3F), _exp5(c0 & 0x1F))
    e1 = (_exp5((c1 >> 11) & 0x1F), _exp6((c1 >> 5) & 0x3F), _exp5(c1 & 0x1F))
    pal = [(e0[0], e0[1], e0[2], 255), (e1[0], e1[1], e1[2], 255)]
    if c0 > c1:
        pal.append(((2 * e0[0] + e1[0] + 1) // 3, (2 * e0[1] + e1[1] + 1) // 3,
                    (2 * e0[2] + e1[2] + 1) // 3, 255))
        pal.append(((e0[0] + 2 * e1[0] + 1) // 3, (e0[1] + 2 * e1[1] + 1) // 3,
                    (e0[2] + 2 * e1[2] + 1) // 3, 255))
    else:
        pal.append(((e0[0] + e1[0] + 1) // 2, (e0[1] + e1[1] + 1) // 2,
                    (e0[2] + e1[2] + 1) // 2, 255))
        pal.append((0, 0, 0, 0))
    return pal


def _bc1_block(b, out, base, dpitch):
    c0 = b[0] | (b[1] << 8)
    c1 = b[2] | (b[3] << 8)
    idx = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24)
    pal = _bc1_palette(c0, c1)
    for y in range(4):
        for x in range(4):
            p = pal[(idx >> (2 * (y * 4 + x))) & 3]
            k = base + y * dpitch + x * 4
            out[k] = p[0]; out[k + 1] = p[1]; out[k + 2] = p[2]; out[k + 3] = p[3]


def _bc4_alpha8(a0, a1, idx, signed):
    """规范 BC4 的 8 个 alpha（带符号：-128 钳到 -127，输出仍按 8 位无符号字节）。"""
    if signed:
        if a0 >= 128: a0 -= 256
        if a1 >= 128: a1 -= 256
        a0 = max(a0, -127); a1 = max(a1, -127)
    al = [a0, a1]
    if a0 > a1:
        for w in (6, 5, 4, 3, 2, 1):
            al.append((w * a0 + (7 - w) * a1 + 3) // 7)
    else:
        for w in (4, 3, 2, 1):
            al.append((w * a0 + (5 - w) * a1 + 2) // 5)
        al += [0, 255]
    return al


def _bc4_block(b, out, base, dpitch, signed):
    a = _bc4_alpha8(b[0], b[1], 0, signed)
    bits = int.from_bytes(b[2:8], "little")       # 48 位 = 16 个 3 位索引
    for y in range(4):
        for x in range(4):
            out[base + y * dpitch + x] = a[(bits >> (3 * (y * 4 + x))) & 7] & 0xFF


def _bc5_block(b, out, base, dpitch, signed):
    # 红块在前 8 字节、绿块在后 8 字节；输出 RG8（与我们的 nvn_bcdec 同布局）
    ra = _bc4_alpha8(b[0], b[1], 0, signed)
    ga = _bc4_alpha8(b[8], b[9], 0, signed)
    rb = int.from_bytes(b[2:8], "little")
    gb = int.from_bytes(b[10:16], "little")
    for y in range(4):
        for x in range(4):
            k = base + y * dpitch + x * 2
            out[k] = ra[(rb >> (3 * (y * 4 + x))) & 7] & 0xFF
            out[k + 1] = ga[(gb >> (3 * (y * 4 + x))) & 7] & 0xFF


def decode_bc1(data, w, h):
    out = bytearray(w * h * 4)
    wb = (w + 3) // 4
    for by in range((h + 3) // 4):
        for bx in range(wb):
            i = (by * wb + bx) * 8
            _bc1_block(data[i:i + 8], out, (by * 4 * w + bx * 4) * 4, w * 4)
    return bytes(out)


def decode_bc3(data, w, h):
    out = bytearray(w * h * 4)
    wb = (w + 3) // 4
    for by in range((h + 3) // 4):
        for bx in range(wb):
            i = (by * wb + bx) * 16
            b = data[i:i + 16]
            _bc1_block(b[8:16], out, (by * 4 * w + bx * 4) * 4, w * 4)
            al = _bc4_alpha8(b[0], b[1], 0, False)
            idx = int.from_bytes(b[2:8], "little")
            for y in range(4):
                for x in range(4):
                    out[((by * 4 + y) * w + bx * 4 + x) * 4 + 3] = al[(idx >> (3 * (y * 4 + x))) & 7] & 0xFF
    return bytes(out)


def decode_bc4(data, w, h):
    out = bytearray(w * h)
    wb = (w + 3) // 4
    for by in range((h + 3) // 4):
        for bx in range(wb):
            i = (by * wb + bx) * 8
            _bc4_block(data[i:i + 8], out, by * 4 * w + bx * 4, w, False)
    return bytes(out)


def decode_bc5(data, w, h):
    out = bytearray(w * h * 2)
    wb = (w + 3) // 4
    for by in range((h + 3) // 4):
        for bx in range(wb):
            i = (by * wb + bx) * 16
            _bc5_block(data[i:i + 16], out, (by * 4 * w + bx * 4) * 2, w * 2, False)
    return bytes(out)


def selftest():
    fails = 0

    def chk(cond, msg):
        nonlocal fails
        print("  %s %s" % ("✅" if cond else "【红】", msg))
        if not cond:
            fails += 1

    # BC1：红/蓝端点，全 0 索引 → 全部红
    blk = bytes([0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0])
    r = decode_bc1(blk, 4, 4)
    chk(r == bytes([255, 0, 0, 255]) * 16, "BC1 端点直给 → 全红（规范式扩展也精确）")
    # BC1 插值：c0=红(31,0,0) c1=黑(0,0,0)，索引 10（= 2/3 插值色）
    idx = 0b10000000000000000000000000000010
    blk2 = bytes([0x00, 0xF8, 0x00, 0x00]) + idx.to_bytes(4, "little")
    r2 = decode_bc1(blk2, 4, 4)
    chk(r2[:4][0] == 170, "BC1 2/3 插值：端点扩展后 (2*255+0+1)//3 = 170（手算；先插值后扩展会错算成 173）")
    chk(r2[:4][3] == 255, "插值色 alpha 保持 255")
    # BC4：255/0 全 0 索引 → 全 255
    r = decode_bc4(bytes([0xFF, 0x00, 0, 0, 0, 0, 0, 0]), 4, 4)
    chk(r == bytes([255]) * 16, "BC4 端点 → 全 255")
    # BC4 插值：a0=200 a1=100，索引 2（0/1 是端点，插值从 2 起）→ (6*200+100+3)//7 = 186
    blk = bytes([200, 100]) + bytes([0b000010]) + bytes(3)
    r = decode_bc4(blk, 4, 4)
    chk(r[0] == 186, "BC4 6/7 插值 = 186（手算）")
    # BC5：R=64 G=192 → RG8
    blk = bytes([0x40]) + bytes(7) + bytes([0xC0]) + bytes(7)
    r = decode_bc5(blk, 4, 4)
    chk(r == bytes([64, 192]) * 16, "BC5 端点 → 每像素 (R=64, G=192)")
    # BC3：alpha 全 255 + 红块
    blk = bytes([0xFF, 0x00] + [0] * 6) + bytes([0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0])
    r = decode_bc3(blk, 4, 4)
    chk(r == bytes([255, 0, 0, 255]) * 16, "BC3 alpha=255 + 红块 → 全红")
    print("\n%s" % ("全部通过" if fails == 0 else "有断言不通过"))
    return fails != 0


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(1 if selftest() else 0)
    print(__doc__)
    sys.exit(2)
