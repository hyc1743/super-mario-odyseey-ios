#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成占位 App 图标（纯标准库，不依赖 Pillow）。

为什么是占位图标：**原版游戏的图标属于游戏内容，不能进本仓库**（参考项目同样是自己画图标）。
这里画一个中性图形（深蓝渐变 + 白色手机轮廓 + 播放三角），与本项目任何商标无关。
输出：Resources/Assets.xcassets/AppIcon.appiconset/{icon1024.png,Contents.json}

用法：python3 smo/ios/tools/gen_icon.py
"""
import json
import os
import struct
import zlib

S = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources",
                   "Assets.xcassets", "AppIcon.appiconset")


def png(path, w, h, rows):
    """rows: 每行 bytes（RGB）"""
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    hdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", hdr)
                           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def lerp(a, b, t):
    return int(a + (b - a) * t)


def render():
    top, bot = (27, 42, 74), (8, 12, 22)
    accent = (232, 69, 60)
    body_w, body_h, radius = 380, 640, 84
    cx, cy = S / 2, S / 2
    tri = ((cx + 6, cy - 96), (cx + 6, cy + 96), (cx + 118, cy))
    rows = []
    for y in range(S):
        row = bytearray()
        base = [lerp(top[i], bot[i], y / (S - 1)) for i in range(3)]
        for x in range(S):
            px = base[:]
            # 手机轮廓：圆角矩形的“环”
            dx, dy = abs(x - cx), abs(y - cy)
            qx, qy = dx - (body_w / 2 - radius), dy - (body_h / 2 - radius)
            d = ((max(qx, 0) ** 2) + (max(qy, 0) ** 2)) ** 0.5 + min(max(qx, qy), 0) - radius
            if abs(d) < 9:
                px = [220, 226, 236]
            # 播放三角（点是否在三角形内，用重心符号法）
            def sign(a, b, c):
                return (a[0] - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (a[1] - c[1])
            p = (x, y)
            d1, d2, d3 = sign(p, tri[0], tri[1]), sign(p, tri[1], tri[2]), sign(p, tri[2], tri[0])
            neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
            pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
            if not (neg and pos):
                px = accent
            row += bytes(px)
        rows.append(bytes(row))
    return rows


def main():
    os.makedirs(OUT, exist_ok=True)
    png(os.path.join(OUT, "icon1024.png"), S, S, render())
    json.dump({"images": [{"filename": "icon1024.png", "idiom": "universal",
                           "platform": "ios", "size": "1024x1024"}],
               "info": {"author": "xcode", "version": 1}},
              open(os.path.join(OUT, "Contents.json"), "w"), indent=2)
    json.dump({"info": {"author": "xcode", "version": 1}},
              open(os.path.join(os.path.dirname(OUT), "Contents.json"), "w"), indent=2)
    print("图标已生成：", os.path.join(OUT, "icon1024.png"))


if __name__ == "__main__":
    main()
