#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""nvn_opaque_check 的自检：确认它真的能抓到反例，而不是「永远报 0」。

为什么需要这个：一个只会说 ✅ 的检查没有价值。本自检用手工汇编的指令序列喂进
被测函数，断言每种形状都被判成预期的类别 —— 包括一条**真正违规**的（读 NVN 对象内部）。

不需要游戏文件，不需要 Metal。用法：
  python smo/verify/nvn_opaque_selftest.py
"""
import os
import struct
import sys

import capstone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nvn_opaque_check as C                                        # noqa: E402

MD = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)

FAILED = []
PASSED = 0


def check(label, got, want):
    global PASSED
    if got == want:
        PASSED += 1
    else:
        FAILED.append(f"{label}: 得到 {got!r}，期望 {want!r}")


def asm(*words):
    """把一串 32 位指令编码拼成 bytes。"""
    return struct.pack(f"<{len(words)}I", *words)


# ---- 手工编码几条 aarch64 指令（只要被测代码认得的那几种形状）----------------

def ldr_imm(rt, rn, off):            # ldr Xt, [Xn, #off]
    return 0xF9400000 | ((off // 8) << 10) | (rn << 5) | rt


def str_imm(rt, rn, off):            # str Xt, [Xn, #off]
    return 0xF9000000 | ((off // 8) << 10) | (rn << 5) | rt


def add_imm(rd, rn, imm):            # add Xd, Xn, #imm
    return 0x91000000 | (imm << 10) | (rn << 5) | rd


def mov_reg(rd, rm):                 # mov Xd, Xm  (= orr Xd, xzr, Xm)
    return 0xAA0003E0 | (rm << 16) | rd


def blr(rn):                         # blr Xn
    return 0xD63F0000 | (rn << 5)


def nop():
    return 0xD503201F


def run_case(words, call_word_idx):
    """跑一遍「回溯 + 窗口裁剪 + 带偏移访问分流」，返回 (trace_kind, [判定...])。

    复刻 nvn_opaque_check.main 里对单个调用点的处理，但对象类型固定成
    NVNbuffer(0x30) —— 自检关心的是分流逻辑，不是名字到类型的映射。
    """
    insns = list(MD.disasm(asm(*words), 0x1000))
    ci = next(i for i, x in enumerate(insns) if x.address == 0x1000 + call_word_idx * 4)
    kind, reg, basis, win_lo = C.trace_obj_reg(insns, ci)
    if kind in (C.TRACE_STACK, C.TRACE_OPAQUE):
        return kind, []
    w0, w1 = C.clamp_to_def(insns, win_lo, ci + C.WIN_AFTER + 1, reg, ci)
    size = 0x30
    out = []
    for _addr, _text, imm, acc in C.struct_access(insns, w0, w1, reg, ci):
        rel = imm - basis
        if acc == "fetch":
            out.append("fetch")
        elif rel < 0:
            out.append("before")
        elif rel >= size:
            out.append("outside")
        else:
            out.append("INSIDE")
    return kind, out


# ---- 用例 --------------------------------------------------------------------
# 寄存器编号：x0=0, x8=8, x19=19, x23=23, sp=31(在 add/mov 里)

# ① 真正的反例：对象在 x19，调用后读 x19+0x8（NVNbuffer 大小 0x30，0x8 在内部）
kind, verdicts = run_case([
    mov_reg(0, 19),          # mov x0, x19     ← 对象指针
    blr(8),                  # blr x8          ← 调用点 (idx 1)
    ldr_imm(9, 19, 0x8),     # ldr x9, [x19,#8]  ← 读 NVN 对象内部！
], 1)
check("反例被抓到: trace", kind, C.TRACE_REG)
check("反例被抓到: 判定", verdicts, ["INSIDE"])

# ② 内嵌子对象：对象在 x19+0x48，读 x19+0x50 ⇒ rel=0x8，仍在对象内 ⇒ INSIDE
kind, verdicts = run_case([
    add_imm(0, 19, 0x48),    # add x0, x19, #0x48
    blr(8),
    ldr_imm(9, 19, 0x50),    # rel = 0x50-0x48 = 0x8 < 0x30
], 1)
check("内嵌子对象内部: trace", kind, C.TRACE_BASEOFF)
check("内嵌子对象内部: 判定", verdicts, ["INSIDE"])

# ③ 内嵌子对象，读到对象之后：x19+0x48 是对象，读 x19+0x80 ⇒ rel=0x38 >= 0x30 ⇒ outside
kind, verdicts = run_case([
    add_imm(0, 19, 0x48),
    blr(8),
    ldr_imm(9, 19, 0x80),
], 1)
check("内嵌子对象之后: 判定", verdicts, ["outside"])

# ④ 内嵌子对象，读到对象之前：读 x19+0x10 ⇒ rel 为负 ⇒ before（AGL 自己的字段）
kind, verdicts = run_case([
    add_imm(0, 19, 0x48),
    blr(8),
    ldr_imm(9, 19, 0x10),
], 1)
check("内嵌子对象之前: 判定", verdicts, ["before"])

# ⑤ `ldr x0,[x0,#0xb8]`：从 AGL 包装对象里取出 NVN 指针。
#    取出来之后我们**没有**任何寄存器持有「NVN 对象起点」的已知基址 ——
#    x0 就是对象本身，但它的来源表达式 [x0+0xb8] 追不动。如实算盲区。
#    （实测形状见 0xae7dcc: ldr x0,[x0,#0xb8] ; blr x8 → nvnCommandBufferSetRenderTargets）
kind, verdicts = run_case([
    ldr_imm(0, 0, 0xb8),     # ldr x0, [x0,#0xb8]
    blr(8),
], 1)
check("取指针后基址追不动 → 盲区", kind, C.TRACE_OPAQUE)

# ⑥ fetch：对象在 x19，而 x19 本身是在窗口里由 `ldr x19,[x19,#0xc0]` 定义的。
#    窗口被裁到这条定义处，于是这条定义自己落在窗口内 —— 它是「取 NVN 指针」，
#    不是「读 NVN 对象字段」，必须判成 fetch 而不是 INSIDE。
#    （实测形状见 0x77f1f4: ldr x19,[x19,#0xc0] ; mov x0,x19 ; blr → BindProgram）
kind, verdicts = run_case([
    ldr_imm(19, 19, 0xc0),   # ldr x19, [x19,#0xc0]   ← 取出 NVN 指针
    mov_reg(0, 19),          # mov x0, x19
    blr(8),                  # idx 2
], 2)
check("fetch: trace", kind, C.TRACE_REG)
check("fetch: 判定", verdicts, ["fetch"])

# ⑦ 栈上临时对象（builder）：跳过，不参与判定
kind, verdicts = run_case([
    add_imm(0, 31, 0x8),     # add x0, sp, #8
    blr(8),
    ldr_imm(9, 31, 0x8),
], 1)
check("栈上对象被跳过", kind, C.TRACE_STACK)

# ⑧ 指针从内存取出、基址追不动：ldr x0,[x9] ⇒ OPAQUE（盲区，如实计入）
kind, verdicts = run_case([
    ldr_imm(0, 9, 0x0),      # ldr x0, [x9]
    blr(8),
], 1)
check("追不动的算盲区", kind, C.TRACE_OPAQUE)

# ⑩ 窗口裁剪：调用前 reg 被重新定义，旧值时期的访问不该算进来
#    （这就是实测 0xa0b9b4 那个假阳性的形状）
kind, verdicts = run_case([
    ldr_imm(8, 23, 0x50),    # ldr x8, [x23,#0x50]   ← 这时 x23 是别的东西
    ldr_imm(23, 8, 0x0),     # ldr x23, [x8]         ← x23 在这里被重新定义
    add_imm(0, 23, 0x48),    # add x0, x23, #0x48
    blr(8),                  # idx 3
], 3)
check("窗口裁剪(调用前)", verdicts, [])

# ⑩ 窗口裁剪：调用后 reg 被重新定义，之后的访问也不该算
kind, verdicts = run_case([
    mov_reg(0, 19),
    blr(8),                  # idx 1
    mov_reg(19, 8),          # mov x19, x8   ← x19 换人了
    ldr_imm(9, 19, 0x8),     # 这条针对新值，不该算
], 1)
check("窗口裁剪(调用后)", verdicts, [])

# ⑪ str 不算定义：`str x0,[x19]` 是读 x0，回溯不该停在它上面
kind, verdicts = run_case([
    mov_reg(0, 19),
    str_imm(0, 19, 0x0),     # str x0, [x19]  —— 读 x0，不是定义 x0
    blr(8),                  # idx 2
    ldr_imm(9, 19, 0x8),
], 2)
check("str 不算定义: trace", kind, C.TRACE_REG)
check("str 不算定义: 判定", verdicts, ["INSIDE"])

# ⑫ 偏移 0 不算读字段（解引用指针本身）
kind, verdicts = run_case([
    mov_reg(0, 19),
    blr(8),
    ldr_imm(9, 19, 0x0),     # ldr x9,[x19]  —— imm==0，不计
], 1)
check("偏移 0 不计", verdicts, [])

# ---- 结果 --------------------------------------------------------------------
print(f"# nvn_opaque_check 自检：{PASSED} 条断言")
for f in FAILED:
    print(f"  FAIL  {f}")
if FAILED:
    print(f"{len(FAILED)} 条失败")
    sys.exit(1)
print(f"全部通过（{PASSED}/{PASSED}）—— 检查器能抓到反例，也能分流掉已知的假阳性形状。")
