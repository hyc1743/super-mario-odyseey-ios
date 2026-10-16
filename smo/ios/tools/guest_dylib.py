#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把改写产物打包成一个**只装节段的 Guest 动态库**（供 App 内嵌装载的 DM_SHELL 模式用）。

为什么需要它：CI 出的壳**不含任何游戏内容**；而 iOS 上只有签名代码段能执行，从文件 mmap 的
代码不能跑。于是：你在**本机**（Windows 就行，纯 Python 写 Mach-O）用本脚本把
`work/rewrite/<模块>.patched.bin` 打包成 `Guest.framework/Guest`（只含三段的 arm64 动态库），
放进 App 安装包（.app/Frameworks/Guest.framework/），整包用 Sideloadly 签名。
App 启动后装载器 dlopen 它、按节名把三段 vm_remap 回连续地址（签名页可执行）。

派生自参考项目 d2r-mobile 的 dm/tools/guest_dylib.py（MIT）；节名与内嵌做法一致
（__TEXT,__dm<模块>x0 / __DM<模块>W,__img / __TEXT,__dm<模块>x1），所以装载器不需要两套逻辑。

用法：guest_dylib.py <产物目录(work/rewrite)> <输出目录> <模块名...>
      例：python smo/ios/tools/guest_dylib.py work/rewrite out rtld sdk subsdk0 main
产出：<输出目录>/Guest.framework/Guest 与 Info.plist
"""
import hashlib
import os
import struct
import sys

PG = 0x4000
MIN_IOS = 26 << 16          # 与工程的最低系统版本一致（ios/project.yml）


def r16(x):
    return (x + PG - 1) & ~(PG - 1)


def s16(s):
    assert len(s) <= 16, s
    return s.encode().ljust(16, b'\0')


def segment(name, addr, size, fileoff, filesize, prot, sects):
    b = struct.pack('<II16sQQQQiiII', 0x19, 72 + 80 * len(sects), s16(name), addr, size,
                    fileoff, filesize, prot, prot, len(sects), 0)
    for sn, a, sz, flags in sects:
        # 文件偏移 = 地址；对齐 2^14（与参考项目同口径）
        b += struct.pack('<16s16sQQIIIIIIII', s16(sn), s16(name), a, sz, a, 14, 0, 0, flags, 0, 0, 0)
    return b


def dylib_cmd(cmd, name):
    return struct.pack('<II', cmd, 24 + ((len(name) + 8) & ~7)) + name + b'\0' * ((8 - len(name) % 8) % 8)


def build(out, names):
    text, wsegs = [], []
    for n in names:
        hdr = {}
        for l in open(os.path.join(out, n + '.fixups.tsv'), encoding='utf-8'):
            if not l.startswith('# '):
                continue
            p = l[2:].split()
            if len(p) == 2:
                hdr[p[0]] = p[1]
        te, po = int(hdr['TEXT_END'], 16), int(hdr['PATCH_OFF'], 16)
        img = open(os.path.join(out, n + '.patched.bin'), 'rb').read()
        sz = len(img)
        a = (te // PG) * PG       # 向下取整：代码尾与缝隙同页（rtld 的 30 条重定位全在缝隙里，见 STATUS）
        end = r16(sz)
        assert po % PG == 0 and a <= po <= end, (n, hex(te), hex(po), hex(sz))
        def cut(lo, hi):
            return img[lo:min(hi, sz)].ljust(hi - lo, b'\0')
        text += [('__dm%sx0' % n, cut(0, a)), ('__dm%sx1' % n, cut(po, end))]
        wsegs.append(('__DM%sW' % n, cut(a, po)))
    off = PG
    tsects = []
    body = b''
    for sn, b in text:
        tsects.append((sn, off, len(b), 0x80000400))     # 纯指令节
        body += b
        off += len(b)
    cmds = [segment('__TEXT', 0, off, 0, off, 5, tsects)]
    for sn, b in wsegs:
        cmds.append(segment(sn, off, len(b), off, len(b), 3, [('__img', off, len(b), 0)]))
        body += b
        off += len(b)
    strtab = b'\0' * 8
    # 空代码签名槽（LC_CODE_SIGNATURE）：没有它，某些签名工具在「签前 Mach-O 准备」阶段
    # 会把 dylib 判成异常并拒签（全能签报「Guest 单独签名失败」就是这个）。
    # 16 字节空签名放进 __LINKEDIT，codesign 会原位替换。
    emptysig = b'\0' * 16
    sigoff = off + len(strtab)
    uuid = hashlib.sha256(body).digest()[:16]            # 由内容定：同样输入在哪台机器出库都逐字节相同
    cmds += [segment('__LINKEDIT', off, PG, off, len(strtab) + 16, 1, []),
             struct.pack('<IIII', 0x1d, 16, sigoff, 16),  # LC_CODE_SIGNATURE（空）
             dylib_cmd(0xd, b'@rpath/Guest.framework/Guest'),                     # LC_ID_DYLIB
             struct.pack('<II10I', 0x80000022, 48, *([0] * 10)),                  # LC_DYLD_INFO_ONLY
             struct.pack('<IIIIII', 0x2, 24, off, 0, off, len(strtab)),           # LC_SYMTAB：0 符号
             struct.pack('<II18I', 0xb, 80, *([0] * 18)),                         # LC_DYSYMTAB
             struct.pack('<II16s', 0x1b, 24, uuid),                               # LC_UUID
             struct.pack('<IIIIII', 0x32, 24, 2, MIN_IOS, MIN_IOS, 0),            # LC_BUILD_VERSION：iOS
             dylib_cmd(0xc, b'/usr/lib/libSystem.B.dylib')]                       # LC_LOAD_DYLIB
    lc = b''.join(cmds)
    assert 32 + len(lc) <= PG
    mh = struct.pack('<IiiIIIII', 0xfeedfacf, 0x0100000c, 0, 6, len(cmds), len(lc), 0x00100085, 0)
    return (mh + lc).ljust(PG, b'\0') + body + strtab + emptysig


INFO_PLIST = '''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>Guest</string>
<key>CFBundleIdentifier</key><string>%s</string>
<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
<key>CFBundleName</key><string>Guest</string>
<key>CFBundlePackageType</key><string>FMWK</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>CFBundleVersion</key><string>1</string>
<key>MinimumOSVersion</key><string>26.0</string>
<key>CFBundleSupportedPlatforms</key><array><string>iPhoneOS</string></array>
</dict></plist>
'''


def write(out, dst, names, bundle_id='local.guest'):
    fw = os.path.join(dst, 'Guest.framework')
    os.makedirs(fw, exist_ok=True)
    data = build(out, names)
    with open(os.path.join(fw, 'Guest'), 'wb') as f:
        f.write(data)
    with open(os.path.join(fw, 'Info.plist'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(INFO_PLIST % bundle_id)
    return fw, len(data)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    fw, n = write(sys.argv[1], sys.argv[2], sys.argv[3:])
    print('%s：%d 字节' % (fw, n))
