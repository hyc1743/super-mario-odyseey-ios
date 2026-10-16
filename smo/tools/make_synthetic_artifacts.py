#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""造一份**合成改写产物**，让装载器自检在没有游戏文件的 CI 上也能跑。

真产物（`work/rewrite/*.patched.bin`）是游戏内容，不能进仓、也不能上传到 CI。
所以这里按**同一个格式**造一份最小的：一个模块映像（MOD0 + 动态段 + 符号表 + 字符串表 + 重定位），
外加改写器产出的 `fixups.tsv` 与 `gates.tsv`。

装载器关心的东西一个不少：
  · 映像头部 `img[4] = MOD0 偏移`，MOD0 里给出动态段偏移
  · 动态段里的 DT_SYMTAB / DT_STRTAB / DT_INIT / DT_INIT_ARRAY / DT_RELA / DT_RELASZ / DT_JMPREL / DT_PLTRELSZ
  · 四类重定位各来一条：RELATIVE / ABS64 / GLOB_DAT / JUMP_SLOT
  · 未定义符号（走宿主的 import）与「本模块定义 + 跨模块导出」
  · fixups.tsv 的表头（段边界、键页位置、8 个槽号）与 gates.tsv

用法：make_synthetic_artifacts.py <输出目录>
"""
import os
import struct
import sys

# 与改写器 patch_nso.py 的 KEY_INDEX 一致
KEYS = ["K18", "KTLS", "KTLR", "KGATE", "KRES0", "KRES1", "KASPECT", "KASPECTUI"]

DT = {"RELA": 7, "RELASZ": 8, "PLTRELSZ": 2, "JMPREL": 23, "SYMTAB": 6, "STRTAB": 5,
      "INIT": 12, "INIT_ARRAY": 25, "INIT_ARRAYSZ": 27}
R_RELATIVE, R_ABS64, R_GLOB_DAT, R_JUMP_SLOT = 1027, 257, 1025, 1026


def build_module(name, exports, imports, extra_relocs=()):
    """造一个模块映像，返回 (映像, 各段偏移)。"""
    # ---- 先排布局（全部 8 字节对齐）----
    text = bytearray(0x200)                 # 代码区（内容无所谓，装载器不看）
    ro = bytearray(0x100)                   # 只读区
    data = bytearray(0x200)                 # 数据区（重定位写这里）

    # 符号表：下标 0 是空符号；然后是导出的、再是 import 的
    syms = [b"\0" * 24]
    strtab = bytearray(b"\0")
    names = {}

    def add(name_b, value, shndx):
        off = len(strtab)
        strtab.extend(name_b + b"\0")
        names[name_b.decode()] = len(syms)
        syms.append(struct.pack("<IBBHQQ", off, 0x12 if shndx else 0x10, 0, shndx, value, 0))

    for i, e in enumerate(exports):
        add(e.encode(), 0x100 + i * 8, 1)          # 定义在 ro 段
    for i, im in enumerate(imports):
        add(im.encode(), 0, 0)                     # 未定义 → 走宿主 import

    # 重定位（写在 data 段里，偏移相对映像）
    relocs = []
    data_base_in_img = None                  # 稍后填
    for i in range(2):
        relocs.append((0, R_RELATIVE, 0, 0x1000 + i * 8))          # addend 非 0
    for i, e in enumerate(exports):
        relocs.append((names[e], R_ABS64, 0x20 + i * 8, 8))
    for i, im in enumerate(imports):
        relocs.append((names[im], R_GLOB_DAT, 0x60 + i * 8, 0))
    for i, im in enumerate(imports):
        relocs.append((names[im], R_JUMP_SLOT, 0xA0 + i * 8, 0))
    relocs += list(extra_relocs)

    # ---- 布局 ----
    # 前 0x10 字节留给「ELF 头」（img[4] 放 MOD0 偏移，真实 NSO 就是这么摆的）：
    # 从 0 开始摆会把 MOD0 覆盖掉 —— 自检里报的就是「找不到 MOD0」。
    off = 0x10
    def take(n, align=8):
        nonlocal off
        off = (off + align - 1) & ~(align - 1)
        p = off
        off += n
        return p

    mod0 = take(0x40)
    dyn = take(0x100)
    symtab = take(len(syms) * 24)
    strtab_off = take(len(strtab))
    rela = take(len(relocs) * 24)
    jmprel = take(0)                            # 本例把 JUMP_SLOT 也放进同一张表
    # data（重定位写的地方）从下一个 16K 页起 —— 与真实模块一致（四个 NSO 的 DATA_OFF 都页对齐，
    # 而且都在「圆整16K(TEXT_END)」之后）。原来紧跟在 rela 后面，落在第一页里：那一页现在归代码段
    # （只读+执行，见 smo/ios/tools/x0_layout.py），重定位写不进去 —— 合成产物得先长得像真的。
    data_off = take(len(data), 0x4000)
    img_size = (off + 0xFFF) & ~0xFFF

    img = bytearray(img_size)
    # ELF 头前 8 字节：img[4] = MOD0 偏移（真实 NSO 就是这么放的）
    img[0:4] = b"\x7fELF"
    struct.pack_into("<I", img, 4, mod0)
    img[mod0:mod0 + 4] = b"MOD0"
    struct.pack_into("<i", img, mod0 + 4, dyn - mod0)       # 动态段相对 MOD0
    struct.pack_into("<i", img, mod0 + 0x10, 0)             # eh_frame_hdr 起点（装载器不看）
    struct.pack_into("<i", img, mod0 + 0x14, 0)

    # 动态段
    dyn_items = [(DT["SYMTAB"], symtab), (DT["STRTAB"], strtab_off), (DT["INIT"], 0x100),
                 (DT["INIT_ARRAY"], 0x180), (DT["INIT_ARRAYSZ"], 8),
                 (DT["RELA"], rela), (DT["RELASZ"], len(relocs) * 24),
                 (DT["JMPREL"], rela), (DT["PLTRELSZ"], 0)]
    for i, (t, v) in enumerate(dyn_items):
        struct.pack_into("<qQ", img, dyn + i * 16, t, v)
    struct.pack_into("<qQ", img, dyn + len(dyn_items) * 16, 0, 0)

    img[symtab:symtab + len(syms) * 24] = b"".join(syms)
    img[strtab_off:strtab_off + len(strtab)] = strtab
    # 导出符号的值指向 ro 段（装载器只做加法，不校验内容）
    # 重定位表
    for i, (si, ty, roff, add) in enumerate(relocs):
        struct.pack_into("<QQq", img, rela + i * 24, data_off + roff, (si << 32) | ty, add)

    # 键页：放在 img_size 之后（装载器要求 key_page_off 是 0x4000 的倍数、大小 0x4000）
    key_page_off = (img_size + 0x3FFF) & ~0x3FFF
    patch_off = (key_page_off + 0x4000 + 0xFFFF) & ~0xFFFF
    img.extend(bytes(patch_off + 0x100 - len(img)))

    layout = dict(PATCH_OFF=patch_off, KEY_PAGE_OFF=key_page_off, KEY_PAGE_SIZE=0x4000,
                  TEXT_END=0x200, RO_OFF=0x200, DATA_OFF=data_off)
    return bytes(img), layout


def write_module(outdir, name, exports, imports, ngates=0):
    img, layout = build_module(name, exports, imports)
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, f"{name}.patched.bin"), "wb").write(img)
    with open(os.path.join(outdir, f"{name}.fixups.tsv"), "w", encoding="utf-8") as f:
        for k, v in layout.items():
            f.write(f"# {k}\t{v:#x}\n")
        for i, k in enumerate(KEYS):
            f.write(f"# KEY_{k}_IDX\t{i}\n")
    with open(os.path.join(outdir, f"{name}.gates.tsv"), "w", encoding="utf-8") as f:
        for i in range(ngates):
            # 返回点、种类、参数、原指令地址（返回点必须落在补丁区之后，装载器会加上基址）
            f.write(f"{layout['PATCH_OFF'] + i * 0x40:x}\tsvc\t{0x40 + i}\t0\n")
    return layout


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "work/synthetic"
    # 两个模块：sdk 导出一批符号、main 使用它们（跨模块）+ 使用宿主 import。
    # sdk 的 import 里带四个 **rtld 链接器符号**（__rel_dyn_start/end、__rel_plt_start/end）——
    # 真实产物里 sdk/subsdk0/main 各导入这四个，装载器必须解析成「模块自己的重定位表边界」，
    # 而不是塞给宿主 import（真机第四崩就是它们被绑成宿主桩地址：musl 的 __nnmusl_init_dso
    # 拿这几个参数去做 DSO 初始化，SDK 的运行时等于没初始化）。合成产物里放上，CI 才拦得住回归。
    write_module(out, "sdk", [f"nn3hid{i}" for i in range(6)] + ["nn2fs4OpenEv",
                              "_ZN2nn2os32SetMemoryAllocatorForThreadLocalEPFPvmmEPFvS1_mE"],
                 ["nnsocketBind", "nnsockX", "__rel_dyn_start", "__rel_dyn_end",
                  "__rel_plt_start", "__rel_plt_end"], ngates=3)
    # main 导出 rtld 要拿来接给 nn::os 的两个线程局部存储分配器（真实产物里就是 main 导出）
    write_module(out, "main", ["_ZN2nn4init6detail30DefaultAllocatorForThreadLocalEmm",
                               "_ZN2nn4init6detail32DefaultDeallocatorForThreadLocalEPvm"],
                 ["nn3hid0", "nn2fs4OpenEv", "memcpy", "unknownHostSymbol"], ngates=0)
    print(f"合成产物已写入 {out}")


if __name__ == "__main__":
    sys.exit(main())
