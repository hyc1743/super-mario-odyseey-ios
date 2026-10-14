#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""不变量检查：改写后的客体模块里，**不能再有指令写 x18**。

为什么这条必须成立（entry.S 的前提）：宿主与客体共用苹果的 x18（平台寄存器）。
门的入口保存了 x18 但**不回读**（见 entry.S 的说明），也就是「宿主调用期间 x18 保持不变」。
如果客体代码自己去写 x18，就等于踩掉苹果运行时的平台寄存器；而客体要读 x18 时，
改写器已经把它换成线程槽 K18 的访问了（见 port_rewriter.py 头部的规则表）。

注意别把 SIMD 寄存器数错：`s18/d18/v18` 与 `x18` 的编号字段相同，但只有**通用** x18 算。
所以这里用 capstone 的 regs_access()（它区分寄存器类），而不是自己抠位。

用法：check_x18.py [目录]     默认 work/rewrite，扫 *.patched.bin
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs                                    # noqa: E402
from capstone.arm64 import ARM64_REG_X18                                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def main() -> int:
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "work", "rewrite")
    files = sorted(glob.glob(os.path.join(d, "*.patched.bin")))
    if not files:
        print(f"没找到 {d}/*.patched.bin（先跑 patch_nso.py）")
        return 2
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    md.detail = True                       # regs_access() 需要 detail
    bad_total = 0
    for f in files:
        img = open(f, "rb").read()
        # 只扫代码段：数据区会被解成杂指令，凭空造出「写 x18」的假阳性。
        # 代码段长度回原 NSO 头读（改写产物是按 vaddr 铺平的映像）。
        mod = os.path.basename(f).split(".")[0]
        nso_path = os.path.join(ROOT, "work", "game", "ExeFS", mod)
        text_end = len(img)
        if os.path.exists(nso_path):
            import struct
            text_end = struct.unpack_from("<I", open(nso_path, "rb").read(0x20), 0x18)[0]
        bad = []
        for insn in md.disasm(img[:text_end], 0):
            _, writes = insn.regs_access()
            if ARM64_REG_X18 in writes:
                bad.append((insn.address, insn.mnemonic, insn.op_str))
        name = os.path.basename(f)
        if bad:
            print(f"🔴 {name}: 有 {len(bad)} 条指令写通用 x18 —— 门的前提被破坏")
            for a, m, o in bad[:10]:
                print(f"     0x{a:x}  {m} {o}")
            bad_total += len(bad)
        else:
            print(f"✅ {name}: 没有指令写通用 x18（SIMD s18/d18 不在此列）")
    if bad_total:
        print(f"\n共 {bad_total} 条。需要：要么让改写器把这些写也改成线程槽，"
              f"要么在 entry.S 的门的出口把客体 x18 回读回去。")
        return 1
    print("\n不变量成立：宿主可以与客体共用 x18。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
