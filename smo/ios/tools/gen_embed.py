#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成内嵌汇编：把改写后的模块映像编进 App（iOS 真机不许运行时生成/改代码）。

派生自参考项目 d2r-mobile 的 dm/ios/tools/gen_embed.py（MIT），契约不变：

    __TEXT,__dm<名>x0   原代码 [0, 圆整16K(TEXT_END))            → RX
    __DM<名>W,__img     ro 余部 + data + bss [圆整16K(TEXT_END), PATCH_OFF)  → RW
    __TEXT,__dm<名>x1   补丁区 [PATCH_OFF, 圆整16K(文件尾))       → RX

为什么拆三段（两条硬约束一起满足）：
  · 苹果只允许**一个**带执行权限的段（ITMS-90999），所以两段代码都必须落在 __TEXT 里；
  · 而 data/bss 必须可写、代码必须不可写，所以中间那段单独放进可写段。
运行时装载器保留一块连续地址，把三段按原相对位置重映射回去 —— 于是运行时布局与原映像逐字节相同，
PC 相对寻址、补丁区的相对跳转、宿主「基址 + 原偏移」的引用全都不用改。

用法：
  gen_embed.py <产物目录> <输出 .S> <模块名...>
  → 写出 .S 与 `<输出>.manifest.tsv`（每段的偏移/大小/sha256，供 check_embed.py 校验），
    链接参数（可写段的 -segprot）打印到 stdout。
"""
import hashlib
import os
import sys

PG = 0x4000


def r16(x):
    return (x + PG - 1) & ~(PG - 1)


def read_hdr(path):
    """读 fixups.tsv 的 `# KEY value` 头（我们的格式：`# PATCH_OFF 0x...`、`# KEY_PAGE_OFF 0x...`）。"""
    hdr = {}
    for line in open(path, encoding="utf-8"):
        if not line.startswith("#"):
            break
        p = line[1:].strip().split()
        if len(p) == 2:
            hdr[p[0]] = p[1]
    return hdr


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    out, dst, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    asm = ["// 由 smo/ios/tools/gen_embed.py 生成，勿手改"]
    flags, rows = [], []
    for n in names:
        hdr = read_hdr(os.path.join(out, f"{n}.fixups.tsv"))
        need = ("TEXT_END", "PATCH_OFF", "KEY_PAGE_OFF", "KEY_PAGE_SIZE")
        missing = [k for k in need if k not in hdr]
        if missing:
            print(f"🔴 {n}.fixups.tsv 头里缺：{' '.join(missing)}")
            return 2
        te, po = int(hdr["TEXT_END"], 16), int(hdr["PATCH_OFF"], 16)
        ko, ks = int(hdr["KEY_PAGE_OFF"], 16), int(hdr["KEY_PAGE_SIZE"], 16)
        binp = os.path.abspath(os.path.join(out, f"{n}.patched.bin"))
        sz = os.path.getsize(binp)
        img = open(binp, "rb").read()
        a, end = r16(te), r16(sz)

        # 这些断言是「三段能拼回原映像」的前提，错一条就会在真机上以难以定位的方式崩
        checks = [
            (po % PG == 0, f"PATCH_OFF 0x{po:x} 必须 16K 对齐"),
            (ko % PG == 0, f"KEY_PAGE_OFF 0x{ko:x} 必须 16K 对齐"),
            (ks == PG, f"键页大小必须 = 0x{PG:x}（实际 0x{ks:x}）"),
            (a <= ko and ko + ks <= po, "键页必须落在 ro/data 段内（不全在补丁区、也不全在代码段）"),
            (po <= end, f"PATCH_OFF 0x{po:x} 不能超过圆整后的文件尾 0x{end:x}"),
            (end <= sz + PG, "圆整后的文件尾不应超过映像 + 一页"),
        ]
        for ok, why in checks:
            if not ok:
                print(f"🔴 {n}: {why}")
                return 2

        secs = ((f"__TEXT,__dm{n}x0", 0, a), (f"__DM{n}W,__img", a, po), (f"__TEXT,__dm{n}x1", po, end))
        for sect, lo, hi in secs:
            seg, sec = sect.split(",")
            if len(seg) > 16 or len(sec) > 16:      # Mach-O 段名/节名上限 16 字节
                print(f"🔴 {n}: 名字太长（Mach-O 上限 16 字节）：{sect}")
                return 2
            fill = min(hi, sz) - lo                 # 映像之外的部分用 .space 补零
            if fill < 0:
                print(f"🔴 {n}: 段 {sect} 越界")
                return 2
            asm += [f"    .section {sect}", "    .p2align 14",
                    f'    .incbin "{binp}", {lo:#x}, {fill:#x}']
            if hi - lo > fill:
                asm.append(f"    .space {hi - lo - fill:#x}")
            asm.append("    .p2align 14")
            piece = img[lo:min(hi, sz)] + b"\x00" * max(0, hi - max(lo, min(hi, sz)))
            rows.append((n, sect, lo, hi - lo, hashlib.sha256(piece).hexdigest()[:16]))
        flags.append(f"-Wl,-segprot,__DM{n}W,rw,rw")

    open(dst, "w", encoding="utf-8", newline="\n").write("\n".join(asm) + "\n")
    man = dst + ".manifest.tsv"
    with open(man, "w", encoding="utf-8", newline="\n") as f:
        f.write("# 模块\t段\t偏移\t大小\tsha256前16\n")
        for r in rows:
            f.write(f"{r[0]}\t{r[1]}\t0x{r[2]:x}\t0x{r[3]:x}\t{r[4]}\n")
    print(f"已生成 {dst}（{len(names)} 个模块，{len(rows)} 段）与 {man}")
    print(" ".join(flags))
    return 0


if __name__ == "__main__":
    sys.exit(main())
