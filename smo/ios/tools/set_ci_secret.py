#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把令牌写进 GitHub 仓库的 Actions secret（一次配置，之后 CI 直接读）。

为什么要有这个脚本：网页上加 secret 要点很多步，而 API 只要一次 PUT。这里把
「取仓库公钥 → libsodium sealed box 加密 → PUT」固化下来 —— 令牌换了重跑一次即可，
也避免把令牌写进任何文件（值只从环境变量读）。

用法：
    SMO_CI_TOKEN=<令牌> python smo/ios/tools/set_ci_secret.py <owner/repo> <secret名>

依赖：pip install pynacl

约定（本项目 CI 用的名字）：
    SMO_PRIVATE_TOKEN   公开仓的 workflow 用它去私有仓拉输入包、把内嵌版 IPA 传回私有仓 release
"""
import base64
import json
import os
import sys
import urllib.request

# Windows 控制台默认 GBK：本脚本要打 ✅/🔴，不守卫就会把自己的成功打成失败（本机实测过一次）
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(errors="replace")

API = "https://api.github.com"


def api(method, path, token, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(API + path, method=method, data=data, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "User-Agent": "smo-set-ci-secret",
    })
    with urllib.request.urlopen(req, timeout=40) as f:
        body = f.read()
    return json.loads(body) if body else {}


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    repo, name = sys.argv[1], sys.argv[2]
    token = os.environ.get("SMO_CI_TOKEN", "").strip()
    if not token:
        print("🔴 没给令牌：SMO_CI_TOKEN=<令牌> python …")
        return 2
    if "/" not in repo:
        print("🔴 仓库要写 owner/repo")
        return 2

    try:
        from nacl import encoding, public
    except ImportError:
        print("🔴 缺 pynacl：pip install pynacl")
        return 3

    key = api("GET", f"/repos/{repo}/actions/secrets/public-key", token)
    box = public.SealedBox(public.PublicKey(key["key"].encode(), encoding.Base64Encoder()))
    sealed = box.encrypt(token.encode())          # 值不进任何文件、不打印
    api("PUT", f"/repos/{repo}/actions/secrets/{name}", token, {
        "encrypted_value": base64.b64encode(sealed).decode(),
        "key_id": key["key_id"],
    })
    print(f"✅ 已写入 {repo} 的 secret：{name}（key_id {key['key_id']}）")
    print("   网页核对：Settings → Secrets and variables → Actions（值不可见，只看有没有）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
