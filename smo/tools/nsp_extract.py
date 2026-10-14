#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NSP/NCA 解密与提取 —— 参考项目 d2r-mobile 的 `dm/tools/extract_game.sh`（内部调 hactool）在本环境下的等价实现。

为什么自己写：这台机器是 Windows，hactool 依赖 mbedtls，从源码构建风险高。
算法不是猜的，逐条对照 hactool 源码移植（源码在 C:/tmp/hactool）：
  NCA 头 XTS   : nca.c nca_decrypt_header()   -> aes_xts_decrypt(header_key, ..., 0xC00, sector=0, sector_size=0x200)
  XTS tweak    : aes.c get_tweak()            -> sector 按 **大端** 写进 16 字节 tweak
  XTS 单元     : mbedtls_aes_crypt_xts()      -> T0 = AES_enc(tweak_key, iv)，逐块 gf128mul_x_ble
  gf128mul     : mbedtls/library/gf128mul.c   -> 两个小端 u64，溢出异或 0x87
  密钥区       : nca.c nca_decrypt_key_area() -> AES-128-ECB(key_area_key_application[crypto_type], encrypted_keys[0x40])
  内容密钥     : nca.c:462                    -> 带 rights_id 的 NCA：AES-128-ECB-Dec(titlekek[crypto_type], ticket 里的标题密钥)
  内容密钥     : nca.c:534                    -> 不带 rights_id：直接用解密后密钥区的第 3 项 decrypted_keys[0x20:0x30]
  CTR          : nca.c:493                    -> ctr[0..7]=反转(section_ctr)，ctr[8..15]=大端(section_offset>>4)，128 位大端计数器
  分区偏移     : utils.h media_to_real()      -> media_start_offset * 0x200
  PFS0 位置    : nca.c:1000                   -> 段起点 + 超级块 pfs0_offset（超级块在 fs 头 +0x08，pfs0_offset 在 +0x38）
  PFS0 文件    : nca.c:983                    -> pfs0_offset + pfs0 头长度 + 条目 offset
  PFS0 结构    : pfs0.h
  IVFC/RomFS   : ivfc.h

本机实测（SMO US v0，0100000000010000）：
  Program NCA 段0 = ExeFS（PFS0，CTR 加密，段内偏移 0x8000）
  段1 = RomFS（IVFC，CTR 加密）  段2 = logo（PFS0，不加密）
  ticket 里的标题密钥在 tik+0x180（16 字节），titlekek 索引 = 2

用法：
  nsp_extract.py info  <nsp> <prod.keys>
  nsp_extract.py exefs <nsp> <prod.keys> <输出目录>
  nsp_extract.py romfs <nsp> <prod.keys> <输出目录>
