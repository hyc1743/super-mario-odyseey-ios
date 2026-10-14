#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查「NVN 对象不透明」这条前提：客体有没有绕过 getter 直接读 NVN 结构体字段。

为什么要查这个（设计背景见 smo/verify/README.md）：
  smo/gfx/nvn/nvn_state.c 把宿主状态全放在宿主侧，客体的 NVN 对象当不透明句柄、一字节不写。
  这比参考项目（直接用 h1k421 的 nvn.h 布局）更稳 —— 那份头是从 Darksiders II 的 DWARF
  重建的，版本未必和 SMO 一致。但它隐含前提：客体自己也只通过 nvnXxxGetYyy() 取值。
  如果 AGL 某处直接 `ldr w9,[x0,#0x18]` 读对象字段，宿主状态表就填不到它。

做法：
  1. 从 smo/gfx/nvn/slots.tsv 取 名字 → GOT 槽（那张表是 nvn_slots.py 从 main 里挖出来的）。
  2. 全 .text 扫 NVN 调用点。形状（实测，见 gfx/README.md）：
         adrp xR, page ; ldr xR,[xR,#off] ; ldr xT,[xR] ; blr xT
     其中 page+off 命中已知槽 ⇒ 这是对某个具名 NVN 函数的调用。
  3. 在调用点前的窗口里回溯「对象指针」(x0) 的来源寄存器：跟 `mov x0, xN` 一跳。
  4. 在调用点前后的窗口里，看该寄存器有没有被当**结构体基址**用：`ldr/str ..., [xN, #imm]`
     且 imm != 0（imm==0 多半是解引用指针本身，不算读字段）。

报出来的是**候选**，不是结论。寄存器会被复用于无关用途，所以每一条都要人工反汇编确认。
本工具不做全程数据流：对象指针若被存进 AGL 成员、过很久在别处解引用，这里看不到。
所以结论只能是「没有发现反例」，不是「证明不存在」。

用法：
  nvn_opaque_check.py [--mod main] [--list] [--tsv 路径] [--win-before N] [--win-after N]
退出码：0 = 没有发现反例（或只有白名单内的）；1 = 有候选，需要人看。
"""
import collections
import os
import struct
import sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))              # smo/verify
ROOT = os.path.dirname(os.path.dirname(HERE))                  # 仓库根
WORK = os.path.join(ROOT, "work", "game")
SLOTS = os.path.join(ROOT, "smo", "gfx", "nvn", "slots.tsv")
NVN_H = os.path.join(ROOT, "smo", "gfx", "nvn", "include", "nvn.h")

# NVN 函数名前缀 → 第一个参数的对象类型名。用来按对象大小分流候选。
# 只列实际报出候选的家族即可，缺的会落到「未知大小」一档（照样报出来）。
PREFIX_TYPE = [
    ("nvnCommandBuffer", "NVNcommandBuffer"),
    ("nvnMemoryPoolBuilder", "NVNmemoryPoolBuilder"),
    ("nvnMemoryPool", "NVNmemoryPool"),
    ("nvnTextureView", "NVNtextureView"),
    ("nvnTexturePool", "NVNtexturePool"),
    ("nvnTextureBuilder", "NVNtextureBuilder"),
    ("nvnTexture", "NVNtexture"),
    ("nvnSamplerBuilder", "NVNsamplerBuilder"),
    ("nvnSamplerPool", "NVNsamplerPool"),
    ("nvnSampler", "NVNsampler"),
    ("nvnBufferBuilder", "NVNbufferBuilder"),
    ("nvnBuffer", "NVNbuffer"),
    ("nvnProgram", "NVNprogram"),
    ("nvnQueueBuilder", "NVNqueueBuilder"),
    ("nvnQueue", "NVNqueue"),
    ("nvnDeviceBuilder", "NVNdeviceBuilder"),
    ("nvnDevice", "NVNdevice"),
    ("nvnWindowBuilder", "NVNwindowBuilder"),
    ("nvnWindow", "NVNwindow"),
    ("nvnEventBuilder", "NVNeventBuilder"),
    ("nvnEvent", "NVNevent"),
    ("nvnSync", "NVNsync"),
    ("nvnBlendState", "NVNblendState"),
    ("nvnPolygonState", "NVNpolygonState"),
    ("nvnColorState", "NVNcolorState"),
    ("nvnChannelMaskState", "NVNchannelMaskState"),
    ("nvnDepthStencilState", "NVNdepthStencilState"),
    ("nvnMultisampleState", "NVNmultisampleState"),
    ("nvnVertexAttribState", "NVNvertexAttribState"),
    ("nvnVertexStreamState", "NVNvertexStreamState"),
]


def load_sizes(path):
    """从 nvn.h 抽 `struct { char reserved[N]; } NVNxxx;` 的大小。

    注意：这份头是 h1k421 从 Darksiders II 的 DWARF 重建的，版本未必与 SMO 一致。
    这里**只**拿它当分流的参考尺度（判断偏移是否明显落在对象之外），
    不拿它的字段布局去读写客体内存 —— 那条设计红线见 README。
    """
    import re
    out = {}
    src = open(path, encoding="utf-8", errors="replace").read()
    pat = re.compile(
        r"typedef\s+struct\s*\{\s*(?:unsigned\s+)?char\s+reserved\s*\[\s*([0-9a-fA-Fx]+)\s*\]\s*;\s*\}\s*(NVN\w+)\s*;")
    for m in pat.finditer(src):
        out[m.group(2)] = int(m.group(1), 0)
    return out


def obj_type_of(name):
    for pfx, ty in PREFIX_TYPE:
        if name.startswith(pfx):
            return ty
    return None

WIN_BEFORE = 8      # 回溯窗口（指令条数）
WIN_AFTER = 4       # 调用点之后的窗口


def sx(v, bits):
    return v - (1 << bits) if v >> (bits - 1) else v


def adrp_page(pc, w):
    return (pc & ~0xFFF) + (sx(((w >> 5) & 0x7FFFF) << 2 | ((w >> 29) & 3), 21) << 12)


def load_slots(path):
    """槽地址 → NVN 名字。"""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            c = line.rstrip("\n").split("\t")
            if len(c) >= 2:
                out[int(c[1], 16)] = c[0]
    return out


def find_callsites(img, text_end, slots):
    """返回 [(调用点地址, NVN 名字)]。

    形状：adrp xR,page ; ldr xR,[xR,#off] ; ldr xT,[xR] ; blr xT
    中间允许夹无关指令，所以 adrp 之后在小窗口里找配对的 ldr。
    """
    n = text_end // 4
    w = struct.unpack_from(f"<{n}I", img, 0)
    out = []
    for i in range(n):
        a0 = w[i]
        if (a0 & 0x9F000000) != 0x90000000:                  # adrp
            continue
        rd = a0 & 0x1F
        page = adrp_page(i * 4, a0)
        for j in range(i + 1, min(i + 6, n)):
            b0 = w[j]
            # ldr Xd, [Xn, #imm]  (64 位, unsigned offset)  = 0xF94....
            if (b0 & 0xFFC00000) != 0xF9400000:
                continue
            if ((b0 >> 5) & 0x1F) != rd:
                continue
            off = ((b0 >> 10) & 0xFFF) << 3
            va = page + off
            if va not in slots:
                break
            # 槽命中。往后找 blr（跳过中间的 ldr 解引用）
            for k in range(j + 1, min(j + 6, n)):
                c0 = w[k]
                if (c0 & 0xFFFFFC1F) == 0xD63F0000:          # blr Xn
                    out.append((k * 4, slots[va]))
                    break
            break
    return out


# 追踪结果的种类。决定了后面怎么解读一个调用点。
#   REG     对象指针在某个寄存器里，偏移基准是 0（对象起点就是该寄存器的值）
#   BASEOFF 对象是「某寄存器 + 常量」处的内嵌子对象，偏移基准是那个常量
#   STACK   对象在栈上（AGL 自己的临时 builder），不是客体持有的 NVN 对象
#   OPAQUE  对象指针从内存里取出来，基址表达式本工具追不动
TRACE_REG, TRACE_BASEOFF, TRACE_STACK, TRACE_OPAQUE = range(4)


def trace_obj_reg(insns, call_idx):
    """回溯 x0（对象指针）的来源，返回 (kind, 寄存器名, 基准偏移, 窗口起点)。

    为什么要区分这些（都是实测形状，见 README 的覆盖率说明）：

      `mov x0, xN`            → REG(xN, 0)。xN 在 mov 之前就持有对象，所以
                                 mov 之前的访问也算在内。
      x0 在窗口里没被定义      → REG(x0, 0)。x0 一路带着对象进来（上一个调用的
                                 返回值，或本函数的参数）。
      `add x0, xN, #imm`      → BASEOFF(xN, imm)。**这是最常见的一种（471 处）**：
                                 NVN 对象内嵌在 AGL 的大对象里，xN 是 AGL 对象，
                                 NVN 对象从 xN+imm 开始。所以对 xN 的访问要用
                                 imm 作基准换算：落在 [imm, imm+size) 才算读 NVN 字段。
      `add/sub/mov x0, sp/x29`→ STACK。AGL 在栈上造的临时对象（典型是 builder：
                                 `add x0, sp, #8 ; nvnBufferBuilderSetDefaults`）。
                                 那是 AGL 自己的栈内存，读写它与「客体是否窥探 NVN
                                 对象」无关 —— 而且 builder 本来就是客体自己填的。
      `ldr x0, [...]`         → OPAQUE。指针从内存取出，基址表达式追不动。
      `cbz x0, ...`           → 不是定义！capstone 的 op_str 第一个操作数是被测
                                 寄存器。continue 掉（106 处都是这个）。

    只跟一跳。再深就不是「局部窗口」这个方法能回答的了。
    """
    lo = max(call_idx - WIN_BEFORE, 0)
    for p in range(call_idx - 1, lo - 1, -1):
        ins = insns[p]
        m, ops = ins.mnemonic, ins.op_str
        if ops.split(",")[0].strip() != "x0":
            continue
        if m.startswith("st"):                   # str x0,[...] 是读 x0，不是定义
            continue
        if m in ("cbz", "cbnz", "tbz", "tbnz"):  # 条件分支，x0 是被测值不是目的
            continue
        parts = [x.strip() for x in ops.split(",")]
        if m == "mov" and len(parts) == 2:
            src = parts[1]
            if src in ("sp", "x29"):
                return TRACE_STACK, src, 0, lo
            if src.startswith("x"):
                return TRACE_REG, src, 0, lo
            return TRACE_OPAQUE, None, 0, lo
        if m in ("add", "sub") and len(parts) == 3 and parts[2].startswith("#"):
            base = parts[1]
            try:
                imm = int(parts[2][1:], 0)
            except ValueError:
                return TRACE_OPAQUE, None, 0, lo
            if base in ("sp", "x29"):
                return TRACE_STACK, base, 0, lo
            if m == "sub":                       # 对象在 base-imm 处，基准是负的
                return TRACE_OPAQUE, None, 0, lo
            return TRACE_BASEOFF, base, imm, lo
        return TRACE_OPAQUE, None, 0, lo
    return TRACE_REG, "x0", 0, lo


def clamp_to_def(insns, lo, hi, reg, call_idx):
    """把窗口起点推到 reg 最后一次被定义之处（含该条）。

    不这么做会把「旧值时期」的访问算进来。实测踩到过：

        0xa0b998  ldr x8,  [x23, #0x50]   ← 这时 x23 是别的东西
        0xa0b9a0  ldr x23, [x8]           ← x23 在这里被重新定义
        0xa0b9ac  add x0,  x23, #0x48     ← 这之后 x23 才是承载 NVN 对象的那个
        0xa0b9b4  blr x8

    不 clamp 的话第一条会被当成「读 NVN 对象内部」—— 纯噪声。

    调用点之后同理：reg 一旦被重新定义，再往后的访问也不是这个对象了，所以收窄上界。
    """
    def is_def(ins):
        m, ops = ins.mnemonic, ins.op_str
        if ops.split(",")[0].strip() != reg:
            return False
        return not (m.startswith("st")
                    or m in ("cbz", "cbnz", "tbz", "tbnz", "cmp", "cmn", "tst"))

    start, end = lo, min(hi, len(insns))
    for p in range(lo, min(call_idx, len(insns))):
        if is_def(insns[p]):
            start = p                   # 之前的访问针对的是旧值，与本对象无关
    for p in range(call_idx + 1, end):
        if is_def(insns[p]):
            end = p                     # 之后的访问针对的是新值
            break
    return start, end


def struct_access(insns, lo, hi, reg, call_idx):
    """在 [lo,hi) 窗口里找 `ldr/str ..., [reg, #imm]` 且 imm != 0。

    返回 [(地址, 指令文本, 偏移, 类别)]。类别用来分流假阳性：
      "fetch"  —— 调用点之前、且目的寄存器就是 reg 本身（`ldr x0,[x0,#0xb8]`）。
                  这是「从 AGL 自己的包装对象里取出 NVN 指针」，访问的是 AGL 结构，
                  不是 NVN 对象。不违规。
      "field"  —— 其余，疑似把 NVN 对象当结构体读写，要看偏移是否在对象大小内。
    """
    hits = []
    pat = f"[{reg}, #"
    for p in range(max(lo, 0), min(hi, len(insns))):
        ins = insns[p]
        if not (ins.mnemonic.startswith("ldr") or ins.mnemonic.startswith("str")):
            continue
        if pat not in ins.op_str:
            continue
        # 取出 imm，排除 0（那是解引用指针本身，不是读字段）
        try:
            frag = ins.op_str.split(pat, 1)[1].split("]")[0]
            imm = int(frag, 0)
        except ValueError:
            continue
        if imm == 0:
            continue
        dst = ins.op_str.split(",")[0].strip()
        kind = "field"
        if p < call_idx and ins.mnemonic.startswith("ldr") and dst == reg:
            kind = "fetch"      # reg = *(reg + imm)：取出的是 NVN 指针，不是读 NVN 字段
        hits.append((ins.address, f"{ins.mnemonic} {ins.op_str}", imm, kind))
    return hits


def main():
    mod = "main"
    argv = sys.argv
    if "--mod" in argv:
        mod = argv[argv.index("--mod") + 1]
    show = "--list" in argv
    tsv = argv[argv.index("--tsv") + 1] if "--tsv" in argv else None

    img_path = os.path.join(WORK, mod + ".bin")
    if not os.path.exists(img_path):
        print(f"缺 {img_path} —— 先跑 nsp_extract.py / nso_info.py 产出 work/game/{mod}.bin")
        return 2
    img = open(img_path, "rb").read()
    nso = open(os.path.join(WORK, "ExeFS", mod), "rb").read(0x20)
    text_end = struct.unpack_from("<I", nso, 0x18)[0]

    slots = load_slots(SLOTS)
    sizes = load_sizes(NVN_H)
    sites = find_callsites(img, text_end, slots)
    print(f"# {mod}: NVN 调用点 {len(sites)} 处，涉及 {len(set(n for _, n in sites))} 个名字")
    print(f"# nvn.h 读出 {len(sizes)} 个对象大小（仅作分流尺度，不用其字段布局）")

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    # 只反汇编每个调用点周围的小窗口 —— 整段 .text 有 3900 万字节，没必要全反。
    span_lo, span_hi = WIN_BEFORE + 2, WIN_AFTER + 1
    rows = []
    cover = collections.Counter()
    for addr, name in sites:
        lo = max(addr - span_lo * 4, 0)
        hi = min(addr + span_hi * 4, text_end)
        insns = list(md.disasm(img[lo:hi], lo))
        call_idx = next((i for i, x in enumerate(insns) if x.address == addr), None)
        if call_idx is None:
            continue
        kind, reg, basis, win_lo = trace_obj_reg(insns, call_idx)
        cover[kind] += 1
        if kind in (TRACE_STACK, TRACE_OPAQUE):
            continue
        w0, w1 = clamp_to_def(insns, win_lo, call_idx + WIN_AFTER + 1, reg, call_idx)
        hits = struct_access(insns, w0, w1, reg, call_idx)
        ty = obj_type_of(name)
        size = sizes.get(ty)
        for hit_addr, text, imm, acc in hits:
            # 换算到「相对 NVN 对象起点」的偏移。内嵌子对象时 basis != 0。
            rel = imm - basis
            if acc == "fetch":
                verdict = "fetch"           # 取 NVN 指针，访问的是 AGL 自己的对象
            elif rel < 0:
                verdict = "before"          # 在内嵌 NVN 对象之前 → AGL 自己的字段
            elif size is None:
                verdict = "unknown"         # 不知道对象多大，无法分流 → 要人看
            elif rel >= size:
                verdict = "outside"         # 偏移超出 NVN 对象 → AGL 紧随其后的字段
            else:
                verdict = "INSIDE"          # 落在 NVN 对象内部 → 真正的反例候选
            rows.append((addr, name, reg, hit_addr, text, imm, basis, rel,
                         ty or "?", size, verdict))

    checked = cover[TRACE_REG] + cover[TRACE_BASEOFF]
    total = sum(cover.values())
    print(f"# 调用点覆盖：已检查 {checked}/{total} "
          f"（寄存器直传 {cover[TRACE_REG]} + 内嵌子对象 {cover[TRACE_BASEOFF]}）")
    print(f"#   跳过 {cover[TRACE_STACK]} 处栈上临时对象（AGL 自己的 builder，与本前提无关）")
    print(f"#   跳过 {cover[TRACE_OPAQUE]} 处指针从内存取出、基址表达式追不动的 —— "
          f"**这部分是本工具的盲区**")

    tally = collections.Counter(r[10] for r in rows)
    print(f"# 已检查的调用点里，带偏移访问 {len(rows)} 条，分流："
          f"fetch(取指针) {tally['fetch']} · "
          f"before(对象之前) {tally['before']} · "
          f"outside(超出对象) {tally['outside']} · "
          f"unknown(大小未知) {tally['unknown']} · "
          f"INSIDE(落在对象内) {tally['INSIDE']}")

    bad = [r for r in rows if r[10] in ("INSIDE", "unknown")]

    def fmt(r):
        addr, name, reg, hit_addr, text, imm, basis, rel, ty, size, verdict = r
        sz = f"0x{size:x}" if size is not None else "?"
        b = f" 基准+0x{basis:x}→rel 0x{rel:x}" if basis else ""
        return (f"0x{addr:x}  {name}  {reg}  0x{hit_addr:x}  {text}  "
                f"+0x{imm:x}{b}  {ty}(size {sz})  {verdict}")

    if show:
        for r in sorted(rows):
            print("  " + fmt(r))
    elif bad:
        print("# 需要人看的（INSIDE / unknown）：")
        for r in sorted(bad):
            print("  " + fmt(r))

    if tsv:
        with open(tsv, "w", encoding="utf-8", newline="\n") as f:
            f.write("# 调用点\tNVN 名字\t对象寄存器\t访问点\t指令\t偏移\t基准\t"
                    "相对偏移\t对象类型\t对象大小\t判定\n")
            for r in sorted(rows):
                sz = f"0x{r[9]:x}" if r[9] is not None else ""
                f.write("0x%x\t%s\t%s\t0x%x\t%s\t0x%x\t0x%x\t0x%x\t%s\t%s\t%s\n"
                        % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
                           r[8], sz, r[10]))
        print(f"写出 {tsv}：{len(rows)} 条")

    pct = 100.0 * checked / total if total else 0.0
    if not bad:
        print(f"没有发现反例：已检查的 {checked} 处调用点（占 {pct:.0f}%）里，")
        print("   所有带偏移的访问都落在 NVN 对象之外或只是取指针，没有一条读写对象内部。")
        print(f"   盲区是那 {cover[TRACE_OPAQUE]} 处追不动的调用点 —— 所以这不是「证明不存在」。")
        print("   终局判据是真机：nvn_state.c 查不到就记 miss 日志，跑一遍就知道有没有漏。")
        return 0
    print("有落在对象内部或大小未知的访问，需要人工反汇编确认。")
    return 1


if __name__ == "__main__":
    sys.exit(main())
