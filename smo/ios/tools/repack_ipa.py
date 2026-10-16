#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 Guest.framework 塞进下载好的 IPA —— 省去每次手动解压/重压。

以后每次更新就三步：
  1. 下载新 release 的 SMOMobile-shell.ipa
  2. python smo/ios/tools/repack_ipa.py <下载的.ipa> out/smo-phone-package/①放进App安装包/Frameworks/Guest.framework
  3. 用你的自签工具签 `SMOMobile-shell-with-guest.ipa` → 覆盖安装（数据保留）

用法：repack_ipa.py <ipa 路径> <Guest.framework 路径> [输出路径]
"""
import os
import shutil
import sys
import zipfile


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    ipa = sys.argv[1]
    fw = sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.splitext(ipa)[0] + "-with-guest.ipa"

    # 找 Payload/*.app
    with zipfile.ZipFile(ipa) as z:
        names = z.namelist()
        apps = sorted(n for n in names if "/" in n and n.startswith("Payload/") and n.endswith(".app/"))
        if not apps:
            print(f"🔴 {ipa} 里没有 Payload/*.app —— 文件对吗？")
            return 2
        appdir = apps[0].rstrip("/")
        print(f"App 目录：{appdir}")

        # 写新 zip：原文件原样拷贝，另加 Frameworks/Guest.framework/**
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as nz:
            copied = 0
            for item in names:
                if item.startswith(appdir + "/Frameworks/"):
                    continue                     # 旧框架（如有）整个替换
                nz.writestr(item, z.read(item))
                copied += 1
            base = appdir + "/Frameworks/Guest.framework/"
            for root, _, files in os.walk(fw):
                for f in files:
                    full = os.path.join(root, f)
                    rel = base + os.path.relpath(full, fw).replace(os.sep, "/")
                    nz.write(full, rel)
            print(f"原文件 {copied} 个 + Guest.framework（{os.path.getsize(os.path.join(fw, 'Guest'))} 字节）")

    print(f"✅ 已写出 {out}（{os.path.getsize(out)} 字节）—— 拿它去自签覆盖安装即可")
    return 0


if __name__ == "__main__":
    sys.exit(main())
