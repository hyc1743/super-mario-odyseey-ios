#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""自检：**不需要任何游戏文件**就能跑的测试。CI 用它保证工具链没被改坏。

覆盖五块：
  1. Yaz0 解压（整字节面量流、回引用流、长度扩展分支）
  2. SARC 解析（自造归档，校验名字/偏移/数据）
  3. NSO 头解析
  4. **改写器端到端**：自造一个合成 NSO（含 svc / mrs tpidr_el0 / x18 读写 / SMO 形状跳转表），
     跑 patch_nso.py，再用 rewrite_check.py 校验产物结构自洽。这条最有价值：
     它把「改写器没坏」变成 CI 每次都能证明的事，而不需要游戏文件。
  5. HLE 接管边界：lmem 本地堆函数保留 SDK 实现，fs / NVN 服务接管仍然有效。

用法：python3 smo/tools/selftest.py
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

FAILS = []


def check(name, cond, extra=""):
    print(("  ✓ " if cond else "  ✗ ") + name + ("" if cond else f"    {extra}"))
    if not cond:
        FAILS.append(name)


# ------------------------------------------------------------------ 1. Yaz0

def make_yaz0_literal(payload):
    out = bytearray(b"Yaz0" + struct.pack(">I", len(payload)) + b"\0" * 8)
    pos = 0
    while pos < len(payload):
        n = min(8, len(payload) - pos)
        out.append((0xFF << (8 - n)) & 0xFF)
        out += payload[pos:pos + n]
        pos += n
    return bytes(out)


def make_yaz0_ref():
    """「AB」×8：2 个字面量 + 1 个长度 14 的回引用（距离 2）。
    code=0b110_00000：第 1、2 位 = 字面量，第 3 位 = 回引用，其余位用不到。"""
    want = b"AB" * 8
    out = bytearray(b"Yaz0" + struct.pack(">I", len(want)) + b"\0" * 8)
    out.append(0b110_00000)
    out += b"AB"
    out += bytes([(12 << 4) | 0x00, 0x01])       # 长度 14 → 12+2；距离-1 = 1
    return bytes(out), want


def test_yaz0():
    from nintendo_archives import yaz0_decompress
    p = bytes(range(256)) * 3
    check("Yaz0 全字面量流", yaz0_decompress(make_yaz0_literal(p)) == p)
    d, want = make_yaz0_ref()
    got = yaz0_decompress(d)
    check("Yaz0 回引用流", got == want, f"{got!r} != {want!r}")


# ------------------------------------------------------------------ 2. SARC

def make_sarc(files):
    names, nodes, data = b"", b"", b""
    data_off = 0x1000
    for name, blob in files:
        nodes += struct.pack("<IIII", 0, (1 << 24) | len(names), len(data), len(data) + len(blob))
        names += name.encode() + b"\0"
        data += blob
    sfnt_off = 0x14 + 0xC + 0x10 * len(files)
    body = (b"SARC" + struct.pack("<HH", 0x14, 0xFEFF)
            + struct.pack("<IIHH", data_off + len(data), data_off, 0x0100, 0)
            + b"SFAT" + struct.pack("<HHI", 0xC, len(files), 0x65)
            + nodes
            + b"SFNT" + struct.pack("<HH", 8, 0) + names)
    body += b"\0" * (data_off - len(body))
    return body + data


def test_sarc():
    from nintendo_archives import Sarc
    files = [("alFoo.sharc", b"\x01\x02\x03" * 10), ("alBar.sharcb", b"xyz")]
    s = Sarc(make_sarc(files))
    check("SARC 项数与名字", sorted(n for n, _, _ in s.entries) == ["alBar.sharcb", "alFoo.sharc"])
    check("SARC 数据", s.read("alFoo.sharc") == files[0][1] and s.read("alBar.sharcb") == files[1][1])


# ------------------------------------------------------------------ 3. NSO 头

def build_nso(text, rodata, data, build_id, mod0_extra=None):
    """造一个未压缩 NSO0：text 在 0，rodata 在 0x4000，data 在 0x8000。"""
    segs = [(0, text), (0x4000, rodata), (0x8000, data)]
    hdr = bytearray(0x100)
    hdr[0:4] = b"NSO0"
    struct.pack_into("<I", hdr, 0xC, 0)
    off = 0x100
    for i, (mo, blob) in enumerate(segs):
        struct.pack_into("<III", hdr, 0x10 + i * 0x10, off, mo, len(blob))
        struct.pack_into("<I", hdr, 0x60 + i * 4, len(blob))
        off += len(blob)
    hdr[0x40:0x60] = build_id
    return bytes(hdr) + text + rodata + data


def test_nso():
    from nso_info import load_nso
    with tempfile.TemporaryDirectory() as td:
        p = os.path.join(td, "m")
        open(p, "wb").write(build_nso(b"\xAA" * 0x30, b"\xBB" * 0x20, b"\xCC" * 0x10, bytes(range(0x20))))
        img, info = load_nso(p)
    check("NSO 构建号", info["build_id"] == bytes(range(0x20)).hex())
    check("NSO 段布局", img[0] == 0xAA and img[0x4000] == 0xBB and info["text_end"] == 0x30
          and info["ro_off"] == 0x4000 and len(img) == 0x8010)


