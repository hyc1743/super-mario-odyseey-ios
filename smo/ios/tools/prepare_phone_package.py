#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把首跑需要的全部东西整理成「直接可用」的包。

产出结构（out/smo-phone-package/）：

    ①放进App安装包/Frameworks/Guest.framework/    ← 解压 IPA 后放进 SMOMobile.app/Frameworks/，
                                                    重打包签名（guest dylib：真机上唯一能执行的映像）
    ①放进App安装包/Frameworks/Guest.framework/    ← 解压 IPA 后放进 SMOMobile.app/Frameworks/，
                                                    重打包签名（guest dylib：真机上唯一能执行的映像）
    SMO Resource/                                   ← **一个文件夹**：ExeFS + RomFS 都在里面，
                                                    手机上一次复制完成（RomFS 巨大，--romfs 指定源才拷）

用法：
    prepare_phone_package.py [--rewrite work/rewrite] [--out out/smo-phone-package]
                             [--romfs work/game/RomFS]     # 给了就硬拷 RomFS（慢）
                             [--pack-romfs]                # 不拷几千个小文件，打成 RomFS.smoa.0/.1 两卷
                                                           # （App 首次启动自动展开，拷手机只有两个文件）
                             [--no-dylib]                  # 跳过 Guest.framework
                             [--guest-zip <路径>]           # 额外把 Guest.framework 压成 zip 并打印 sha256
                                                             #（上传到任意网盘/URL，供私有 fork 的 CI 注入）
