#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""改写前后**动态等价性校验**（Unicorn，不需要真机）。

改写器的自检是**静态**的（断言原字节、逐条规则的形式检查），静态检查证明不了「同一输入下效果一致」。
这里把两侧都跑一遍比较末态：
  · 原指令序列：站点上的 `svc #n`，用**同一个效果模型**代替真正的 svc；
  · 改写后的序列：站点上是 `b <门块>`，门块在补丁区（stp/mrs tpidrro_el0/adrp+ldr 键页槽/ldr/blr/ldp/b 回原处），
    最后 `blr` 到的"宿主函数"用**同一个模型**顶替。
效果模型两边一致，所以它比较的是**改写本身**：门块有没有正确保存/恢复寄存器与栈、有没有回到正确的续点。

比较项：① 续点一致（两边都停在"原指令的下一条"）② x0..x29 一致（**跳过 x18**：门与宿主共用苹果
平台寄存器，见 entry.S）③ SP 一致（门块压栈/出栈必须配平）。

怎么在仿真里表达"宿主门入口"（这一步是跑通的关键，写清楚免得下次重新摸索）：
门块从 `[TPIDRRO_EL0 + 键页槽]` 取宿主门入口指针。仿真里 TPIDRRO_EL0 = 0，于是把键页槽写成 Q = SENT+8，
在 SENT 处放 `ret`、在 Q 处放指向 SENT 的指针 —— 等价于真实装载时"宿主把 dm_gate 的地址填进线程槽"。
另外：**映像映射在 BASE 上，所以写键页槽必须是 BASE + vaddr**（我一开始写成了裸 vaddr，白折腾一轮）。

svc 号必须两边取自同一张「站点 → svc 号」表（对应真实 `dm_gate` 用返回点当门标识去查表）——
否则比较的就变成"我的模型"而不是改写了。

实测（真实产物）：
    sdk  svc 106/106 ✅   cntpct 29/29 ✅      rtld svc 4/4 ✅
main / subsdk0 没有裸 svc（都经 sdk 的 nn::os 包装），所以那里没有 svc 站点。

