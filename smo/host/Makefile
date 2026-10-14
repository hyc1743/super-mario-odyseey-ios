# 宿主（macOS 开发/CI）。iOS 出包时同一份源码编进 App（映像走内嵌签名段那条路，见 loader_apple.c）。
#
# 依赖：clang（Xcode 命令行工具即可）。
# 用法：
#   make -C smo/host            编出 smo_host
#   make -C smo/host test       用合成产物跑装载器自检（不需要游戏文件）
#   make -C smo/host run        在真实产物上装载并进 nnMain（需要 work/rewrite 里有产物）
CC      ?= clang
CFLAGS  ?= -O2 -Wall -Wextra -Werror -g
LDFLAGS ?=
SRC = loader_core.c loader_apple.c thread.c hle.c hle_svc.c hle_thread.c hle_fs.c hle_oe.c hle_hid.c hle_force_gen.c hle_nvn_reg.c main.c entry.S ../gfx/nvn/hle_nvn.c ../gfx/nvn/nvn_table.c ../gfx/nvn/nvn_state.c ../gfx/nvn/nvn_core.c ../gfx/nvn/nvn_shader.c ../gfx/nvn/nvn_replay.c
INC = -I. -I../gfx/nvn

smo_host: $(SRC) dm.h hle.h
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

# 图形层单测的公共依赖。
#
# 为什么是一个变量而不是每个目标各写一份：原先三个目标各自抄了一遍文件清单，
# 加重放层时 hle_nvn.c 里多了一处 nvn_replay_call，只有 replay_selftest 的清单跟着改了，
# 另外两个目标从此链不上（undefined reference to `nvn_replay_call`）。
# CI 没发现是因为它绕过 Makefile、自己写 gcc 命令行 —— 现在 regress.sh 会跑 `make test`。
# 结论：这一组文件是相互依赖的整体，就该只写一次。
GFX_COMMON = ../gfx/nvn/hle_nvn.c ../gfx/nvn/nvn_table.c ../gfx/nvn/nvn_state.c \
             ../gfx/nvn/nvn_core.c ../gfx/nvn/nvn_shader.c ../gfx/nvn/nvn_replay.c

# NVN 图形层单测：接口层（按名取函数 + 命令记录）
nvn_iface_selftest: $(GFX_COMMON) ../gfx/nvn/tests/nvn_selftest.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

# NVN 核心家族状态层单测（设备/池/缓冲/同步/采样器/程序）
nvn_selftest: $(GFX_COMMON) ../gfx/nvn/tests/nvn_core_selftest.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

# 着色器索引单测（合成自检；真实数据用命令行给索引与 blob）
shader_selftest: $(GFX_COMMON) ../gfx/nvn/tests/shader_selftest.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

# 重放记账层单测（不需要 Metal）
replay_selftest: $(GFX_COMMON) ../gfx/nvn/tests/replay_selftest.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^

# 纹理块线性布局单测（纯算术：GOB 双射 / 解交错可逆 / 尺寸性质）。
# 不进 GFX_COMMON：nvn_tex.c 不依赖状态层，单独编更快也更能定位问题。
tex_selftest: ../gfx/nvn/nvn_tex.c ../gfx/nvn/tests/tex_selftest.c ../gfx/nvn/nvn_tex.h
	$(CC) $(CFLAGS) $(INC) -o $@ ../gfx/nvn/nvn_tex.c ../gfx/nvn/tests/tex_selftest.c

# 装载器自检只用平台无关的那部分（本机也能编，见 smo/host/tests/loader_selftest.c）
loader_selftest: loader_core.c tests/loader_selftest.c dm.h
	$(CC) $(CFLAGS) -o $@ loader_core.c tests/loader_selftest.c

# 内核内存族的单测：不需要游戏文件，也不需要 Apple 平台
svc_selftest: hle_svc.c tests/svc_selftest.c dm.h hle.h
	$(CC) $(CFLAGS) -I. -o $@ hle_svc.c tests/svc_selftest.c

# 线程族单测：真并发（争锁互斥、条件唤醒、事件、地址仲裁、超时）
thread_selftest: hle_thread.c tests/thread_selftest.c dm.h hle_thread.h
	$(CC) $(CFLAGS) -I. -o $@ hle_thread.c tests/thread_selftest.c

# fs 服务单测：自己造一棵 romfs 与一个存档目录，不需要游戏文件
fs_selftest: hle_fs.c hle.c hle_force_gen.c tests/fs_selftest.c dm.h hle.h
	$(CC) $(CFLAGS) -I. -o $@ hle_fs.c hle.c hle_force_gen.c tests/fs_selftest.c

# oe + hid 单测：环境查询与 Npad/触屏布局
oehid_selftest: hle_oe.c hle_hid.c hle.c hle_force_gen.c tests/oehid_selftest.c dm.h hle.h dm_input.h dm_oe.h
	$(CC) $(CFLAGS) -I. -o $@ hle_oe.c hle_hid.c hle.c hle_force_gen.c tests/oehid_selftest.c

# 合成改写产物：装载器自检用。真产物是游戏内容，不能进 CI，所以自己造一份。
# 路径是仓库根下的 work/synthetic（make 在 smo/host 里跑，所以要 ../..）。
PY     ?= python3
SYNDIR  = ../../work/synthetic

$(SYNDIR)/sdk.patched.bin:
	$(PY) ../tools/make_synthetic_artifacts.py $(SYNDIR)

test: loader_selftest svc_selftest thread_selftest fs_selftest oehid_selftest nvn_iface_selftest nvn_selftest shader_selftest replay_selftest tex_selftest $(SYNDIR)/sdk.patched.bin
	./loader_selftest $(SYNDIR) sdk main
	./svc_selftest
	./thread_selftest
	./fs_selftest
	./oehid_selftest
	./nvn_iface_selftest
	./nvn_selftest
	./shader_selftest
	./replay_selftest
	./tex_selftest

# 改写前后动态等价性校验（Unicorn；需要 work/rewrite 里有真实产物，所以不进 CI）
equiv: ../gfx/nvn/nvn_state.c
	$(SMO_PY) ../../smo/tools/equiv_check.py sdk --limit 200
	$(SMO_PY) ../../smo/tools/equiv_check.py sdk --kind cntpct --limit 40
	$(SMO_PY) ../../smo/tools/equiv_check.py rtld --limit 10

run: smo_host
	./smo_host work/rewrite rtld sdk subsdk0 main --list

clean:
	rm -f smo_host loader_selftest svc_selftest thread_selftest fs_selftest oehid_selftest nvn_selftest nvn_iface_selftest shader_selftest replay_selftest tex_selftest

.PHONY: test run equiv clean
