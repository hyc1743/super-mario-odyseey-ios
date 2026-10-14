#!/bin/bash
# 构建 App 壳并打成**未签名 IPA**。CI（GitHub 的 macOS runner）与本机都能跑。
#
# 需要一个装了 Xcode 的 macOS；xcodegen 用来生成 .xcodeproj。
#   brew install xcodegen
#
# 用法：  bash smo/ios/shell_build.sh
# 环境变量：
#   SMO_OUT      产物目录（默认 smo/ios/out）
#   SMO_VERSION  版本号（默认取 git describe，取不到就是 0.1.0）
#                —— 注意会做合法性归化：CFBundleShortVersionString 只允许「数字 + 点」，
#                   传 git SHA 进来（CI 在分支推送时会这么传）也自动回落到 0.1.0
set -euo pipefail
cd "$(dirname "$0")"

OUT=${SMO_OUT:-out}

# 版本号归化：v1.2.3 → 1.2.3；别的（例如 40 位 SHA）→ 0.1.0
RAW=${SMO_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo 0.1.0)}
case "$RAW" in
  v[0-9]*)            VERSION=${RAW#v} ;;
  [0-9]*.[0-9]*)      VERSION=$RAW ;;
  *)                  VERSION=0.1.0 ;;

esac
case "$VERSION" in *[!0-9.]*) VERSION=0.1.0 ;; esac      # 还带非法字符就一律回落
echo "== 构建 SMOMobile 壳（版本 $VERSION，来源 '${RAW}'）=="
sw_vers
xcodebuild -version

command -v xcodegen >/dev/null || { echo "缺 xcodegen：brew install xcodegen"; exit 1; }

# 部署目标：不能写死太低 —— 新版 Xcode 会当场拒绝（"range of supported deployment target versions"）。
# 从 SDK 自己的 SDKSettings.plist 读它允许的最低值，并取「工程里写的」与「SDK 允许的」中较大者。
SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)
SDK_VER=$(xcrun --sdk iphoneos --show-sdk-version)
SDK_MIN=$(/usr/libexec/PlistBuddy -c "Print :MinimumDeploymentTarget" "$SDK_PATH/SDKSettings.plist" 2>/dev/null || true)
[ -n "$SDK_MIN" ] || SDK_MIN=$SDK_VER
PROJ_DEP=$(sed -n 's/^ *iOS: "\([0-9.]*\)".*/\1/p' project.yml | head -1)
DEP=$(printf '%s\n%s\n' "${PROJ_DEP:-0}" "$SDK_MIN" | sort -V | tail -1)
echo "== 部署目标：工程写的 ${PROJ_DEP:-无}，SDK $SDK_VER 允许的最低 $SDK_MIN ⇒ 用 $DEP =="

python3 tools/gen_icon.py

rm -rf Gen "$OUT"
mkdir -p Gen "$OUT"
xcodegen generate --spec project.yml --project Gen

# 构建日志同时落到文件：CI 在失败时把它传成 artifact，否则只有管理员能看日志
LOG="$OUT/xcodebuild.log"
set +e
xcodebuild \
  -project Gen/SMOMobile.xcodeproj \
  -scheme SMOMobile \
  -configuration Release \
  -sdk iphoneos \
  -destination 'generic/platform=iOS' \
  -derivedDataPath Gen/dd \
  IPHONEOS_DEPLOYMENT_TARGET="$DEP" \
  MARKETING_VERSION="$VERSION" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
  build > "$LOG" 2>&1
RC=$?
set -e
tail -40 "$LOG"
[ $RC = 0 ] || { echo "🔴 xcodebuild 失败（完整日志在 $LOG）"; exit $RC; }

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