用法：equiv_check.py <模块名> [--limit N] [--kind svc|cntpct]
kind=cntpct 的额外一步：`mrs xN, cntpct_el0` 的目标寄存器不固定（实测 x20/x21/x23 等，
不在 x0..x8 里），所以模型要知道往哪个寄存器写。目标寄存器从 `gates.tsv` 第 3 列取
（改写器写出来的，第 4 列是站点，已核对与 sites.tsv 一致）。
"""
import os
import struct
import sys

from capstone import CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, Cs
from unicorn import (UC_ARCH_ARM64, UC_HOOK_CODE, UC_HOOK_MEM_FETCH_UNMAPPED,
                     UC_MODE_ARM, Uc, UC_PROT_ALL)
from unicorn.arm64_const import (UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_SP,
                                 UC_ARM64_REG_X0, UC_ARM64_REG_X18)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(ROOT, "work")
BASE = 0x1000                      # 映像基址（与 vaddr 对齐，便于把 vaddr 当 PC 用）
STACK = 0x200000000
SENT = 0x300000000            # 宿主门入口桩（仿真里代表 dm_gate）                # 模拟栈（挑一个远离映像的地址）
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
    # 哨兵页：改写后的门块经字面量池 blr 到宿主 dm_gate，未重定位时那个字段是 0。
    # 直接跳到 0 会在**取指阶段**就失败，代码钩子根本不会被调用（这就是之前 UC_ERR_READ_UNMAPPED 的原因），
    # 所以先把 0 处映射出来，让钩子有机会把「跑到映像外」识别成宿主边界。
    uc.mem_map(SENT, PAGE, UC_PROT_ALL)
    uc.mem_write(SENT, bytes([0xC0, 0x03, 0x5F, 0xD6]))          # P：ret（钩子会先接管）
    uc.mem_write(SENT + 8, struct.pack("<Q", SENT))              # Q：指向 P 的指针
    return uc


def init_regs(uc, site, seed):
    """确定性的初值：同一个 seed 两边完全一样（比较才有意义）。"""
    for i in range(31):
        uc.reg_write(UC_ARM64_REG_X0 + i, (seed * 0x9E3779B97F4A7C15 + i * 0x1234567) & 0xFFFFFFFFFFFFFFFF)
    uc.reg_write(UC_ARM64_REG_SP, STACK - 0x1000)
    uc.reg_write(UC_ARM64_REG_LR, site + 4)          # 模拟"调用者调进这个站点"
    uc.reg_write(UC_ARM64_REG_PC, BASE + site)


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


def svc_no_of(img, site):
    """站点处的 svc 号（从原映像读）。真实实现里 dm_gate 用返回点当门标识去查这张表，
    所以两边都必须用同一个「站点 → svc 号」映射，否则比较的就不是改写而是我的模型了。"""
    w = struct.unpack_from("<I", img, site)[0]
    return (w >> 5) & 0xFFFF


def apply_cntpct_model(uc, reg):
    """计时器门的效果：把一个确定性的计数值写进目标寄存器（两边同一个模型）。"""
    uc.reg_write(UC_ARM64_REG_X0 + reg, 0x1234567890ABCDEF)


def run_original(img, site, seed, expect_reg=None):
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
            apply_svc_model(uc_, svc_no_of(img, site))
            uc_.reg_write(UC_ARM64_REG_PC, addr + 4)
        elif insn.mnemonic == "mrs":
            # 原指令 `mrs xN, cntpct_el0`：用模型代替（否则比的是 Unicorn 的计数器，不确定）。
            # 顺带核对 gates.tsv 记的目标寄存器与指令里的 Rd 是否一致 —— 一致才说明改写器的表是对的。
            rd_name = insn.reg_name(insn.operands[0].reg) if insn.operands else ''
            rd = int(rd_name[1:]) if rd_name.startswith('x') and rd_name[1:].isdigit() else -1
            if expect_reg is not None:
                assert rd == expect_reg, f"0x{site:x}: gates.tsv 记的是 x{expect_reg}，指令里是 x{rd}"
            apply_cntpct_model(uc_, rd)
            uc_.reg_write(UC_ARM64_REG_PC, addr + 4)
        state["done"] = True                          # 站点只跑这一条

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(BASE + site, BASE + site + 4, count=1)
    return snapshot(uc), uc.reg_read(UC_ARM64_REG_PC) - BASE


def run_rewritten(img, site, seed, img_len, key_page, key_slot, svc_no, kind, dst_reg):
    uc = make_uc(img)
    # 门块从 [TPIDRRO_EL0 + 键页槽] 取宿主门入口指针。仿真里 TPIDRRO_EL0 = 0，
    # 所以把槽值写成 Q（= SENT+8），于是：ldr x30,[Q] = P，blr x30 → PC = P → 钩子接管。
    # 这一步对应真实装载时"宿主把 dm_gate 的地址填进线程槽"。
    uc.mem_write(BASE + key_page + key_slot, struct.pack("<Q", SENT + 8))   # 映像映射在 BASE 上，写的是虚拟地址 + BASE
    init_regs(uc, site, seed)
    state = {"host_calls": 0, "steps": 0}

    def hook(uc_, addr, size, _):
        state["steps"] += 1
        if state["steps"] > 4096:
            uc_.emu_stop()
            return
        # 宿主门入口桩：这里代表 dm_gate
        if addr == SENT:
            state["host_calls"] += 1
            if kind == "cntpct":
                apply_cntpct_model(uc_, dst_reg)        # 同一个模型、同一个目标寄存器
            else:
                apply_svc_model(uc_, svc_no)            # 与原指令序列同一个模型、同一个 svc 号
            uc_.reg_write(UC_ARM64_REG_PC, uc_.reg_read(UC_ARM64_REG_LR))
            return
        if (addr - BASE) == site + 4:                 # 回到续点
            uc_.emu_stop()

    def unmapped(uc_, access, address, size, value, _):
        # 取指跑到映像外 = 宿主的 dm_gate 入口（未重定位的 patched.bin 里那个字面量不是有效地址）。
        # 现场补一页 ret 并让它重试：ret 会回到 LR，也就是门块的续点 —— 于是门块自己的
        # 保存/恢复/续点逻辑照常执行（这正是我们要验的东西）。
        state["host_calls"] += 1
        apply_svc_model(uc_, svc_no)
        page = address & ~(PAGE - 1)
        try:
            uc_.mem_map(page, PAGE, UC_PROT_ALL)
        except Exception:
            pass
        uc_.mem_write(page, bytes([0xC0, 0x03, 0x5F, 0xD6]))    # ret

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED, unmapped)
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
    all_sites = read_sites(mod, kind)
    sites = all_sites[:limit]
    if not sites:
        print(f"🔴 {mod}.sites.tsv 里没有 kind={kind} 的站点")
        return 2
    hdr = {}
    for line in open(os.path.join(WORK, "rewrite", f"{mod}.fixups.tsv"), encoding="utf-8"):
        if not line.startswith("#"):
            break
        q = line[1:].strip().split()
        if len(q) == 2:
            hdr[q[0]] = q[1]
    key_page = int(hdr["KEY_PAGE_OFF"], 16)
    key_slot = 0x18              # 门块读的是 [x29, #0x18]（见 0xa2dd8c 处的反汇编）
    print(f"# {mod}: {kind} 站点 {len(sites)} 个（共 {len(all_sites)}），映像 {len(orig)} → 改写后 {len(new)}")
    print(f"# 键页 0x{key_page:x}，槽 +0x{key_slot:x}（仿真里填成宿主门入口桩）")

    bad = 0
    # cntpct 的目标寄存器在 gates.tsv 第 3 列、门块在第 1 列（第 4 列是站点，已核对与 sites.tsv 一致）。
    # 用 chr(9) 而不是字面的制表符：这个文件被反复用 heredoc 编辑过，字面量容易被吃掉。
    regs = {}
    with open(os.path.join(WORK, "rewrite", f"{mod}.gates.tsv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            q = line.rstrip().split(chr(9))
            if len(q) >= 4:
                regs[(int(q[3], 16), q[1])] = (int(q[2]), int(q[0], 16))

    for i, sv in enumerate(sites):
        # read_sites 有的版本返回 int（只有站点）、有的返回 (站点, 目标寄存器, 门块)
        site, dst_reg, gate = sv if isinstance(sv, tuple) else (sv, None, None)
        if dst_reg is None:
            r = regs.get((site, kind))
            if r:
                dst_reg, gate = r
        if kind == "cntpct" and dst_reg is None:
            print(f"🔴 需要 gates.tsv 提供目标寄存器但没拿到（站点 0x{site:x}）—— cntpct 先跳过")
            return 2
        seed = 1234 + i
        a, pc_a = run_original(orig, site, seed, dst_reg)
        b, pc_b, hc = run_rewritten(new, site, seed, len(new), key_page, key_slot,
                                    svc_no_of(orig, site) if kind == "svc" else 0, kind, dst_reg)
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
