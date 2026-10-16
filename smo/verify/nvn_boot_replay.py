#!/usr/bin/env python3
"""Replay SMO's actual NVN device startup guards against a compiled host library.

Optional local verification; requires user-supplied SMO ExeFS/main and Unicorn.
Runs only main+0x731270..0x7313c4, including native nvnLoadCProcs. SDK, services,
game allocators, rendering and subsequent initialization are outside coverage.
The only substituted internal game call allocates the NVN device storage.
Build the GFX_COMMON sources as a shared library, then pass --library PATH.
"""
import argparse
import ctypes as C
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import nso_info
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X19, UC_ARM64_REG_X20, UC_ARM64_REG_X29, UC_ARM64_REG_X30, UC_ARM64_REG_SP, UC_ARM64_REG_PC


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--expect-reject", action="store_true")
    args = parser.parse_args()
    library = C.CDLL(str(Path(args.library).resolve()))
    raw, _ = nso_info.load_nso(args.main)
    if struct.unpack_from("<I", raw, 0x731314)[0] != 0x12001c08:
        raise SystemExit("Unsupported SMO main: device guard instruction differs")
    nso_info._IMG = raw
    dynamic, _ = nso_info.parse_dynamic(raw)
    symbols = nso_info.symbols(raw, dynamic)
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    keep = []

    def memory(size):
        size = (size + 0xfff) & ~0xfff
        buf = C.create_string_buffer(size + 0x1000)
        keep.append(buf)
        ptr = (C.addressof(buf) + 0xfff) & ~0xfff
        uc.mem_map_ptr(ptr, size, 7, ptr)
        return ptr

    base = memory(len(raw))
    uc.mem_write(base, raw)
    # Resolve local symbols and relative pointers, as loader_core does. The
    # sole external call in this range is intercepted at its PLT entry below.
    for tag, size_tag in ((7, 8), (23, 2)):
        for off in range(dynamic.get(tag, 0), dynamic.get(tag, 0) + dynamic.get(size_tag, 0), 24):
            location, info, addend = struct.unpack_from("<QQq", raw, off)
            kind = info & 0xffffffff
            sym = symbols[info >> 32]
            if kind == 1027:
                value = base + addend
            elif sym[2]:
                value = base + sym[1] + addend
            else:
                continue
            uc.mem_write(base + location, struct.pack("<Q", value))

    callbacks = {}
    code_pages = set()
    signature = C.CFUNCTYPE(C.c_uint64, *([C.c_uint64] * 8))

    def bind(ptr):
        if ptr in callbacks:
            return
        page = ptr & ~0xfff
        if page not in code_pages:
            uc.mem_map(page, 0x1000)
            code_pages.add(page)
        uc.mem_write(ptr, struct.pack("<I", 0xd65f03c0))
        callbacks[ptr] = signature(ptr)

    library.nvn_DeviceGetProcAddress.argtypes = [C.c_uint64] * 8
    library.nvn_DeviceGetProcAddress.restype = C.c_uint64
    library.nvn_BootstrapLoader.argtypes = [C.c_char_p]
    library.nvn_BootstrapLoader.restype = C.c_uint64
    for name in (Path(__file__).parents[1] / "gfx/nvn/names.txt").read_text(encoding="utf-8").splitlines():
        if name.startswith("nvn"):
            encoded = C.c_char_p(name.encode())
            ptr = library.nvn_DeviceGetProcAddress(0, C.cast(encoded, C.c_void_p).value, 0, 0, 0, 0, 0, 0)
            if ptr:
                bind(ptr)

    stack = memory(0x10000)
    framework = memory(0x1000)
    device = memory(0x4000)
    uc.reg_write(UC_ARM64_REG_SP, stack + 0x8000)
    uc.reg_write(UC_ARM64_REG_X29, stack + 0x9000)
    uc.reg_write(UC_ARM64_REG_X19, framework)
    uc.reg_write(UC_ARM64_REG_X20, framework + 0x800)
    outcome = []
    calls = []

    def returned(value):
        uc.reg_write(UC_ARM64_REG_X0, value)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_X30))

    def step(_, pc, size, user):
        del size, user
        if pc == base + 0x731a7c:
            outcome.append("rejected")
            uc.emu_stop()
        elif pc == base + 0x7313c4:
            outcome.append("passed")
            uc.emu_stop()
        elif pc == base + 0x72cee8:
            returned(device)
        elif pc == base + 0xbf03f8:
            name = C.string_at(uc.reg_read(UC_ARM64_REG_X0))
            calls.append(name.decode())
            returned(library.nvn_BootstrapLoader(name))
        elif pc in callbacks:
            values = [uc.reg_read(UC_ARM64_REG_X0 + i) for i in range(8)]
            returned(callbacks[pc](*values))

    uc.hook_add(UC_HOOK_CODE, step)
    uc.emu_start(base + 0x731270, base + len(raw), count=100000)
    print("Native SMO device startup:", outcome, "bootstrap:", calls)
    expected = "rejected" if args.expect_reject else "passed"
    if outcome != [expected]:
        raise SystemExit("Unexpected device initialization guard outcome")


if __name__ == "__main__":
    main()
