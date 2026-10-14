#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把摊平映像包成最小 ELF，然后交给 LLVM 反汇编 —— 手写反汇编只适合做模式匹配，读代码还得靠真反汇编器。

背景：`nso_info.py --dump` 出来的是「按 vaddr 铺平」的映像（偏移 == 虚拟地址），不是 ELF，
所以 `llvm-objdump -d` 认不出来（会报 coff-<unknown arch>）。这里补一个只有程序头 + 两个段的
最小 ELF：段地址就是 vaddr，于是反汇编器打印的地址和游戏里的地址一一对应。

用法：
  rawdis.py <模块名> --at 0xADDR [--lines N]     反汇编某处附近
  rawdis.py <模块名> --func 0xADDR [--lines N]   同上（语义化写法）
  rawdis.py <模块名> --ref 0xADDR                找「谁把 0xADDR 这个地址装进寄存器」（adrp/add 对）
输出为 LLVM 反汇编文本。

环境：LLVM 在 $SMO_LLVM 或 C:/toolchain/llvm/LLVM/bin（可用 SMO_OBJDUMP 指定 llvm-objdump 路径）。
"""
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(ROOT, "work", "game")

OBJDUMP = os.environ.get("SMO_OBJDUMP") or os.path.join(
    os.environ.get("SMO_LLVM", "C:/toolchain/llvm/LLVM"), "bin",
    "llvm-objdump.exe" if os.name == "nt" else "llvm-objdump")


def wrap(img: bytes, text_end: int, out: str) -> None:
    """一个 PT_LOAD（RWX，vaddr=0）+ .text/.img 两个段 + .shstrtab。"""
    base = 0x1000
    shstr = b"\x00.text\x00.img\x00.shstrtab\x00"
    off_text, off_img = 1, 7
    off_shstr = 15

    ehsize, phentsize, shentsize = 64, 56, 64
    phoff = ehsize
    nph = 1
    body = base
    shstr_off = body + len(img)
    shoff = shstr_off + len(shstr)
    shnum = 3

    ehdr = (b"\x7fELF\x02\x01\x01" + b"\x00" * 9 +
            struct.pack("<HHIQQQIHHHHHH",
                        2, 183, 1, 0, phoff, shoff, 0,
                        ehsize, phentsize, nph, shentsize, shnum, 2))
    phdr = struct.pack("<IIQQQQQQ", 1, 7, 0, 0, 0, len(img), len(img), 0x1000)

    def shdr(name, typ, flags, addr, off, size, align=0x1000):
        return struct.pack("<IIQQQQIIQQ", name, typ, flags, addr, off, size, 0, 0, align, 0)

    shdrs = (shdr(off_text, 1, 0x6, 0, body, text_end) +
             shdr(off_img, 1, 0x2, text_end, body + text_end, len(img) - text_end) +
             shdr(off_shstr, 3, 0, 0, shstr_off, len(shstr), 1))

    buf = ehdr + phdr
    buf += b"\x00" * (base - len(buf))
    buf += img + shstr + shdrs
    open(out, "wb").write(buf)


def elf_for(mod: str) -> str:
    img = open(os.path.join(WORK, mod + ".bin"), "rb").read()
    # .text 尾：dump 出来的映像头 0x18 字节不是 NSO 头，所以回原 NSO 读
    nso = open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20)
    text_end = struct.unpack_from("<I", nso, 0x18)[0]
    out = os.path.join(ROOT, "work", f"dis_{mod}.elf")
    wrap(img, text_end, out)
    return out


def objdump(elf: str, lo: int, hi: int) -> str:
    r = subprocess.run([OBJDUMP, "-d", "--no-show-raw-insn",
                        f"--start-address={lo}", f"--stop-address={hi}", elf],
                       capture_output=True, text=True)
    return r.stdout or r.stderr


def refs(img: bytes, text_end: int, target: int):
    """找 adrp+add（或 adrp+ldr）把 target 装进寄存器的位置。"""
    out = []
    n = text_end // 4
    w = struct.unpack_from(f"<{n}I", img, 0)
    for i in range(n - 1):
        a0, a1 = w[i], w[i + 1]
        if (a0 & 0x9F000000) != 0x90000000:          # adrp
            continue
        rd = a0 & 0x1F
        page = ((i * 4) & ~0xFFF) + ((((a0 >> 5) & 0x7FFFF) << 2 | ((a0 >> 29) & 3)) << 12)
        if page != (target & ~0xFFF):
            continue
        imm = (a1 >> 10) & 0xFFF if (a1 & 0x7F800000) == 0x11000000 else None
        if imm is None:
            continue
        if ((a1 >> 5) & 0x1F) != rd:
            continue
        if page + ((imm << 12) >> 12) == target or page + (imm if (a1 >> 22) & 1 else imm << 12) == target:
            out.append((i * 4, page + (imm if (a1 >> 22) & 1 else imm << 12)))
    return out


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    mod, what, addr = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
    lines = 24
    if "--lines" in sys.argv:
        lines = int(sys.argv[sys.argv.index("--lines") + 1])
    elf = elf_for(mod)

    if what in ("--ref", "--refs"):
        img = open(os.path.join(WORK, mod + ".bin"), "rb").read()
        nso = open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20)
        text_end = struct.unpack_from("<I", nso, 0x18)[0]
        rs = refs(img, text_end, addr)
        print(f"# 引用 0x{addr:x} 的位置 {len(rs)} 处")
        for site, val in rs[:20]:
            print(f"# 0x{site:x} (值 0x{val:x})")
            print(objdump(elf, site, site + 16))
        return 0

    print(objdump(elf, addr, addr + lines * 4))
    return 0


if __name__ == "__main__":
    sys.exit(main())
