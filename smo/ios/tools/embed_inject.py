#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把游戏程序**内嵌**进 App（参考项目同路线：编进主二进制自己的段，签名一把过）。

为什么不用单独的 Guest.framework：那是一个 58 MB 的独立 dylib，签名工具要**单独签它** ——
实测全能签做不到（它自己报「大前 Mach-O 准备失败（FAT/tail-append/大库/换壳 Framework）」），
装出来的包要么装不上、要么闪退。参考项目能用的包走的是**内嵌**：映像作为节编进 App 主二进制，
签名工具只签一个二进制。

用法（CI 上由 workflow 调）：
    embed_inject.py <输入包.zip> <仓库根>
输入包内容：改写产物（`<模块>.patched.bin` + `.fixups.tsv` + `.gates.tsv`）。
脚本做的事：
  1. 解到临时目录
  2. `gen_embed.py` 生成 `<仓库根>/smo/ios/Sources/Embedded.S` 与同名的 `.c`（内嵌表）
  3. 把 `GCC_PREPROCESSOR_DEFINITIONS: SMO_EMBED=1` 与 `OTHER_LDFLAGS: <-segprot …>` 写进 project.yml
"""
import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

# Windows 控制台默认编码是 GBK：本文件与 gen_embed 都要打中文/emoji，直接 print 会 UnicodeEncodeError。
# 编码不动、不能表示的字符替换掉（GBK 下 emoji 变 '?'，中文照常）；CI（UTF-8）行为不变。
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(errors="replace")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    zip_path, root = sys.argv[1], sys.argv[2]
    mods = sys.argv[3:] or ["sdk", "subsdk0", "main"]

    tmp = tempfile.mkdtemp(prefix="embed_in_")
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(tmp)
    missing = [m for m in mods
               if not all(os.path.exists(os.path.join(tmp, f"{m}.{e}"))
                          for e in ("patched.bin", "fixups.tsv", "gates.tsv"))]
    if missing:
        print(f"🔴 输入包里缺：{missing}")
        return 2

    dst = os.path.join(root, "smo", "ios", "Sources", "Embedded.S")
    # 强制子进程用 UTF-8 写 stdout：否则 Windows 上它是 GBK，下面按 UTF-8 解码会得到替换字符
    env = dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUTF8="1")
    r = subprocess.run([sys.executable, os.path.join(HERE, "gen_embed.py"), tmp, dst] + mods,
                       env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = (r.stdout or b"").decode("utf-8", "replace").strip()
    err = (r.stderr or b"").decode("utf-8", "replace").strip()
    if out:
        print(out)
    if r.returncode:
        if err:
            print(err)
        print("🔴 生成内嵌段失败（边界页检查拒绝出包？见上面的理由）")
        return 3
    flags = out.splitlines()[-1].strip()

    p = os.path.join(root, "smo", "ios", "project.yml")
    s = open(p, encoding="utf-8").read()
    if "GCC_PREPROCESSOR_DEFINITIONS" not in s:
        add = ('    GCC_PREPROCESSOR_DEFINITIONS: "SMO_EMBED=1"\n'
               f'    OTHER_LDFLAGS: "{flags}"\n')
        anchor = '    CLANG_ENABLE_OBJC_ARC: "YES"\n'
        if anchor not in s:
            print("🔴 project.yml 里找不到插入点")
            return 4
        s = s.replace(anchor, anchor + add, 1)
        open(p, "w", encoding="utf-8", newline="\n").write(s)
    print(f"已写入 {dst} 与 .c 表；工程设置注入 SMO_EMBED=1、{flags}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
