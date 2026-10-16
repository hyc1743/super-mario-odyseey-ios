#!/usr/bin/env bash
# 回归总入口（对应参考项目 dm/verify/regress.sh）。
#
# 分两档：
#   不需要游戏文件的（CI 每次都跑）        —— 自检、生成器一致性、单测
#   需要游戏文件的（本机有产物时才跑）     —— 改写结构自检、动态等价、x18、NVN 不透明性
#
# 用法：
#   smo/verify/regress.sh              跑所有能跑的（自动跳过缺产物的）
#   smo/verify/regress.sh --quick      只跑不需要游戏文件的那一档
#
# 退出码：0 = 全过；非 0 = 失败项数。失败不提前退出 —— 一次看清所有坏掉的地方。
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 2

# 控制台编码：工具的输出里有中文与 ✅ 符号，Windows 下默认 GBK 会崩在 print 上。
# 这与 smo/env.sh 的做法一致。
export PYTHONIOENCODING=utf-8
PY="${PY:-python}"
QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

PASS=0
FAIL=0
SKIP=0
FAILED_NAMES=()

summary() {
  printf '\n\033[1m═══ 回归结果 ═══\033[0m\n'
  printf '  通过 %d · 失败 %d · 跳过 %d\n' "$PASS" "$FAIL" "$SKIP"
  for n in ${FAILED_NAMES+"${FAILED_NAMES[@]}"}; do
    printf '\033[31m  失败: %s\033[0m\n' "$n"
  done
}

run() {
  local name="$1"; shift
  printf '\n\033[1m── %s\033[0m\n' "$name"
  if "$@"; then
    PASS=$((PASS + 1))
    printf '\033[32m   PASS\033[0m %s\n' "$name"
  else
    FAIL=$((FAIL + 1))
    FAILED_NAMES+=("$name")
    printf '\033[31m   FAIL\033[0m %s\n' "$name"
  fi
}

skip() {
  SKIP=$((SKIP + 1))
  printf '\n\033[1m── %s\033[0m\n\033[33m   SKIP\033[0m %s\n' "$1" "$2"
}

# ── 一、不需要游戏文件 ────────────────────────────────────────────────────────

run "工具链自检（Yaz0 / SARC / NSO 头 / 改写器端到端）" \
    "$PY" smo/tools/selftest.py

run "NVN 不透明性检查器的自检（确认它能抓到反例）" \
    "$PY" smo/verify/nvn_opaque_selftest.py

run "三段切分检查器的自检（边界页有重定位必须拒绝出包）" \
    "$PY" smo/ios/tools/x0_layout_selftest.py

# 生成器的名字一致性：抓过两次手写错误（见 STATUS.md 2.47），值得每次跑
run "NVN 表生成器一致性" \
    "$PY" smo/tools/gen_nvn.py --check

run "NVN 记录桩生成器一致性" \
    "$PY" smo/tools/gen_nvn_records.py --check

run "HLE 强制接管表生成器一致性" \
    "$PY" smo/tools/gen_hle_force.py --check

# C 单测。需要 make + 编译器。
# 本机（Windows/MSYS）要先 `source smo/env.sh` 才有 /c/toolchain 里的 clang 与 mingw32-make；
# 没有就如实 SKIP，不要假装跑过了。
MAKE=""
for m in make mingw32-make gmake; do
  command -v "$m" >/dev/null 2>&1 && { MAKE="$m"; break; }
done
# 挑一个**真的能编**的编译器，而不是「PATH 里有这个名字」就算。
# 本机踩到过：smo/env.sh 把 LLVM 的 bin 放在 mingw64 之前，于是 `clang` 先被找到，
# 但那份 clang 没有 C 运行时头文件（#include <stdio.h> 直接 fatal error）。
# 判据只能是「拿它编一个最小程序能不能过」。
# 注意探测要写到**真实临时文件**，不能 `-o /dev/null`：mingw 的 ld 对 /dev/null
# 会报 "final link failed: file truncated"，于是一个好编译器被误判成不可用。
CC_FOUND=""
CC_PROBE_DIR="$(mktemp -d)"
trap 'rm -rf "$CC_PROBE_DIR"' EXIT
for c in clang gcc cc; do
  command -v "$c" >/dev/null 2>&1 || continue
  if printf 'int main(void){return 0;}\n' \
       | "$c" -x c - -o "$CC_PROBE_DIR/probe" 2>/dev/null; then
    CC_FOUND="$c"; break
  fi
