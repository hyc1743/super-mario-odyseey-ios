#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把已有的 RomFS.smoa.N 卷**重新分卷**（每个不超过指定大小），条目顺序与 CRC 原样保留。

为什么要这个工具：几千个小文件打成「两三个大文件」是为了手机侧好拷，但**单个文件太大**时
有的拷贝路径反而不可靠（实测：3.2 GB 的卷拷进手机后第 1 条 CRC 就不对）。分小一点（1 GB）
既好拷又好校验，代价只是多几个文件。

用法：
    smoarc_split.py <输出目录> <输入卷...> [--max-mb 1024] [--verify]
例：
    smoarc_split.py out/split out/smo-phone-package/"SMO Resource"/RomFS/RomFS.smoa.0 \
                    out/smo-phone-package/"SMO Resource"/RomFS/RomFS.smoa.1 --max-mb 1024 --verify

格式（与 smo/host/smoarc.h 一致）：
    "SMOA" u8(卷号) u8(卷总数)
    条目：u16(名字字节数) u32(数据字节数) u32(crc32) 名字 数据
    卷尾："END!"
条目**不跨卷**；单个条目大于上限时独占一卷（不会硬切，格式本身不支持切片）。
"""
import os
import struct
import sys
import zlib


def read_entries(volumes):
    """按顺序产出 (name_bytes, size, crc, 数据可读对象, 卷路径)。一次只持有当前条目的数据。"""
    for vp in volumes:
        f = open(vp, "rb")
        magic = f.read(4)
        if magic != b"SMOA":
            raise SystemExit(f"🔴 {vp} 不是 SMOA 卷（头是 {magic!r}）")
        vol = f.read(1)[0]
        f.read(1)                                   # 卷总数：重新分卷时不用
        while True:
            tag = f.read(4)
            if tag == b"END!":
                break
            if len(tag) != 4:
                raise SystemExit(f"🔴 {vp} 中途结束（没有 END! 标记）")
            f.seek(-4, 1)
            nl, sz, crc = struct.unpack("<HII", f.read(10))
            name = f.read(nl)
            yield name, sz, crc, f


def main():
    argv = sys.argv[1:]
    if len(argv) < 2:
        print(__doc__)
        return 1
    max_mb = 1024
    verify = "--verify" in argv
    if "--max-mb" in argv:
        max_mb = int(argv[argv.index("--max-mb") + 1])
    rest = [a for i, a in enumerate(argv)
            if not a.startswith("--") and not (i > 0 and argv[i - 1] == "--max-mb")]
    out_dir, volumes = rest[0], rest[1:]
    limit = max_mb * 1024 * 1024

    os.makedirs(out_dir, exist_ok=True)
    # **先写临时名，全部写完再删旧卷改名**：out_dir 很可能就是输入卷所在目录 ——
    # 上一版一上来就删 RomFS.smoa.*，把同目录的输入卷也删了（5.5 GB，只能从源重打一遍）。

    # 先扫一遍条目（不读数据）算出分卷方案与总数
    plan = []                                        # [(name, size, crc)]
    for name, sz, crc, f in read_entries(volumes):
        plan.append((name, sz, crc))
        f.seek(sz, 1)
    total_bytes = sum(sz for _, sz, _ in plan)

    groups, cur, cur_bytes = [], [], 6               # 6 = "SMOA" + 卷号 + 卷总数
    for e in plan:
        need = 10 + len(e[0]) + e[1]
        if cur and cur_bytes + need + 4 > limit:     # +4 = END!
            groups.append(cur)
            cur, cur_bytes = [], 6
        cur.append(e)
        cur_bytes += need
    if cur:
        groups.append(cur)
    print(f"条目 {len(plan)} 个 / {total_bytes} 字节 → 分 {len(groups)} 卷（每卷 ≤ {max_mb} MB）")

    # 再读一遍数据写出去（顺序一致，所以两次读取的条目一一对应）
    it = read_entries(volumes)
    tmp_paths = []
    for vi, group in enumerate(groups):
        path = os.path.join(out_dir, f"RomFS.smoa.{vi}.part")
        tmp_paths.append(path)
        written = 6
        with open(path, "wb") as o:
            o.write(b"SMOA" + bytes([vi, len(groups)]))
            for (name, sz, crc) in group:
                name2, sz2, crc2, f = next(it)
                if (name2, sz2, crc2) != (name, sz, crc):
                    raise SystemExit("🔴 两次扫描不一致（卷在变？）")
                o.write(struct.pack("<HII", len(name), sz, crc))
                o.write(name)
                left = sz
                while left:
                    b = f.read(min(1 << 20, left))
                    if not b:
                        raise SystemExit(f"🔴 读卷时提前结束（{name!r}）")
                    o.write(b)
                    left -= len(b)
                written += 10 + len(name) + sz
            o.write(b"END!")
        print(f"  {os.path.basename(path)}：{os.path.getsize(path)} 字节（{len(group)} 条）")

    # 新卷都写完了，这时候才清旧卷（含同目录的输入卷）再改名
    for old in os.listdir(out_dir):
        if old.startswith("RomFS.smoa.") and not old.endswith(".part"):
            os.remove(os.path.join(out_dir, old))
    for vi, tmp in enumerate(tmp_paths):
        os.replace(tmp, os.path.join(out_dir, f"RomFS.smoa.{vi}"))

    if verify:
        print("校验（逐条 CRC）…")
        bad = n = 0
        for name, sz, crc, f in read_entries([os.path.join(out_dir, f"RomFS.smoa.{i}")
                                              for i in range(len(groups))]):
            c = 0
            left = sz
            while left:
                b = f.read(min(1 << 20, left))
                if not b:
                    break
                c = zlib.crc32(b, c)
                left -= len(b)
            n += 1
            if (c & 0xFFFFFFFF) != crc:
                bad += 1
                print(f"  ❌ {name.decode('utf-8', 'replace')}")
        print(f"  校验完成：{n} 条，坏 {bad} 条" + ("" if not bad else " ❌"))
        if bad:
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
