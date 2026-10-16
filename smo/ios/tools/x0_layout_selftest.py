#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""x0_layout 的自检：边界页检查必须能**报错**，不只会说 OK。

不需要游戏文件。造两个最小的 MOD0 映像：
  · 好的：重定位写在 16K 之后（与真实模块一致）→ 必须放行
  · 坏的：重定位写在 [TEXT_END, 圆整16K) 里（rtld 的形状）→ 必须拒绝，并说清楚是哪一页

再端到端跑一次 gen_embed.py 与 guest_dylib.py：坏的模块两条出包路线都必须拒绝出包、退出码非 0。
用法：python smo/ios/tools/x0_layout_selftest.py
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from x0_layout import boundary_relocs, check_x0, x0_end      # noqa: E402

FAILS = []


def check(name, cond):
    print(("  ✅ " if cond else "  ✗ ") + name)
    if not cond:
        FAILS.append(name)


def make_image(reloc_off, text_end=0x200):
    """最小 MOD0 映像：一条 RELATIVE 重定位写在 reloc_off。返回 (映像, fixups 头)。"""
    img = bytearray(0x10000)
    mod0, dyn, rela = 0x40, 0x80, 0x180
    struct.pack_into("<I", img, 4, mod0)
    img[mod0:mod0 + 4] = b"MOD0"
    struct.pack_into("<i", img, mod0 + 4, dyn - mod0)
    for i, (t, v) in enumerate(((7, rela), (8, 24), (23, rela), (2, 0))):
        struct.pack_into("<qQ", img, dyn + i * 16, t, v)
    struct.pack_into("<QQq", img, rela, reloc_off, 1027, 0x10)        # R_AARCH64_RELATIVE
    hdr = {"PATCH_OFF": 0xC000, "KEY_PAGE_OFF": 0x8000, "KEY_PAGE_SIZE": 0x4000,
           "TEXT_END": text_end, "RO_OFF": text_end, "DATA_OFF": 0x4000}
    return bytes(img), hdr


def write_artifacts(d, name, img, hdr):
    open(os.path.join(d, f"{name}.patched.bin"), "wb").write(img)
    with open(os.path.join(d, f"{name}.fixups.tsv"), "w", encoding="utf-8") as f:
        for k, v in hdr.items():
            f.write(f"# {k}\t{v:#x}\n")
        for i, k in enumerate(("K18", "KTLS", "KTLR", "KGATE", "KRES0", "KRES1", "KRES2", "KRES3")):
            f.write(f"# KEY_{k}_IDX\t{i}\n")
    open(os.path.join(d, f"{name}.gates.tsv"), "w").close()


def run(args):
    env = dict(os.environ, PYTHONIOENCODING="utf-8")
    r = subprocess.run([sys.executable] + args, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", env=env)
    return r.returncode, r.stdout + r.stderr


def main():
    print("== 1. 切点：TEXT_END 向上取整到 16K ==")
    check("0x200 → 0x4000", x0_end(0x200) == 0x4000)
    check("0x4000（已对齐）→ 0x4000，不多占一页", x0_end(0x4000) == 0x4000)
    check("0x3de8b8（sdk 实测）→ 0x3e0000", x0_end(0x3DE8B8) == 0x3E0000)
    check("0xbf1838（main 实测）→ 0xbf4000", x0_end(0xBF1838) == 0xBF4000)

    print("== 2. 边界页检查 ==")
    good, _ = make_image(0x4008)
    bad, _ = make_image(0x0300)
    edge_lo, _ = make_image(0x0200)                  # 恰在 TEXT_END 上
    edge_hi, _ = make_image(0x3FF8)                  # 边界页最后一个槽
    past, _ = make_image(0x4000)                     # 恰在边界页之后
    check("重定位在 16K 之后 → 放行", check_x0("good", good, 0x200) is None)
    check("重定位在边界页里 → 拒绝", check_x0("bad", bad, 0x200) is not None)
    check("恰在 TEXT_END 上 → 拒绝（区间含下端）", len(boundary_relocs(edge_lo, 0x200)) == 1)
    check("边界页最后一个槽 → 拒绝", len(boundary_relocs(edge_hi, 0x200)) == 1)
    check("恰在 圆整16K 上 → 放行（区间不含上端）", len(boundary_relocs(past, 0x200)) == 0)
    why = check_x0("bad", bad, 0x200) or ""
    check("拒绝理由里写明了页范围与条数", "0x200" in why and "0x4000" in why and "1 条" in why)

    print("== 3. 两条出包路线端到端：坏模块必须拒绝出包 ==")
    with tempfile.TemporaryDirectory(prefix="x0chk_") as d:
        img, hdr = make_image(0x0300)
        write_artifacts(d, "bad", img, hdr)
        rc, out = run([os.path.join(HERE, "gen_embed.py"), d, os.path.join(d, "e.S"), "bad"])
        check(f"gen_embed.py 拒绝（退出码 {rc}）", rc != 0 and "边界页" in out)
        check("gen_embed.py 没写出 .S", not os.path.exists(os.path.join(d, "e.S")))
        rc, out = run([os.path.join(HERE, "guest_dylib.py"), d, os.path.join(d, "g"), "bad"])
        check(f"guest_dylib.py 拒绝（退出码 {rc}）", rc != 0 and "边界页" in out)
        check("guest_dylib.py 没写出 Guest",
              not os.path.exists(os.path.join(d, "g", "Guest.framework", "Guest")))

    print()
    if FAILS:
        print(f"{len(FAILS)} 条失败")
        return 1
    print("全部通过 —— 边界页检查能放行好的、拒绝坏的，两条出包路线都守住了")
    return 0


if __name__ == "__main__":
    sys.exit(main())
