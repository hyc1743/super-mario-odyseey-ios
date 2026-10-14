# 本机工具链路径（每种会话先 source 一次）。免管理员、全部解包安装，位置固定在 C:/toolchain。
# 用法： source smo/env.sh
export SMO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export TOOLCHAIN=/c/toolchain
export PATH="$TOOLCHAIN/llvm/LLVM/bin:$TOOLCHAIN/mingw64/bin:$TOOLCHAIN/cmake/bin:$TOOLCHAIN/ninja:$TOOLCHAIN/spirv-cross/bin:$TOOLCHAIN/dotnet:$PATH"
export PY="$LOCALAPPDATA/Programs/Python/Python312/python.exe"
export PYTHONIOENCODING=utf-8
# 参考项目与控制台输出用 UTF-8，避免 MSYS 下中文乱码
export LANG=zh_CN.UTF-8
echo "toolchain: $(clang --version | head -1 | cut -c1-40) | $(g++ --version | head -1 | cut -c1-30) | cmake $(cmake --version | head -1 | awk '{print $3}') | spirv-cross $(spirv-cross --help >/dev/null 2>&1 && echo ok)"
echo "python: $("$PY" --version)"
