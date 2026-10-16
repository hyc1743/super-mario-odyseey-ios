#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成内嵌汇编：把改写后的模块映像编进 App（iOS 真机不许运行时生成/改代码）。

派生自参考项目 d2r-mobile 的 dm/ios/tools/gen_embed.py（MIT），契约不变：

    __TEXT,__dm<名>x0   原代码 [0, 圆整16K(TEXT_END))            → RX（含 ro 首页的一部分，只读）
    __DM<名>W,__img     ro 余部 + data + bss [圆整16K(TEXT_END), PATCH_OFF)  → RW（重定位只落这里）
    __TEXT,__dm<名>x1   补丁区 [PATCH_OFF, 圆整16K(文件尾))       → RX

x0 的切点是 TEXT_END **向上**取整（见 x0_layout.py）：边界页归代码段，所以那一页里不能有重定位
要写。出包时逐条核对，有就拒绝出包 —— iOS 上没有 RWX 页可用，这个问题不能留到真机。

为什么拆三段（两条硬约束一起满足）：
  · 苹果只允许**一个**带执行权限的段（ITMS-90999），所以两段代码都必须落在 __TEXT 里；
  · 而 data/bss 必须可写、代码必须不可写，所以中间那段单独放进可写段。
运行时装载器保留一块连续地址，把三段按原相对位置重映射回去 —— 于是运行时布局与原映像逐字节相同，
PC 相对寻址、补丁区的相对跳转、宿主「基址 + 原偏移」的引用全都不用改。

