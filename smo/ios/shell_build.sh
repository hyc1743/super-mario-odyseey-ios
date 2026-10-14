#!/bin/bash
# 构建 App 壳并打成**未签名 IPA**。CI（GitHub 的 macOS runner）与本机都能跑。
#
# 需要一个装了 Xcode 的 macOS；xcodegen 用来生成 .xcodeproj。
#   brew install xcodegen
#
# 用法：  bash smo/ios/shell_build.sh
# 环境变量：
#   SMO_OUT      产物目录（默认 smo/ios/out）
#   SMO_VERSION  版本号，写进 Info.plist（默认取 git describe，取不到就是 0.1.0-dev）
set -euo pipefail
cd "$(dirname "$0")"

OUT=${SMO_OUT:-out}
VERSION=${SMO_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo 0.1.0-dev)}
echo "== 构建 SMOMobile 壳（版本 $VERSION）=="
sw_vers
xcodebuild -version

command -v xcodegen >/dev/null || { echo "缺 xcodegen：brew install xcodegen"; exit 1; }

python3 tools/gen_icon.py

rm -rf Gen "$OUT"
mkdir -p Gen "$OUT"
xcodegen generate --spec project.yml --project Gen

xcodebuild \
  -project Gen/SMOMobile.xcodeproj \
  -scheme SMOMobile \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath Gen/dd \
  MARKETING_VERSION="$VERSION" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
  build

APP=$(find Gen/dd/Build/Products -maxdepth 3 -name 'SMOMobile.app' -type d | head -1)
[ -n "$APP" ] || { echo "没找到 SMOMobile.app"; exit 1; }
echo "== .app: $APP =="
du -sh "$APP"
/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$APP/Info.plist" || true
lipo -info "$APP/SMOMobile" || true

# 未签名 IPA = Payload/<App>.app 打包成 zip。签名由使用者在自己的机器上做
# （Sideloadly / AltStore + 自己的 Apple ID），与参考项目 docs/NO_XCODE.md 的口径一致。
rm -rf "$OUT/Payload"
mkdir -p "$OUT/Payload"
cp -R "$APP" "$OUT/Payload/"
( cd "$OUT" && zip -qry "SMOMobile-shell.ipa" Payload )
rm -rf "$OUT/Payload"

echo "== 产物 =="
ls -la "$OUT"
echo "== 检查：壳里不许有任何游戏内容 =="
# 与参考项目 shell_build.sh 末尾同样的自我检查：壳只该有自己的代码、图标与几张小表。
if unzip -l "$OUT/SMOMobile-shell.ipa" | grep -qE '\.(nsp|nca|sharc|sharcb|bfsha|szs|tik)$|RomFS|ExeFS'; then
  echo "🔴 壳里出现了游戏内容文件名，中止"; exit 1
fi
echo "通过：壳里只有兼容层自己的东西"
