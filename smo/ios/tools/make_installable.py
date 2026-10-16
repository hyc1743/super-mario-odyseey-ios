#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""一条命令出「可直接自签安装」的 IPA。

流程（全程本地，不需要网盘，不需要私有 fork）：
  1. 从公共仓下载最新 release 的 SMOMobile-shell.ipa（壳，不含游戏内容）
  2. 用本机 out/smo-phone-package/①放进App安装包/Frameworks/Guest.framework
     （你自己的游戏程序，之前用 prepare_phone_package.py 生成）注入进去
  3. 输出 <输出目录>/SMOMobile-可安装.ipa —— 拿给自签工具签名即可

以后每次更新：重跑本命令 → 自签 → **覆盖安装**（手机上的游戏资源保留，不用重拷）。

用法：make_installable.py [输出目录（默认 out）] [--smb-root \192.168.1.4\Files\SMO] [--resources]
  --smb-root：出完 IPA 后自动复制到 <root>/ipa/；若 <root>/gamedata/ 还不存在，
              顺带把手机资源包复制过去（以后重跑默认复用上次记住的路径）
  --resources：即使 gamedata 已存在也强制重拷资源（资源固定，平时不用）
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
    out = os.path.join(ROOT, "out")
    smb = None
    force_res = False
    for i, a in enumerate(argv):
        if a == "--smb-root" and i + 1 < len(argv):
            smb = argv[i + 1]
        elif a == "--resources":
            force_res = True
        elif not a.startswith("--") and i == 0:
            out = a
    # 记住上次的 smb 路径（本地工作流：重跑一条命令即可）
    cfg = os.path.join(ROOT, "out", "smb_root.cfg")
    if not smb and os.path.exists(cfg):
        smb = open(cfg, encoding="utf-8").read().strip()
    if smb:
        open(cfg, "w", encoding="utf-8", newline=chr(10)).write(smb)
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

    # 4) 复制到 SMB（可选）
    if smb:
        import shutil
        ipadir = os.path.join(smb, "ipa")
        os.makedirs(ipadir, exist_ok=True)
        shutil.copy2(dst, os.path.join(ipadir, os.path.basename(dst)))
        print(f"4. IPA → {ipadir}")
        gamedata = os.path.join(smb, "gamedata")
        src = os.path.join(ROOT, "out", "smo-phone-package", "②复制到手机")
        if force_res or not os.path.exists(gamedata):
            if os.path.isdir(src):
                print(f"4. 资源 → {gamedata}（5.2 GB，视网络速度需几分钟到十几分钟）")
                if os.path.exists(gamedata):
                    shutil.rmtree(gamedata)
                shutil.copytree(src, gamedata)
                print(f"4. 资源复制完成 → {gamedata}")
            else:
                print("4. 本地没有资源包（先跑 prepare_phone_package.py）")
        else:
            print("4. gamedata 已存在，跳过资源（--resources 可强制重拷）")

    print()
    print("完成：拿这个文件去自签工具签名，覆盖安装即可。")
    print("以后更新：重跑 python smo/ios/tools/make_installable.py → 自签 → 覆盖安装。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
