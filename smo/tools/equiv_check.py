#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""改写前后**动态等价性校验**（用 Unicorn 跑，不需要真机）。

# ⚠ 半成品（本轮到此，尚未接进 CI / test）

**已经能跑的**：原始一侧 —— 在站点上执行原指令（`svc #n`）并用同一个效果模型代替真正的 svc，
归档对比 x0..x29（跳 x18）与 SP，并确认续点是「原指令的下一条」。

**还没跑通的**：改写后一侧。站点是 `b <门块>`，门块在补丁区，最后通过一个字面量池里的地址
`blr` 到宿主的 `dm_gate`；那个字段在**未重定位**的 patched.bin 里是 0，所以模型里要把「PC 跑到映像外」
当成宿主边界。现在是 `UC_ERR_READ_UNMAPPED`（推测是取指/读字面量落在了没映射的地方），
还没定位到具体是哪一条、哪个地址 —— 这是下次接着做的第一件事。

所以：**目前不要引用这个工具的任何结论**。它存在的意义是把方法与站点地址定下来，
以及把「比较什么」这件事想清楚（下面三条）。

为什么值得有这一层：改写器的自检是**静态**的（断言原字节、逐条规则的形式检查）。静态检查证明不了
「同一输入下效果一致」。这里用 Unicorn 把两边都跑一遍，比较末态：
  · 原指令序列（例如 `svc #n`）—— 用**同一个效果模型**代替真正的 svc；
  · 改写后的序列（例如 `b <门块>`，门块里 `bl dm_gate` 之后 `ret` 回原处）—— 那个“宿主函数调用”
    同样用效果模型顶替。
于是检验的是**改写本身**：门块有没有正确保存/恢复寄存器与栈、有没有回到正确的续点、
有没有把入参摆对位置。效果模型两边一致，所以它不会掩盖改写的差异。

校验项（对站点逐条比较）：
  1. 续点一致：两边都必须停在“原指令的下一条”
  2. x0..x29 一致（**跳过 x18**：门与宿主共用苹果平台寄存器，见 entry.S 的说明）
  3. SP 一致（门块压栈/出栈必须配平 —— 不配平在真机上就是返回时 SP 错位）

用法：
  equiv_check.py <模块名> [--limit N] [--kind svc]