用法：
  gen_embed.py <产物目录> <输出 .S> <模块名...>
  → 写出 .S、`<输出>.manifest.tsv`（每段的偏移/大小/sha256，供 check_embed.py 校验），
    以及 `<输出>.c`（内嵌表：三段指针/大小 + 改写器表头 + 门表 —— iOS 上没有 work/rewrite/*.tsv 可读，
    装载器必须从这张表里取布局与门），链接参数（可写段的 -segprot）打印到 stdout。
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from x0_layout import check_x0, x0_end                              # noqa: E402

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
    flags, rows, mods = [], [], []
    for n in names:
        hdr = read_hdr(os.path.join(out, f"{n}.fixups.tsv"))
        need = ("TEXT_END", "PATCH_OFF", "KEY_PAGE_OFF", "KEY_PAGE_SIZE")
        missing = [k for k in need if k not in hdr]
        if missing:
            print(f"🔴 {n}.fixups.tsv 头里缺：{' '.join(missing)}")
            return 2
        te, po = int(hdr["TEXT_END"], 16), int(hdr["PATCH_OFF"], 16)
        ko, ks = int(hdr["KEY_PAGE_OFF"], 16), int(hdr["KEY_PAGE_SIZE"], 16)
        # 路径统一成正斜杠：反斜杠在汇编器的字符串里是转义符（Windows 上生成的 .S 会踩）
        binp = os.path.abspath(os.path.join(out, f"{n}.patched.bin")).replace(chr(92), "/")
        sz = os.path.getsize(binp)
        img = open(binp, "rb").read()
        # x0 向上取整：代码尾与 ro 开头同页时，这一页归代码段（RX）。
        # 早先是向下取整 + 把边界页 mprotect 成 RWX —— macOS 上能跑，iOS 上必失败，
        # 于是 .text 末尾那几 KB 代码在真机上不可执行（第二十六轮）。
        why = check_x0(n, img, te)
        if why:
            print(f"🔴 {why}")
            return 2
        a = x0_end(te)
        end = r16(sz)

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

        # 标签必须**在段内、紧邻 .incbin**：标签落在错误的段里会让链接期找不到符号
        # （CI 上真实踩过：Undefined symbols: "__dm_sdk_x0"）。
        syms = {}
        secs = ((f"__TEXT,__dm{n}x0", 0, a), (f"__DM{n}W,__img", a, po), (f"__TEXT,__dm{n}x1", po, end))
        for idx, (sect, lo, hi) in enumerate(secs):
            sym = f"_dm_{n}_{'x0' if idx == 0 else ('w' if idx == 1 else 'x1')}"
            syms[idx] = sym
            seg, sec = sect.split(",")
            if len(seg) > 16 or len(sec) > 16:      # Mach-O 段名/节名上限 16 字节
                print(f"🔴 {n}: 名字太长（Mach-O 上限 16 字节）：{sect}")
                return 2
            fill = min(hi, sz) - lo                 # 映像之外的部分用 .space 补零
            if fill < 0:
                print(f"🔴 {n}: 段 {sect} 越界")
                return 2
            asm += [f"    .section {sect}", "    .p2align 14",
                    f"    .globl {syms[idx]}", f"{syms[idx]}:",
                    f'    .incbin "{binp}", {lo:#x}, {fill:#x}']
            if hi - lo > fill:
                asm.append(f"    .space {hi - lo - fill:#x}")
            asm.append("    .p2align 14")
            piece = img[lo:min(hi, sz)] + b"\x00" * max(0, hi - max(lo, min(hi, sz)))
            rows.append((n, sect, lo, hi - lo, hashlib.sha256(piece).hexdigest()[:16]))
        flags.append(f"-Wl,-segprot,__DM{n}W,rw,rw")

        # 门表：gates.tsv 每行 `返回点 种类 参数 原指令地址`（装载器用它登记宿主门）
        gates = []
        gp = os.path.join(out, f"{n}.gates.tsv")
        if os.path.exists(gp):
            for line in open(gp, encoding="utf-8"):
                if line.startswith("#"):
                    continue
                q = line.split()
                if len(q) == 4:
                    gates.append((int(q[0], 16), q[1], int(q[2]), int(q[3], 16)))
        mods.append((n, hdr, te, po, a, end, syms, gates))

    open(dst, "w", encoding="utf-8", newline="\n").write("\n".join(asm) + "\n")
    man = dst + ".manifest.tsv"
    with open(man, "w", encoding="utf-8", newline="\n") as f:
        f.write("# 模块\t段\t偏移\t大小\tsha256前16\n")
        for r in rows:
            f.write(f"{r[0]}\t{r[1]}\t0x{r[2]:x}\t0x{r[3]:x}\t{r[4]}\n")
    # ---- 内嵌表（.c）：布局头 + 门表 + 三段指针。装载器在 iOS 上没有 tsv 可读，只认这张表 ----
    KEY_NAMES = ["K18", "KTLS", "KTLR", "KGATE", "KRES0", "KRES1", "KASPECT", "KASPECTUI"]
    c = ["// 由 smo/ios/tools/gen_embed.py 生成，勿手改。",
         "// 内嵌表：三段指针/大小 + 改写器表头 + 门表（iOS 上没有 work/rewrite/*.tsv 可读）。",
         "#include \"../host/dm.h\"", ""]
    for n, hdr, te, po, a, end, syms, gates in mods:
        c.append(f"// ---- {n} ----")
        for sym in (syms[0], syms[1], syms[2]):
            # C 声明去掉前导下划线：Mach-O 上 C 符号会自己加一个下划线，
            # 写成 `_dm_main_x0` 会变成链接期符号 `__dm_main_x0` 而与汇编里的 `_dm_main_x0` 对不上
            c.append(f"extern const uint8_t {sym[1:]}[];")
        c.append(f"static const dm_embed_gate g_gate_{n}[] = {{")
        for ret, kind, arg, orig in gates:
            c.append(f"    {{ 0x{ret:x}, DM_GK_{kind.upper()}, {arg}, 0x{orig:x} }},")
        c.append("};")
        c.append("")
    c.append("static const dm_embed_mod g_embed_mods[] = {")
    for n, hdr, te, po, a, end, syms, gates in mods:
        t, r, d = (int(hdr.get("TEXT_END", "0"), 16), int(hdr.get("RO_OFF", "0"), 16),
                   int(hdr.get("DATA_OFF", "0"), 16))
        kpo, kps = int(hdr["KEY_PAGE_OFF"], 16), int(hdr["KEY_PAGE_SIZE"], 16)
        idxs = []
        for k in KEY_NAMES:
            key = f"KEY_{k}_IDX"
            idxs.append(int(hdr.get(key, "0"), 10))
        idx_s = ", ".join(str(i) for i in idxs)
        c.append(f'    {{ "{n}",')
        c.append(f"      {syms[0][1:]}, 0x{a:x}, {syms[1][1:]}, 0x{po - a:x}, {syms[2][1:]}, 0x{end - po:x},")
        c.append(f"      0x{os.path.getsize(os.path.join(out, n + '.patched.bin')):x},   // image_size")
        c.append(f"      0x{t:x}, 0x{r:x}, 0x{d:x}, 0x{po:x}, 0x{kpo:x}, 0x{kps:x},")
        c.append(f"      {{ {idx_s} }},")
        c.append(f"      g_gate_{n}, (unsigned)(sizeof g_gate_{n} / sizeof g_gate_{n}[0]) }},")
    c.append("};")
    c.append(f"static const unsigned g_embed_n = {len(mods)};")
    c.append("")
    c.append("// 访问器（函数）：Mach-O 的弱导入只对函数可靠，对数据符号不生效（smo_host 不链内嵌表时链接失败）")
    c.append("const dm_embed_mod *dm_embed_table(unsigned *n) {")
    c.append("    *n = g_embed_n; return g_embed_mods;")
    c.append("}")
    cpath = os.path.splitext(dst)[0] + ".c"
    open(cpath, "w", encoding="utf-8", newline=chr(10)).write(chr(10).join(c) + chr(10))
    print(f"已生成 {dst}（{len(names)} 个模块，{len(rows)} 段）、{man} 与 {cpath}")
    print(" ".join(flags))
    return 0


if __name__ == "__main__":
    sys.exit(main())