"""
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))


def main():
    rewrite = os.path.join(ROOT, "work", "rewrite")
    out = os.path.join(ROOT, "out", "smo-phone-package")
    romfs = None
    no_dylib = False
    argv = sys.argv[1:]
    pack_romfs = "--pack-romfs" in argv
    for i, a in enumerate(argv):
        if a == "--rewrite" and i + 1 < len(argv):
            rewrite = argv[i + 1]
        if a == "--out" and i + 1 < len(argv):
            out = argv[i + 1]
        if a == "--romfs" and i + 1 < len(argv):
            romfs = argv[i + 1]
        if a == "--no-dylib":
            no_dylib = True

    need = ["sdk", "subsdk0", "main"]          # 不含 rtld：宿主扮演它（x0_layout.py）
    missing = [n for n in need
               if not (os.path.exists(os.path.join(rewrite, n + ".fixups.tsv"))
                       and os.path.exists(os.path.join(rewrite, n + ".gates.tsv")))]
    if missing:
        print(f"🔴 {rewrite} 里缺：{missing}（先跑 patch_nso.py 生成改写产物）")
        return 2

    exefs_out = os.path.join(out, "SMO Resource", "ExeFS")
    fw_out = os.path.join(out, "①放进App安装包", "Frameworks", "Guest.framework")
    os.makedirs(exefs_out, exist_ok=True)
    os.makedirs(fw_out, exist_ok=True)

    # ① guest dylib
    guest_zip = None
    for i, a in enumerate(argv):
        if a == "--guest-zip" and i + 1 < len(argv):
            guest_zip = argv[i + 1]

    if not no_dylib:
        sys.path.insert(0, os.path.join(HERE))
        import guest_dylib
        data = guest_dylib.build(rewrite, need)
        with open(os.path.join(fw_out, "Guest"), "wb") as f:
            f.write(data)
        with open(os.path.join(fw_out, "Info.plist"), "w", encoding="utf-8", newline="\n") as f:
            f.write(guest_dylib.INFO_PLIST % "local.guest")
        print(f"① Guest.framework → {fw_out}（{len(data)} 字节）")
        if guest_zip:
            # 给 CI「内嵌路线」用的输入包：改写产物本身（patched.bin + tsv），CI 上现场生成内嵌段
            import hashlib
            import zipfile
            with zipfile.ZipFile(guest_zip, "w", zipfile.ZIP_DEFLATED) as z:
                for n in need:
                    for ext in ("patched.bin", "fixups.tsv", "gates.tsv"):
                        f = os.path.join(rewrite, f"{n}.{ext}")
                        if os.path.exists(f):
                            z.write(f, f"{n}.{ext}")
            h = hashlib.sha256(open(guest_zip, "rb").read()).hexdigest()
            print(f"① embed 输入包 → {guest_zip}（{os.path.getsize(guest_zip)} 字节）")
            print(f"① sha256 = {h}   ← 私有 fork 的 GUEST_SHA256 填这个，GUEST_URL 填它的下载地址")

    # ② RomFS：巨大，默认只写说明；--pack-romfs 时打成卷（两个大文件）
    romfs_out = os.path.join(out, "SMO Resource", "RomFS")
    if pack_romfs and romfs and os.path.isdir(romfs):
        import zlib
        files = []
        for dp, _, fns in os.walk(romfs):
            for fn in fns:
                full = os.path.join(dp, fn)
                files.append((os.path.relpath(full, romfs).replace(os.sep, "/"), full))
        files.sort()
        vol_limit = 3 * 1024 * 1024 * 1024
        vols = [[]]                     # 每卷的条目列表
        cur = 0
        for rel, full in files:
            sz = os.path.getsize(full)
            if vols[-1] and cur + sz > vol_limit:
                vols.append([])
                cur = 0
            vols[-1].append((rel, full))
            cur += sz
        if os.path.exists(romfs_out):
            shutil.rmtree(romfs_out)
        os.makedirs(romfs_out)
        for vi, vol in enumerate(vols):
            with open(os.path.join(romfs_out, f"RomFS.smoa.{vi}"), "wb") as vf:
                vf.write(b"SMOA" + bytes([vi, len(vols)]))
                for rel, full in vol:
                    data = open(full, "rb").read()
                    vf.write(struct.pack("<HII", len(rel.encode()), len(data), zlib.crc32(data) & 0xFFFFFFFF))
                    vf.write(rel.encode())
                    vf.write(data)
                vf.write(b"END!")
        print(f"② RomFS 打成 {len(vols)} 卷 → {romfs_out}（{len(files)} 个文件）")
    elif romfs and os.path.isdir(romfs):
        if os.path.exists(romfs_out):
            shutil.rmtree(romfs_out)
        shutil.copytree(romfs, romfs_out)
        print(f"② RomFS 已复制 → {romfs_out}")
    else:
        print(f"② RomFS：未复制（5.2 GB）—— 直接把 {romfs or 'work/game/RomFS'} 整个拷进手机的 SMOMobile 文件夹即可")

    # ③ 说明
    readme = os.path.join(out, "③说明.txt")
    with open(readme, "w", encoding="utf-8", newline="\n") as f:
        f.write("""SMO 首跑包 —— 分三步
========================

【一】把 Guest.framework 放进 App 安装包
  1. 解压 SMOMobile-shell.ipa（改后缀 .zip 再解压）
  2. 把本包「①放进App安装包/Frameworks/Guest.framework」整个放进
     Payload/SMOMobile.app/Frameworks/ 里面
  3. 重新压缩 Payload 目录为 zip，改名 .ipa
  4. Sideloadly 签名安装（你的 Apple ID；免费 7 天，到期重签重装，数据不丢）

【二】把数据拷到手机
  1. 先打开一次 App（让它在「文件」里出现；v0.3.0 起才开了文件共享，旧版没有这个目录）
  2. 用「文件」App：我的 iPhone → SMOMobile 文件夹
  3. 把「SMO Resource」**整个文件夹**拷进去（一层包装目录，App 认）

【三】跑起来、拿日志
  1. 打开 App：宿主自动装载 → 屏幕日志滚动
  2. 点右下角「导出日志」→ 存文件并拉起分享 → AirDrop/邮件发给开发者
  3. 期待行为：日志滚到某个 UNIMPL/FATAL 停住 —— 那就是要修的第一个点

【注意】本包不含任何游戏内容（RomFS 由你自己的游戏提取）；项目与 Nintendo 无关。
""")
    print(f"③ 说明 → {readme}")
    print("\n完成。把「②」拷进手机，「①」放进 IPA 重签名即可。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