读 work/game/<模>.bin 与 work/rewrite/<模>.patched.bin 与 <模>.sites.tsv。
"""
import os
import struct
import sys

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs
from unicorn import (UC_ARCH_ARM64, UC_HOOK_CODE, UC_MODE_ARM, Uc, UC_PROT_ALL)
from unicorn.arm64_const import (UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_SP,
                                 UC_ARM64_REG_X0, UC_ARM64_REG_X18)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(ROOT, "work")
BASE = 0x1000                      # 映像基址（与 vaddr 对齐，便于把 vaddr 当 PC 用）
STACK = 0x200000000                # 模拟栈（挑一个远离映像的地址）
STACK_SZ = 0x100000
PAGE = 0x1000


def align(x):
    return (x + PAGE - 1) & ~(PAGE - 1)


def read_sites(mod, kind):
    out = []
    with open(os.path.join(WORK, "rewrite", f"{mod}.sites.tsv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            p = line.rstrip("\n").split("\t")
            if len(p) >= 2 and p[1] == kind:
                out.append(int(p[0], 16))
    return out


def make_uc(img):
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    # 多映射 4 MB 空位：门块/补丁区可能落在映像尾之后，且键页等数据引用也在这一带
    uc.mem_map(BASE, align(len(img)) + 0x400000, UC_PROT_ALL)
    uc.mem_write(BASE, img)
    uc.mem_map(STACK - STACK_SZ, STACK_SZ, UC_PROT_ALL)
    return uc


def init_regs(uc, site, seed):
    """确定性的初值：同一个 seed 两边完全一样（比较才有意义）。"""
    for i in range(31):
        uc.reg_write(UC_ARM64_REG_X0 + i, (seed * 0x9E3779B97F4A7C15 + i * 0x1234567) & 0xFFFFFFFFFFFFFFFF)
    uc.reg_write(UC_ARM64_REG_SP, STACK - 0x1000)
    uc.reg_write(UC_ARM64_REG_LR, site + 4)          # 模拟"调用者调进这个站点"
    uc.reg_write(UC_ARM64_REG_PC, site)


def snapshot(uc):
    d = {}
    for i in range(31):
        if i == 18:
            continue                                  # x18 是苹果平台寄存器，门不回读它
        d[f"x{i}"] = uc.reg_read(UC_ARM64_REG_X0 + i)
    d["sp"] = uc.reg_read(UC_ARM64_REG_SP)
    return d


# ---- 效果模型：svc 对 x0..x8 的作用（两边用同一个，所以比较的是"改写"而不是"模型"）----
def apply_svc_model(uc, svc_no):
    for i in range(9):
        v = uc.reg_read(UC_ARM64_REG_X0 + i)
        uc.reg_write(UC_ARM64_REG_X0 + i, (v ^ (svc_no * 0x100000001B3)) & 0xFFFFFFFFFFFFFFFF)


def run_original(img, site, seed):
    uc = make_uc(img)
    init_regs(uc, site, seed)
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    md.detail = True          # 读 svc 的立即数需要 detail
    state = {"done": False}

    def hook(uc_, addr, size, _):
        if state["done"]:
            return
        insn = next(md.disasm(bytes(uc_.mem_read(addr, 4)), addr), None)
        if insn is None:
            return
        if insn.mnemonic == "svc":
            apply_svc_model(uc_, insn.operands[0].imm if insn.operands else 0)
            uc_.reg_write(UC_ARM64_REG_PC, addr + 4)
        state["done"] = True                          # 站点只跑这一条

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(BASE + site, BASE + site + 4, count=1)
    return snapshot(uc), uc.reg_read(UC_ARM64_REG_PC) - BASE


def run_rewritten(img, site, seed, img_len):
    uc = make_uc(img)
    init_regs(uc, site, seed)
    state = {"host_calls": 0, "steps": 0}

    def hook(uc_, addr, size, _):
        state["steps"] += 1
        if state["steps"] > 4096:
            uc_.emu_stop()
            return
        # "宿主函数"的落点：fixup 之后指向宿主，未装载时就是一个映像外的地址
        if addr < BASE or addr >= BASE + img_len:
            state["host_calls"] += 1
            apply_svc_model(uc_, 0)                   # 与原指令序列同一个模型
            uc_.reg_write(UC_ARM64_REG_PC, uc_.reg_read(UC_ARM64_REG_LR))
            return
        if (addr - BASE) == site + 4:                 # 回到续点
            uc_.emu_stop()

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(BASE + site, 0, count=100000, timeout=2 * 1000 * 1000)
    return snapshot(uc), uc.reg_read(UC_ARM64_REG_PC) - BASE, state["host_calls"]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    mod = sys.argv[1]
    kind = sys.argv[sys.argv.index("--kind") + 1] if "--kind" in sys.argv else "svc"
    limit = int(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else 10
    orig = open(os.path.join(WORK, "game", f"{mod}.bin"), "rb").read()
    new = open(os.path.join(WORK, "rewrite", f"{mod}.patched.bin"), "rb").read()
    sites = read_sites(mod, kind)[:limit]
    if not sites:
        print(f"🔴 {mod}.sites.tsv 里没有 kind={kind} 的站点")
        return 2
    print(f"# {mod}: {kind} 站点 {len(sites)} 个（共 {len(read_sites(mod, kind))}），映像 {len(orig)} → 改写后 {len(new)}")

    bad = 0
    for i, site in enumerate(sites):
        seed = 1234 + i
        a, pc_a = run_original(orig, site, seed)
        b, pc_b, hc = run_rewritten(new, site, seed, len(new))
        diffs = [k for k in a if a[k] != b[k]]
        ok = (pc_a == site + 4) and (pc_b == site + 4) and not diffs
        if not ok:
            bad += 1
            print(f"🔴 0x{site:x}: 续点 原 0x{pc_a:x} / 新 0x{pc_b:x}；"
                  f"不一致的寄存器 {diffs[:6]}{' …' if len(diffs) > 6 else ''}（宿主调用 {hc} 次）")
            if diffs:
                k = diffs[0]
                print(f"     {k}: 原 0x{a[k]:x} 新 0x{b[k]:x}")
        else:
            print(f"  ✅ 0x{site:x}: 续点一致、x0..x29（除 x18）与 SP 一致（宿主调用 {hc} 次）")
    print(f"\n{'有 ' + str(bad) + ' 个站点不等价' if bad else '全部等价'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