done
if [ -n "$MAKE" ] && [ -n "$CC_FOUND" ]; then
  run "宿主与图形层 C 单测（$MAKE + $CC_FOUND）" \
      "$MAKE" -C smo/host test CC="$CC_FOUND" PY="$PY"
elif [ -z "$CC_FOUND" ]; then
  skip "宿主与图形层 C 单测" "没有能用的 clang/gcc —— 本机先 source smo/env.sh"
else
  skip "宿主与图形层 C 单测" "有 $CC_FOUND 但没有 make —— 本机先 source smo/env.sh"
fi

[ "$QUICK" = 1 ] && { summary; exit $FAIL; }

# ── 二、需要游戏文件（work/rewrite 里的改写产物）─────────────────────────────

if [ -f work/rewrite/sdk.patched.bin ]; then
  run "改写结构自检（四个模块）" \
      "$PY" smo/tools/rewrite_check.py work/rewrite

  run "全局不变量：客体不写 x18" \
      "$PY" smo/tools/check_x18.py

  run "动态等价性：sdk 的 svc 站点（Unicorn）" \
      "$PY" smo/tools/equiv_check.py sdk --kind svc

  run "动态等价性：sdk 的 cntpct 站点（Unicorn）" \
      "$PY" smo/tools/equiv_check.py sdk --kind cntpct

  # rtld 不装（宿主扮演它），但改写器照样处理它 —— 这一项验的是改写器本身，所以留着
  run "动态等价性：rtld 的 svc 站点（Unicorn；验改写器，rtld 本身不装）" \
      "$PY" smo/tools/equiv_check.py rtld --kind svc

  # 三段切分：x0 向上取整，边界页归代码段（只读+执行）。要装的三个模块边界页里必须 0 条
  # 重定位（iOS 上没有 RWX 页）；rtld 那 32 条正是它不装的原因。出包脚本也做同一检查。
  run "三段切分：要装的模块边界页里没有重定位（iOS 无 RWX）" \
      "$PY" smo/ios/tools/x0_layout.py work/rewrite sdk subsdk0 main
else
  skip "改写产物相关（结构自检 / x18 / 动态等价）" \
       "缺 work/rewrite/sdk.patched.bin —— 先跑 patch_nso.py"
fi

if [ -f work/game/main.bin ]; then
  run "NVN 对象不透明性（客体是否裸读 NVN 字段）" \
      "$PY" smo/verify/nvn_opaque_check.py
else
  skip "NVN 对象不透明性" "缺 work/game/main.bin —— 先跑 nso_info.py --dump"
fi

# 纹理布局最硬的判据：拿游戏自己的贴图核对。BNTX 里记着工具链算出的实际字节数，
# 我们的 nvn_tex.c 算一遍去比 —— 单测只能证明算法自洽，这一条证明它与真机一致。
# 抽样 150 个归档（全量 4335 个要跑十几分钟，`--all` 可以跑全）。
if [ -d work/game/RomFS ] && [ -n "$CC_FOUND" ]; then
  run "纹理块线性布局 vs 游戏真实贴图（BNTX 记录的大小）" \
      "$PY" smo/gfx/tools/tex_layout_check.py --sample 150
elif [ -z "$CC_FOUND" ]; then
  skip "纹理块线性布局 vs 真实贴图" "要编 nvn_tex.c 成动态库，本机先 source smo/env.sh"
else
  skip "纹理块线性布局 vs 真实贴图" "缺 work/game/RomFS —— 先跑 nsp_extract.py romfs"
fi

# ── 三、还没有的（等 Metal 重放）──────────────────────────────────────────────
skip "逐拍对拍（宿主命令流 vs 真机）" "等 Metal 重放"
skip "截图闸门（FLIP 图像差异）"      "等 Metal 重放"

summary
exit $FAIL
