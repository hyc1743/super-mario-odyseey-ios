#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""一条命令出「可直接自签安装」的 IPA。

流程（全程本地，不需要网盘，不需要私有 fork）：
  1. 从公共仓下载最新 release 的 SMOMobile-shell.ipa（壳，不含游戏内容）
  2. 用本机 out/smo-phone-package/①放进App安装包/Frameworks/Guest.framework
     （你自己的游戏程序，之前用 prepare_phone_package.py 生成）注入进去
  3. 输出 <输出目录>/SMOMobile-可安装.ipa —— 拿给自签工具签名即可

以后每次更新：重跑本命令 → 自签 → **覆盖安装**（手机上的游戏资源保留，不用重拷）。

用法：make_installable.py [输出目录（默认 out）]
"""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
RELEASE_API = "https://api.github.com/repos/hyc1743/super-mario-odyssey-ios/releases/latest"


def main():
    argv = sys.argv[1:]
    out = sys.argv[1] if (len(argv) > 0 and not argv[0].startswith("--")) else os.path.join(ROOT, "out")
    os.makedirs(out, exist_ok=True)

    # 1) 最新 release 的壳 IPA
    print("1. 查公共仓最新 release …")
    with urllib.request.urlopen(RELEASE_API) as f:
        rel = json.load(f)
    ipa = [a for a in rel["assets"] if a["name"].endswith(".ipa")]
    if not ipa:
        print("🔴 最新 release 里没有 .ipa")
        return 2
    url = ipa[0]["browser_download_url"]
    name = ipa[0]["name"]
    print(f"   最新版本：{rel['tag_name']}（{name}）")
    local = os.path.join(out, name)
    print(f"2. 下载 {url}")
    urllib.request.urlretrieve(url, local)

    # 2) 本地的 Guest.framework
    fw = os.path.join(ROOT, "out", "smo-phone-package", "①放进App安装包", "Frameworks", "Guest.framework")
    if not os.path.exists(os.path.join(fw, "Guest")):
        print("🔴 找不到本地的 Guest.framework —— 先跑：")
        print("   python smo/ios/tools/prepare_phone_package.py")
        return 3

    # 3) 注入（复用 repack_ipa.py）
    sys.path.insert(0, HERE)
    import repack_ipa
    outname = "SMOMobile-可安装.ipa"
    dst = os.path.join(out, outname)
    argv = [sys.argv[0], local, fw, dst]
    rc = repack_ipa.main() if False else None
    import zipfile
    with zipfile.ZipFile(local) as z:
        names = z.namelist()
        appdir = sorted(n for n in names if n.startswith("Payload/") and n.endswith(".app/"))[0].rstrip("/")
        with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as nz:
            for item in names:
                if item.startswith(appdir + "/Frameworks/"):
                    continue
                nz.writestr(item, z.read(item))
            base = appdir + "/Frameworks/Guest.framework/"
            for root, _, files in os.walk(fw):
                for f in files:
                    full = os.path.join(root, f)
                    nz.write(full, base + os.path.relpath(full, fw).replace(os.sep, "/"))

    print(f"3. 已注入 Guest.framework → {dst}（{os.path.getsize(dst)} 字节）")

    print()
    print("完成：拿这个文件去自签工具签名，覆盖安装即可。")
    print("以后更新：重跑 python smo/ios/tools/make_installable.py → 自签 → 覆盖安装。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