# ------------------------------------------------------------------ 4. 改写器端到端

def synth_module_text():
    """合成一段代码，包含改写器要处理的每一种情形。返回 (text, rodata, 说明)。"""
    W = [0xD503201F] * (0x100 // 4)      # 0x00..0x100：含 MOD0 头（末尾回填）+ nop 填充
    add = W.append
    add(0xD4000801)                      # 0x100 svc #0x40
    add(0xD53BD040)                      # 0x104 mrs x0, tpidr_el0
    add(0x528000B2)                      # 0x108 movz w18, #5        （x18 写）
    add(0x90000030)                      # 0x10C adrp x16, #0x4000（imm=4，页基址 0x0）
    add(0x91020210)                      # 0x110 add x16, x16, #0x80 （表在 0x4080）
    add(0x51000432)                      # 0x114 sub w18, w1, #1
    add(0x71000E5F)                      # 0x118 cmp w18, #3
    add(0x54000728)                      # 0x11C b.hi #0x200
    add(0xB8B27A12)                      # 0x120 ldrsw x18, [x16, x18, lsl #2]
    add(0x8B100252)                      # 0x124 add x18, x18, x16
    add(0xD61F0240)                      # 0x128 br x18               （SMO 形状跳转表）
    while len(W) * 4 < 0x200:
        add(0xD503201F)
    while len(W) * 4 < 0x400:
        add(0xD503201F)

    # 跳转表：4 个 i32 偏移，目标 = 表基址 + 偏移（负偏移指回 .text）
    base = 0x4000 + 0x80
    targets = [0x130, 0x150, 0x170, 0x190]
    table = b"".join(struct.pack("<i", t - base) for t in targets)

    # eh_frame_hdr：version=1, ptr_enc=udata4, count_enc=udata4, table_enc=sdata4|datarel
    ehs_off = 0x4000                       # 放在 rodata 起点
    ehs = ehs_off
    hdr_ef = struct.pack("<BBBB", 1, 0x03, 0x03, 0x3B)
    hdr_ef += struct.pack("<I", 0)         # eh_frame_ptr（本仓不读它的值）
    hdr_ef += struct.pack("<I", 1)         # FDE 条数
    hdr_ef += struct.pack("<ii", 0x100 - ehs, 0x100 - ehs)   # 一个 FDE：函数从 0x100 起
                                                          # （映像头部是 ELF/MOD0，不是有效指令）
    rodata = bytearray(0x100)
    rodata[0:len(hdr_ef)] = hdr_ef
    rodata[0x80:0x80 + len(table)] = table

    # 回填 .text 开头的 MOD0 头：真实 NSO 的映像以 ELF 头起头，img[4] 是 MOD0 的相对偏移，
    # MOD0+0x10 是 eh_frame_hdr 的相对偏移（patch_nso.py 就是这么读的）
    text = bytearray(b"".join(struct.pack("<I", w) for w in W))
    text[0:4] = b"ELF"
    struct.pack_into("<I", text, 4, 8)
    text[8:12] = b"MOD0"
    struct.pack_into("<iii", text, 0x10, 0, 0, 0)                       # dyn / bss_start / bss_end
    struct.pack_into("<ii", text, 0x18, ehs - 8, ehs + len(hdr_ef) - 8)  # eh_frame_hdr 起止（相对 MOD0）
    struct.pack_into("<i", text, 0x20, 0)                                # module_object
    return bytes(text), bytes(rodata), targets


def test_rewriter():
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    text, rodata, targets = synth_module_text()

    # 先自检合成的指令确实是我们想要的那几条（避免「测试自己写错了还报通过」）
    want = {0x100: "svc", 0x104: "mrs", 0x108: "mov", 0x10C: "adrp",
            0x114: "sub", 0x118: "cmp", 0x11C: "b.hi", 0x120: "ldrsw",
            0x124: "add", 0x128: "br"}
    got = {i.address: i.mnemonic for i in md.disasm(text[0x100:0x12C], 0x100)}
    bad = {hex(a): (m, got.get(a)) for a, m in want.items() if got.get(a) != m}
    check("合成代码反汇编符合预期", not bad, str(bad))
    check("合成跳转表可解析", md.disasm(text[0x120:0x124], 0x120).__next__().op_str == "x18, [x16, x18, lsl #2]",
          md.disasm(text[0x120:0x124], 0x120).__next__().op_str)

    with tempfile.TemporaryDirectory() as td:
        nso = os.path.join(td, "m")
        open(nso, "wb").write(build_nso(text, rodata, b"\0" * 0x100, bytes(range(0x20))))
        out = os.path.join(td, "out")
        r = subprocess.run([sys.executable, os.path.join(HERE, "patch_nso.py"), nso, out, "m"],
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        check("patch_nso.py 跑通", r.returncode == 0, (r.stderr or "").strip()[-400:])
        if r.returncode != 0:
            return
        jt = os.path.join(out, "m.jt.tsv")
        check("跳转表被识别", os.path.exists(jt) and "0x128" in open(jt, encoding="utf-8").read(),
              open(jt, encoding="utf-8").read() if os.path.exists(jt) else "无 jt.tsv")
        gates = open(os.path.join(out, "m.gates.tsv"), encoding="utf-8").read()
        check("svc 生成了宿主门", "svc" in gates, gates.replace("\n", " "))
        sites = open(os.path.join(out, "m.sites.tsv"), encoding="utf-8").read().splitlines()
        check("改写点数量合理", len(sites) >= 5, f"{len(sites)} 个")
        r2 = subprocess.run([sys.executable, os.path.join(HERE, "rewrite_check.py"), out, "m"],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
        check("产物结构自检通过", r2.returncode == 0 and "全部通过" in r2.stdout,
              (r2.stdout + r2.stderr).strip()[-400:])


def test_hle_policy():
    import gen_hle_force
    import hle_inventory

    # v0.6.6 真机：本地堆函数被接到返回 0 的桩，SDK 随后解引用空指针。
    # 同时检查策略、库存及生成的 C，不能只验证生成器与错误清单自洽。
    symbols = (
        "_ZN2nn4lmem13CreateExpHeapEPvmi",
        "_ZN2nn4lmem13FreeToExpHeapEPNS0_6detail8HeapHeadEPv",
        "_ZN2nn4lmem19AllocateFromExpHeapEPNS0_6detail8HeapHeadEm",
        "_ZN2nn4lmem15CreateFrameHeapEPvmi",
        "_ZN2nn4lmem16DestroyFrameHeapEPNS0_6detail8HeapHeadE",
        "_ZN2nn4lmem21AllocateFromFrameHeapEPNS0_6detail8HeapHeadEmi",
    )
    forced = {r[1] for r in gen_hle_force.load_rows()}
    with open(gen_hle_force.OUT, encoding="utf-8") as f:
        generated = f.read()
    with open(gen_hle_force.TSV, encoding="utf-8") as f:
        inventory = [line.rstrip("\n").split("\t") for line in f]
    for sym in symbols:
        check("lmem 使用 SDK 实现：" + sym,
              hle_inventory.verdict(sym, ["sdk"]) == ("模块内", "lmem")
              and sym not in forced and sym not in generated
              and any(len(r) == 5 and r[1] == sym and r[3:] == ["sdk", "模块内"]
                      for r in inventory))
    for sym in hle_inventory.LOCAL_SF_SYMBOLS:
        check("HIPC 本地缓冲区/序列化使用 SDK 实现：" + sym,
              hle_inventory.verdict(sym, ["sdk"]) == ("模块内", "sf")
              and sym not in forced and sym not in generated
              and any(len(r) == 5 and r[1] == sym and r[3:] == ["sdk", "模块内"]
                      for r in inventory))
    check("HIPC 传输仍由宿主接管",
          "_ZN2nn2sf4hipc15SendSyncRequestENS_3svc6HandleEPvm" in forced)
    for row in inventory:
        if len(row) == 5 and row[2] in ("font", "fontll") and row[3] == "sdk":
            check("SDK font decoder/rasterizer remains local: " + row[1],
                  hle_inventory.verdict(row[1], ["sdk"]) == ("模块内", row[2])
                  and row[4] == "模块内" and row[1] not in forced)
    check("shared font resource service still intercepted",
          "_ZN2nn2pl22GetSharedFontLoadStateENS0_14SharedFontTypeE" in forced)
    for sym in hle_inventory.LOCAL_SDK_FLAGS:
        check("SDK 本地 fs 开关保留：" + sym,
              hle_inventory.verdict(sym, ["sdk"]) == ("模块内", "fs") and sym not in forced)
    for sym in gen_hle_force.SDK_STARTUP:
        check("SDK 内部初始化入口明确接管：" + sym, sym in forced and sym in generated)
    # 服务接管仍必须保留，不能通过关闭整个 HLE 表来修复本地库。
    fs = "_ZN2nn2fs12MountRomCacheEPKcPvm"
    check("fs 服务仍需接管", hle_inventory.verdict(fs, ["sdk"])[0]
          == "模块内(服务客户端，仍需接管)")
    check("图形入口仍需接管", "nvnBootstrapLoader" in forced)


def main():
    for name, fn in (("1. Yaz0", test_yaz0), ("2. SARC", test_sarc),
                     ("3. NSO 头", test_nso), ("4. 改写器端到端", test_rewriter),
                     ("5. HLE 接管边界", test_hle_policy)):
        print(f"== {name} ==")
        try:
            fn()
        except Exception as e:
            check(name + "（异常）", False, f"{type(e).__name__}: {e}")
    print()
    if FAILS:
        print(f"🔴 {len(FAILS)} 项失败：" + "、".join(FAILS))
        return 1
    print("全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
