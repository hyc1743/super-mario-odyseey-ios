#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从参考项目 d2r-mobile 的 dm/tools/patch_nso.py 生成 SMO 版 smo/tools/patch_nso.py。

做法：只搬运**通用机制**（NSO 装载、键页、编码器、site_* 改写块、窗口、主扫描循环、输出），
去掉 D2R 专属的 CUTS / OBS / PARAMS / touch-run-v4 规则块，再插入 SMO 的跳转表形状支持。
生成出来的文件是自包含的，之后由本仓自己维护。
"""
import os
import re
import sys

# 参考项目的 patch_nso.py（MIT）。用环境变量指向你自己的检出，例如：
#   DM_REF=/path/to/d2r-mobile/dm/tools/patch_nso.py python3 smo/tools/port_rewriter.py
REF = os.environ.get("DM_REF", "d2r-mobile/dm/tools/patch_nso.py")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patch_nso.py")

L = open(REF, encoding="utf-8").read().split("\n")


def seg(a, b):          # 1 基闭区间
    return "\n".join(L[a - 1:b])


DOC = '''# SMO 版 NSO 改写器：Switch aarch64 模块 → 可在苹果系统原生执行的映像。
# 派生自参考项目 d2r-mobile 的 dm/tools/patch_nso.py（MIT），只保留通用改写机制，去掉其 D2R 专属规则表；
# 本文件由 smo/tools/port_rewriter.py 从上游生成后自行维护。
#
# 通用改写（苹果内核会改 x18 与 tpidr_el0；Horizon 的线程区、svc、计时器在苹果上没有对应物）：
#   x18 读写 → 线程槽 K18（TSD，经 tpidrro_el0）      mrs/msr tpidr_el0 → 线程槽 KTLS
#   mrs tpidrro_el0 → 线程槽 KTLR                     svc #n、mrs cntpct_el0/cntvct_el0 → 宿主门
#   已登记剪口（CUTS，按 NSO 构建号限定）：改写前逐条断言原字节，不符即中止
#   已登记观测门（OBS，按构建号限定）：入口一条指令换成「进宿主观测回调 → 执行原指令 → 跳回」
#
# SMO 新增（上游没有的形状）：x18 跳转表。SMO 的 sdk / subsdk0 里有 6 处
#     ldrsw xR,[xB, xIdx, lsl#2] ; add x18, xR, xB ; br x18        （xB = adrp+add 常量）
# 上游只认「adr x18 → 表 + ldrb 索引 + add x18,x18,W,lsl#2」的形状（D2R 的编译器这么生成），
# 对 SMO 这种「表里存 4 字节有符号偏移、目标 = 表基址 + 偏移」的形状会断言失败。
# 这里的处理：改写期把整张表读出来（表在 rodata 里是常量），把 br x18 换成
# 「按 x18 槽里的目标值分发」的比较链，每条目标一条直接分支垫片，到达目标前恢复借用的 x16/x17。
# 这样不依赖索引寄存器、也不改表，且任何寄存器都不会带着被改过的值到达目标。
#
# 用法：patch_nso.py <NSO 文件> <输出目录> <模块名>
'''

CUTS_STUB = '''
# ---- 已登记剪口：{NSO 构建号: [(偏移, 原字节, 新字节, 理由, 作用域)]} ----
# 上游这张表是 D2R 专属的（含 7 条联网离线剪口与一批宿主功能落点），本仓一律不搬。
# SMO 的剪口要按 smo 自己的构建号登记，每条都要断言原字节并写清理由。
CUTS = {}

# ---- 已登记观测门：{NSO 构建号: [(偏移, 编号, 原指令字, 理由)]} ----
OBS = {}
'''

JT_CODE = '''
# ---- SMO 形状的 x18 跳转表 ---------------------------------------------------
# 检测：br x18 前一条是 add x18, xR, xB，且 xB 由 adrp+add 变成常量、表在 rodata 内；
# 索引范围由更前面的 cmp w18, #N 决定（原版随后紧跟 b.hi 越界守卫）。
# 得到目标表后，把 br x18 换成比较链分发（见 site_jt_chain）。
def mov64(reg, v):
    """movz/movk 装 64 位常量，返回指令字列表。"""
    v &= (1 << 64) - 1
    ch = [(v >> (16 * i)) & 0xFFFF for i in range(4)]
    first = next((i for i in range(4) if ch[i]), 0)
    out = [0xD2800000 | (first << 21) | (ch[first] << 5) | reg]
    for i in range(4):
        if i != first and ch[i]:
            out.append(0xF2800000 | (i << 21) | (ch[i] << 5) | reg)
    return out


def CMP_X(rn, rm):          # cmp rn, rm  ==  subs xzr, rn, rm
    return 0xEB000000 | (rm << 16) | (rn << 5) | 31


def B_COND(pc, to, cond):   # b.<cond> 到 to（cond=0 = EQ）
    off = (to - pc) >> 2
    assert -(1 << 18) <= off < (1 << 18), f'条件分支越界 {pc:#x} -> {to:#x}'
    return 0x54000000 | ((off & 0x7FFFF) << 5) | cond


def jt_smo_shape(insl, j):
    """识别 SMO 形状，返回 (目标列表, 说明)；不匹配返回 None。"""
    from capstone.arm64 import ARM64_OP_IMM, ARM64_OP_MEM
    # add x18, xR, xB 不一定紧挨着 br（subsdk0 0x1a8884 与 br 之间还夹着两条 mov），往前找 8 条
    f = fk = None
    for k in range(j - 1, max(0, j - 8), -1):
        p = insl[k]
        if p.mnemonic == 'add' and p.op_str.startswith('x18,'):
            srcs = [p.reg_name(o.reg) for o in p.operands[1:]]
            if len(srcs) >= 2 and srcs[-1] not in ('x18', ''):
                f, fk = p, k
                break
    if f is None:
        return None
    basereg = [f.reg_name(o.reg) for o in f.operands[1:]][-1]
    # 表基址必须是 adrp+add 出来的常量
    page = off_ = None
    for k in range(fk - 1, max(0, j - 64), -1):
        p = insl[k]
        if p.mnemonic == 'add' and p.op_str.startswith(basereg + ', ' + basereg + ','):
            imm = p.operands[2].imm
            if len(p.operands) > 3 and p.operands[3].imm:
                imm <<= p.operands[3].imm
            off_ = imm
        elif p.mnemonic == 'adrp' and p.op_str.startswith(basereg + ','):
            page = p.operands[1].imm
            if off_ is not None:
                break
    if page is None or off_ is None:
        return None
    base = page + off_
    # 表里取数据的指令：ldrsw xR, [xB, xIdx, lsl #2]
    e = None
    for k in range(fk - 1, max(0, j - 40), -1):
        p = insl[k]
        if p.mnemonic == 'ldrsw' and len(p.operands) > 1 and p.operands[1].type == ARM64_OP_MEM \\
                and p.reg_name(p.operands[1].mem.base) == basereg:
            e = p
            break
    if e is None:
        return None
    # 条目数：cmp w18, #N
    count = None
    for k in range(fk - 1, max(0, j - 20), -1):
        p = insl[k]
        if p.mnemonic == 'cmp' and p.op_str.startswith('w18,') and p.operands[1].type == ARM64_OP_IMM:
            count = p.operands[1].imm + 1
            break
    if not count or count > 256:
        return None
    targets = []
    for i in range(count):
        o = base + 4 * i
        if o + 4 > len(IMG):
            return None
        t = base + struct.unpack_from('<i', IMG, o)[0]
        if not (0 <= t < TEXT_END):
            return None
        targets.append(t)
    return targets, f'表基址 0x{base:x} 条目 {count} 目标 ' + ' '.join(f'{t:#x}' for t in targets)


def site_jt_chain(targets):
    """br x18（SMO 跳转表）→ 按目标值分发到直接分支垫片。

    进入时槽 K18 里已经是算好的目标地址（前面的 ldrsw/add 已各自按 x18 改写块写过槽）。
    链：保存 x16/x17 → x17 = 槽[K18] → 逐条 movz/movk+cmp+b.eq → 垫片（恢复 x16/x17 → 直接跳目标）。
    兜底 brk #0x180：原版有 b.hi 守卫，理论到不了；真到了要响亮地崩，而不是悄悄跳错。
    """
    start = here()
    eq = []                                   # (占位字下标, 目标下标)
    n_head = 2 + 1 + 2 + 1 + 1
    size = n_head
    for t in targets:
        size += len(mov64(16, t)) + 2          # +cmp +b.eq
    size += 1                                  # brk
    padbase = start + 4 * size
    emit(STUR(16, 31, -16))
    emit(STUR(17, 31, -24))
    emit(MRS_TPIDRRO(16))
    emit_key_addr(17, 'K18')                   # x17 = 槽 K18 的字节偏移
    emit(ADDR(16, 16, 17))                     # x16 = &槽
    emit(LDRX(17, 16))                         # x17 = 客体 x18 = 目标地址
    for i, t in enumerate(targets):
        for w in mov64(16, t):
            emit(w)
        emit(CMP_X(17, 16))
        eq.append((len(patch), i))
        emit(0)
    emit(0xD4200000 | (0x180 << 5))            # brk #0x180（兜底，不可达）
    for i, (_pos, _i) in enumerate(eq):
        pad = padbase + 12 * _i
        struct.pack_into('<I', patch, _pos, B_COND(PATCH_OFF + _pos, pad, 0))
    for i, t in enumerate(targets):
        emit(LDUR(16, 31, -16))
        emit(LDUR(17, 31, -24))
        emit(B(here(), t))
        assert here() == padbase + 12 * i + 12, '垫片布局对不上'
    return start

'''

OBS_APPLY = '''
# 观测门：与参考项目同构（入口原指令搬进宿主门块，宿主只读不改）
obs = list(OBS.get(BUILD_ID, []))
for off, oid, w in obs:
    put_branch(off, site_obs(off, oid, w)); stat('obs')
'''

TAIL = '''
os.makedirs(outdir, exist_ok=True)
P = lambda ext: os.path.join(outdir, f'{NAME}.{ext}')
open(P('patched.bin'), 'wb').write(IMG + bytearray(PATCH_OFF - len(IMG)) + patch)
import hashlib
with open(os.path.join(outdir, 'build_src.sha256'), 'w', encoding='utf-8') as f:
    f.write(hashlib.sha256(open(os.path.abspath(__file__), 'rb').read()).hexdigest() + '  patch_nso.py\\n')
with open(P('fixups.tsv'), 'w', encoding='utf-8') as f:
    f.write(f'# PATCH_OFF\\t{PATCH_OFF:#x}\\n# KEY_PAGE_OFF\\t{KEY_PAGE_OFF:#x}\\n# KEY_PAGE_SIZE\\t0x4000\\n'
            f'# TEXT_END\\t{TEXT_END:#x}\\n# RO_OFF\\t{RO_OFF:#x}\\n# DATA_OFF\\t{DATA_OFF:#x}\\n')
    for k in KEY_INDEX: f.write(f'# KEY_{k}_IDX\\t{KEY_INDEX[k]}\\n')
with open(P('gates.tsv'), 'w', encoding='utf-8') as f:
    for ret, k, arg, orig in gates: f.write(f'{ret:#x}\\t{k}\\t{arg}\\t{orig:#x}\\n')
with open(P('sites.tsv'), 'w', encoding='utf-8') as f:
    for a in sorted(kinds):
        if a not in done: f.write(f'{a:#x}\\t{kinds[a][0]}\\n')
    for wseg in windows: f.write(f'{wseg[0].address:#x}\\tW\\t{wseg[-1].address + 4:#x}\\n')
    for a, (b, c) in jumps.items(): f.write(f'{a:#x}\\tJ\\t{b:#x}\\t{c}\\n')
with open(P('cuts.tsv'), 'w', encoding='utf-8') as f:
    f.write(f'# BUILD_ID\\t{BUILD_ID}\\n')
    for off, ob, nb, why, scope in cuts: f.write(f'{off:#x}\\t{ob}\\t{nb}\\t{scope}\\t{why}\\n')
with open(P('jt.tsv'), 'w', encoding='utf-8') as f:                     # 本仓新增：SMO 跳转表改写的登记（原表基址/条目/目标）
    f.write(f'# BUILD_ID\\t{BUILD_ID}\\n')
    for a, (tgt, why) in sorted(jt_smo.items()): f.write(f'{a:#x}\\t{why}\\n')
with open(P('obs.tsv'), 'w', encoding='utf-8') as f:
    f.write(f'# BUILD_ID\\t{BUILD_ID}\\n')
    for off, oid, ow, why in obs: f.write(f'{off:#x}\\t{oid}\\t{ow}\\t{why}\\n')
print(NAME, stats, 'cuts', len(cuts), 'obs', len(obs), 'jt', len(jt_smo), 'patch', len(patch),
      'PATCH_OFF', hex(PATCH_OFF), 'slot_refs', len(fix), 'gates', len(gates))
'''

BOOTLIST = seg(876, 889)

generic = seg(245, 602)

# --- 主扫描循环打补丁 1：br x18 检测支持 SMO 形状 ---
old_detect = """            assert base and width, f'jump table {ins.address:x}'
            jumps[ins.address] = (base, width)"""
new_detect = """            if base and width:
                jumps[ins.address] = (base, width)
                continue
            jt = jt_smo_shape(insl, j)          # 本仓新增：SMO 形状（表存 4 字节偏移，目标 = 表基址 + 偏移）
            assert jt, f'jump table 形状未识别 {ins.address:x}'
            jt_smo[ins.address] = jt"""
assert old_detect in generic, '找不到 br x18 检测锚点'
generic = generic.replace(old_detect, new_detect)

# --- 打补丁 2：主循环里分流 ---
old_disp = """        if m == 'br': st = site_br(ins, *jumps[a]); stat('br')"""
new_disp = """        if m == 'br':
            if a in jt_smo: st = site_jt_chain(jt_smo[a][0]); stat('jt_smo')
            else: st = site_br(ins, *jumps[a]); stat('br')"""
assert old_disp in generic, '找不到 br 分流锚点'
generic = generic.replace(old_disp, new_disp)

# --- 打补丁 3：声明 jt_smo ---
assert "done = set(); windows = []; jumps = {}" in generic
generic = generic.replace("done = set(); windows = []; jumps = {}",
                          "done = set(); windows = []; jumps = {}; jt_smo = {}")

head = seg(7, 51)
body = (DOC
        + head
        + CUTS_STUB
        + seg(166, 166)                                  # BUILD_ID
        + "\n" + seg(177, 177) + "\n"                    # cuts = list(CUTS.get(...))
        + seg(238, 242)                                  # 剪口应用 + 断言
        + JT_CODE
        + generic
        + OBS_APPLY
        + TAIL
        + BOOTLIST + "\n")
open(OUT, "w", encoding="utf-8", newline="\n").write(body)
print("生成", OUT, len(body.split(chr(10))), "行")
