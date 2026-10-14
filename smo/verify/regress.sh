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

# 生成器的名字一致性：抓过两次手写错误（见 STATUS.md 2.47），值得每次跑
run "NVN 表生成器一致性" \
    "$PY" smo/tools/gen_nvn.py --check

run "NVN 记录桩生成器一致性" \
    "$PY" smo/tools/gen_nvn_records.py --check

run "HLE 强制接管表生成器一致性" \
    "$PY" smo/tools/gen_hle_force.py --check

# C 单测。需要 clang/gcc；Windows 本机没有 clang 时自动跳过。
if command -v clang >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1; then
  run "宿主与图形层 C 单测" make -C smo/host test
else
  skip "宿主与图形层 C 单测" "本机没有 clang/gcc"
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

  run "动态等价性：rtld 的 svc 站点（Unicorn）" \
      "$PY" smo/tools/equiv_check.py rtld --kind svc
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

# ── 三、还没有的（等 Metal 重放）──────────────────────────────────────────────
skip "逐拍对拍（宿主命令流 vs 真机）" "等 Metal 重放"
skip "截图闸门（FLIP 图像差异）"      "等 Metal 重放"

summary
exit $FAIL