"""
import os
import struct
import sys
import hashlib
from Crypto.Cipher import AES

MEDIA_SIZE = 0x200
NCA_HEADER_SIZE = 0xC00


# ----------------------------------------------------------------------------- 密钥

def load_keys(path):
    keys = {}
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if "=" not in line:
                continue
            k, v = line.split("=", 1)
            k, v = k.strip(), v.strip()
            if v:
                try:
                    keys[k] = bytes.fromhex(v)
                except ValueError:
                    pass
    return keys


# ----------------------------------------------------------------------------- 密码学原语（照抄 hactool/mbedtls 语义）

def _xor(a, b):
    return bytes(x ^ y for x, y in zip(a, b))


def gf128mul_x_ble(x):
    """mbedtls/library/gf128mul.c:65。"""
    a = int.from_bytes(x[0:8], "little")
    b = int.from_bytes(x[8:16], "little")
    ra = ((a << 1) & 0xFFFFFFFFFFFFFFFF) ^ (0x87 if (b >> 63) else 0)
    rb = ((a >> 63) | ((b << 1) & 0xFFFFFFFFFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF
    return ra.to_bytes(8, "little") + rb.to_bytes(8, "little")


def xts_decrypt(key32, data, sector_start=0, unit=0x200):
    """hactool aes.c aes_xts_decrypt + get_tweak（sector 大端写进 tweak）+ mbedtls XTS 单元语义。"""
    assert len(data) % unit == 0, "XTS 长度必须是 sector_size 的整数倍"
    aes_data = AES.new(key32[:16], AES.MODE_ECB)
    aes_tweak = AES.new(key32[16:32], AES.MODE_ECB)
    out = bytearray()
    for i in range(0, len(data), unit):
        t = aes_tweak.encrypt((sector_start + i // unit).to_bytes(16, "big"))
        blk = data[i:i + unit]
        for j in range(0, len(blk), 16):
            c = blk[j:j + 16]
            out += _xor(aes_data.decrypt(_xor(c, t)), t)
            t = gf128mul_x_ble(t)
    return bytes(out)


def ctr_decrypt(key, ctr_bytes, data):
    """mbedtls_aes_crypt_ctr 语义：128 位大端计数器，密钥流块 = AES_enc(key, 计数器)。

    这里用 pycryptodome 的 CTR（nonce 为空 = 整个 16 字节都是计数器，按大端递增）一次算完；
    它与上面逐块手写的实现逐字节一致（本仓 tools 里核对过）。手写版 1 MB 要 0.26 s，
    RomFS 有 5.5 GB，必须走这条。
    """
    return AES.new(key, AES.MODE_CTR, nonce=b"", initial_value=int.from_bytes(ctr_bytes, "big")).decrypt(data)


def ecb_decrypt(key, data):
    return AES.new(key, AES.MODE_ECB).decrypt(data)


# ----------------------------------------------------------------------------- PFS0

class Pfs0:
    """hactool pfs0.h —— Switch 的简单归档（NSP 与 ExeFS 都用）。

    只解析头部；文件数据由调用方按自己的方式取（NSP 直接 fseek，ExeFS 要走 CTR 解密）。
    """

    def __init__(self, read):
        """read(offset, length) -> bytes，offset 相对 PFS0 起点。"""
        magic, self.num_files, self.strtab_size, _ = struct.unpack("<4sIII", read(0, 0x10))
        assert magic == b"PFS0", f"不是 PFS0（magic={magic!r}）"
        ent = read(0x10, 0x18 * self.num_files)
        self.strtab = read(0x10 + 0x18 * self.num_files, self.strtab_size)
        self.entries = []
        for i in range(self.num_files):
            off, size, nameoff, _ = struct.unpack_from("<QQII", ent, i * 0x18)
            end = self.strtab.index(b"\0", nameoff)
            self.entries.append((self.strtab[nameoff:end].decode(), off, size))
        self.header_size = 0x10 + 0x18 * self.num_files + self.strtab_size


# ----------------------------------------------------------------------------- NCA

class Nca:
    CONTENT_TYPES = {0: "Program", 1: "Meta", 2: "Control", 3: "Manual", 4: "Data", 5: "PublicData"}
    CRYPT = {0: "NONE?", 1: "NONE", 2: "XTS", 3: "CTR", 4: "BKTR"}

    def __init__(self, fh, offset, size, keys):
        self.fh, self.offset, self.size, self.keys = fh, offset, size, keys
        fh.seek(offset)
        self.header = xts_decrypt(keys["header_key"], fh.read(NCA_HEADER_SIZE), 0, MEDIA_SIZE)
        self.magic = self.header[0x200:0x204]
        assert self.magic in (b"NCA3", b"NCA2"), f"NCA 头解密失败（magic={self.magic!r}，检查 header_key）"
        self.distribution = self.header[0x204]
        self.content_type = self.header[0x205]
        self.crypto_type_raw = self.header[0x206]
        self.kaek_ind = self.header[0x207]
        self.nca_size = struct.unpack_from("<Q", self.header, 0x208)[0]
        self.title_id = struct.unpack_from("<Q", self.header, 0x210)[0]
        self.sdk_version = struct.unpack_from("<I", self.header, 0x21C)[0]
        self.crypto_type2 = self.header[0x220]
        self.rights_id = self.header[0x230:0x240]
        self.section_entries = [struct.unpack_from("<II", self.header, 0x240 + i * 0x10) for i in range(4)]
        self.encrypted_keys = self.header[0x300:0x340]
        # nca.c:430 —— crypto_type 取两者最大再减 1（0 与 1 都是 master key 0）
        ct = max(self.crypto_type_raw, self.crypto_type2)
        self.crypto_type = ct - 1 if ct else 0
        self._content_key = None

    # ---- 基本信息
    def info(self):
        return (f"title={self.title_id:016x} type={self.CONTENT_TYPES.get(self.content_type, self.content_type)} "
                f"file={self.size}B logical={self.nca_size}B crypto_type={self.crypto_type} kaek={self.kaek_ind} "
                f"rights_id={'-' if not any(self.rights_id) else self.rights_id.hex()} sdk={self.sdk_version:#x}")

    def read_raw(self, off, n):
        self.fh.seek(self.offset + off)
        return self.fh.read(n)

    def fs_header(self, sec):
        return self.header[0x400 + sec * 0x200: 0x400 + (sec + 1) * 0x200]

    def superblock(self, sec):
        return self.fs_header(sec)[0x08:]

    # ---- 密钥
    def decrypt_key_area(self):
        return ecb_decrypt(self.keys[f"key_area_key_application_{self.crypto_type:02x}"], self.encrypted_keys)

    def _ctr_base(self, sec):
        fsh = self.fs_header(sec)
        start, _ = self.section_entries[sec]
        sec_off = start * MEDIA_SIZE
        base = bytearray(16)
        base[0:8] = fsh[0x140:0x148][::-1]                       # section_ctr 反转
        base[8:16] = ((sec_off >> 4) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big")
        return int.from_bytes(bytes(base), "big"), sec_off

    def _probe(self, key, sec, delta, magic):
        """用候选密钥解段内 delta 处 16 字节，看 magic —— 决定性判据，不靠猜。"""
        try:
            base, sec_off = self._ctr_base(sec)
            ctr = ((base + (delta // 16)) & ((1 << 128) - 1)).to_bytes(16, "big")
            return ctr_decrypt(key, ctr, self.read_raw(sec_off + delta, 16))[:4] == magic
        except Exception:
            return False

    def first_pfs0_section(self):
        for i in range(4):
            fsh = self.fs_header(i)
            if self.section_entries[i][0] and fsh[2] == 1 and fsh[3] == 2:
                return i
        return None

    def pfs0_offset(self, sec):
        return struct.unpack_from("<Q", self.superblock(sec), 0x38)[0]

    def title_key(self, tik=None):
        """返回 (内容密钥, 来源说明)。带 rights_id 的 NCA 必须给 tik。"""
        if not any(self.rights_id):
            return self.decrypt_key_area()[0x20:0x30], "密钥区 key_area[2]（无 rights_id，nca.c:534）"
        assert tik, "该 NCA 带 rights_id，需要 NSP 里的 .tik"
        dec = self.decrypt_key_area()
        sec = self.first_pfs0_section()
        delta = self.pfs0_offset(sec)
        tk = self.keys.get(f"titlekek_{self.crypto_type:02x}")
        indices = [self.crypto_type] + [i for i in range(0x20) if i != self.crypto_type]
        for ki in indices:
            k = self.keys.get(f"titlekek_{ki:02x}")
            if not k:
                continue
            for to in range(0, len(tik) - 15, 16):
                cand = ecb_decrypt(k, tik[to:to + 16])
                for label, key in (("直接", cand), ("异或密钥区[2]", _xor(cand, dec[0x20:0x30]))):
                    if self._probe(key, sec, delta, b"PFS0"):
                        if ki != self.crypto_type:
                            print(f"  [注意] 实际用的是 titlekek_{ki:02x}，头里的 crypto_type 推出的是 {self.crypto_type:02x}")
                        return key, f"titlekek_{ki:02x} 解 tik[0x{to:x}]（{label}，nca.c:462）"
        raise RuntimeError("无法从 ticket 解出内容密钥")

    # ---- 段读取（CTR 流式；XTS 按 0x200 单元）
    def section_read(self, sec, delta, n):
        start, end = self.section_entries[sec]
        sec_off, sec_size = start * MEDIA_SIZE, (end - start) * MEDIA_SIZE
        assert 0 <= delta and delta + n <= sec_size, "段内越界"
        fsh = self.fs_header(sec)
        crypt = fsh[4]
        if crypt in (3, 4):                       # CTR
            aligned = delta & ~0xF
            pad = delta - aligned
            raw = self.read_raw(sec_off + aligned, n + pad)
            base, _ = self._ctr_base(sec)
            ctr = ((base + (aligned // 16)) & ((1 << 128) - 1)).to_bytes(16, "big")
            out = ctr_decrypt(self._content_key, ctr, raw)
            return out[pad:pad + n]
        if crypt in (2, 0xFF):                    # XTS
            aligned = delta & ~(MEDIA_SIZE - 1)
            pad = delta - aligned
            cnt = ((n + pad + MEDIA_SIZE - 1) // MEDIA_SIZE) * MEDIA_SIZE
            raw = self.read_raw(sec_off + aligned, cnt)
            out = xts_decrypt(self.decrypt_key_area()[:32], raw, (sec_off + aligned) // MEDIA_SIZE, MEDIA_SIZE)
            return out[pad:pad + n]
        return self.read_raw(sec_off + delta, n)  # 未加密


# ----------------------------------------------------------------------------- NSP

def open_nsp(path):
    fh = open(path, "rb")
    magic, num, strsz, _ = struct.unpack("<4sIII", fh.read(0x10))
    assert magic == b"PFS0", "不是有效的 NSP（PFS0）"
    ent = fh.read(0x18 * num)
    strtab = fh.read(strsz)
    data_start = 0x10 + 0x18 * num + strsz
    files = []
    for i in range(num):
        off, size, nameoff, _ = struct.unpack_from("<QQII", ent, i * 0x18)
        end = strtab.index(b"\0", nameoff)
        files.append((strtab[nameoff:end].decode(), data_start + off, size))
    return fh, files


def _read_tik(fh, files):
    for name, off, size in files:
        if name.endswith(".tik"):
            fh.seek(off)
            return fh.read(size)
    return None


def _open_program_nca(nsp, keyfile):
    keys = load_keys(keyfile)
    fh, files = open_nsp(nsp)
    tik = _read_tik(fh, files)
    for name, off, size in files:
        if name.endswith(".nca"):
            nca = Nca(fh, off, size, keys)
            if nca.content_type == 0:
                nca._content_key, how = nca.title_key(tik)
                return fh, nca, how, name
    raise RuntimeError("NSP 里没有 Program NCA")


# ----------------------------------------------------------------------------- 子命令

def cmd_info(nsp, keyfile):
    keys = load_keys(keyfile)
    fh, files = open_nsp(nsp)
    print(f"NSP: {os.path.basename(nsp)}  条目 {len(files)}")
    for name, off, size in files:
        print(f"  {name:44s} off=0x{off:x} size={size}")
    tik = _read_tik(fh, files)
    for name, off, size in files:
        if not name.endswith(".nca"):
            continue
        nca = Nca(fh, off, size, keys)
        try:
            key, how = nca.title_key(tik)
            nca._content_key = key
            note = f"\n  内容密钥: {how}"
        except Exception as e:
            note = f"\n  内容密钥: 未知（{e}）"
        print(f"\nNCA {name}\n  {nca.info()}{note}")
        for i in range(4):
            start, end = nca.section_entries[i]
            if not start and not end:
                continue
            fsh = nca.fs_header(i)
            ptype = {0: "ROMFS", 1: "PFS0"}.get(fsh[2], fsh[2])
            ftype = {2: "PFS0", 3: "ROMFS"}.get(fsh[3], fsh[3])
            print(f"  段{i}: 文件 0x{start*MEDIA_SIZE:x}-0x{end*MEDIA_SIZE:x} "
                  f"({end*MEDIA_SIZE-start*MEDIA_SIZE} B) 分区={ptype} fs={ftype} 加密={Nca.CRYPT.get(fsh[4], fsh[4])}")
            if ftype == "PFS0":
                print(f"       PFS0 于段内 0x{nca.pfs0_offset(i):x}")
            if ftype == "ROMFS":
                ivfc = nca.superblock(i)
                if ivfc[:4] == b"IVFC":
                    num_levels, mhs = struct.unpack_from("<II", ivfc, 0x0C)[0], struct.unpack_from("<I", ivfc, 0x08)[0]
                    print(f"       IVFC levels={num_levels} master_hash_size={mhs}")
                    for lv in range(num_levels):
                        lo, ds, bs, _ = struct.unpack_from("<QQII", ivfc, 0x10 + lv * 0x18)
                        print(f"         L{lv}: offset=0x{lo:x} size={ds} block={1 << bs}")
                else:
                    print(f"       !! IVFC magic 不对: {ivfc[:4]!r}")
    fh.close()


def cmd_exefs(nsp, keyfile, outdir):
    fh, nca, how, name = _open_program_nca(nsp, keyfile)
    print(f"[NCA {name}]\n  {nca.info()}\n  内容密钥: {how}")
    sec = nca.first_pfs0_section()
    poff = nca.pfs0_offset(sec)
    print(f"  ExeFS: 段{sec}，段内 PFS0 偏移 0x{poff:x}")
    p = Pfs0(lambda o, n: nca.section_read(sec, poff + o, n))
    os.makedirs(outdir, exist_ok=True)
    for fn, foff, fsize in sorted(p.entries, key=lambda e: -e[2]):
        data = nca.section_read(sec, poff + p.header_size + foff, fsize)
        dst = os.path.join(outdir, fn)
        with open(dst, "wb") as o:
            o.write(data)
        magic = data[:4]
        print(f"    {fn:16s} {fsize:10d} B  magic={magic!r:12s} sha256={hashlib.sha256(data).hexdigest()[:24]}")
    fh.close()


def cmd_romfs(nsp, keyfile, outdir):
    fh, nca, how, name = _open_program_nca(nsp, keyfile)
    sec = None
    for i in range(4):
        fsh = nca.fs_header(i)
        if nca.section_entries[i][0] and fsh[2] == 0 and fsh[3] == 3:
            sec = i
            break
    assert sec is not None, "没有 RomFS 段"
    ivfc = nca.superblock(sec)
    assert ivfc[:4] == b"IVFC", f"IVFC 头不对 {ivfc[:4]!r}"
    num_levels = struct.unpack_from("<I", ivfc, 0x0C)[0]
    levels = [struct.unpack_from("<QQII", ivfc, 0x10 + lv * 0x18) for lv in range(6)]
    # hactool nca_process_ivfc_section：RomFS 数据一律在第 IVFC_MAX_LEVEL-1 = 5 层（不是 num_levels-1：
    # SMO 的 Control NCA num_levels=7 而第 6 层是空的）
    data_off = levels[5][0]
    print(f"  IVFC levels={num_levels} 数据层 L5 偏移 0x{data_off:x} 大小 {levels[5][1]}")
    print(f"[NCA {name}] RomFS 段{sec} IVFC levels={num_levels} 数据层偏移 0x{data_off:x}")
    h = nca.section_read(sec, data_off, 0x50)
    (hdr_size, dh_off, dh_sz, dm_off, dm_sz, fh_off, fh_sz, fm_off, fm_sz, fdata_off) = struct.unpack("<QQQQQQQQQQ", h)
    print(f"  头: hdr={hdr_size} dir_meta=0x{dm_off:x}+{dm_sz} file_meta=0x{fm_off:x}+{fm_sz} data=0x{fdata_off:x}")
    dirs = nca.section_read(sec, data_off + dm_off, dm_sz)
    fent = nca.section_read(sec, data_off + fm_off, fm_sz)

    def dir_entry(o):
        parent, sibling, child, file, _h, nsz = struct.unpack_from("<IIIIII", dirs, o)
        return parent, sibling, child, file, dirs[o + 0x18:o + 0x18 + nsz].decode(errors="replace")

    def file_entry(o):
        parent, sibling, doff, dsize, _h, nsz = struct.unpack_from("<IIQQII", fent, o)
        return parent, sibling, doff, dsize, fent[o + 0x20:o + 0x20 + nsz].decode(errors="replace")

    os.makedirs(outdir, exist_ok=True)
    nfiles = nbytes = 0
    stack = [("", 0)]
    while stack:
        rel, doff = stack.pop()
        _, _, child, file, dname = dir_entry(doff)
        cur = os.path.join(outdir, rel, dname) if dname else outdir
        os.makedirs(cur, exist_ok=True)
        o = file
        while o != 0xFFFFFFFF:
            _, sibling, foff, fsize, fname = file_entry(o)
            dst = os.path.join(cur, fname)
            remaining, written = fsize, 0
            with open(dst, "wb") as out:
                while remaining:
                    chunk = nca.section_read(sec, data_off + fdata_off + foff + written, min(1 << 22, remaining))
                    out.write(chunk)
                    written += len(chunk)
                    remaining -= len(chunk)
            nfiles += 1
            nbytes += fsize
            o = sibling
        c = child
        while c != 0xFFFFFFFF:
            _, sibling, _, _, cname = dir_entry(c)
            stack.append((os.path.join(rel, dname) if dname else "", c))
            c = sibling
    print(f"RomFS 提取完成: {nfiles} 文件 / {nbytes} 字节 → {outdir}")
    fh.close()


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    cmd, nsp, keyfile = sys.argv[1], sys.argv[2], sys.argv[3]
    if cmd == "info":
        cmd_info(nsp, keyfile)
    elif cmd == "exefs":
        cmd_exefs(nsp, keyfile, sys.argv[4])
    elif cmd == "romfs":
        cmd_romfs(nsp, keyfile, sys.argv[4])
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
