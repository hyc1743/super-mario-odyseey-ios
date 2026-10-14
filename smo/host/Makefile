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
SRC = loader_core.c loader_apple.c thread.c hle.c hle_force_gen.c main.c entry.S

smo_host: $(SRC) dm.h hle.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

# 装载器自检只用平台无关的那部分（本机也能编，见 smo/host/tests/loader_selftest.c）
loader_selftest: loader_core.c tests/loader_selftest.c dm.h
	$(CC) $(CFLAGS) -o $@ loader_core.c tests/loader_selftest.c

test: loader_selftest
	./loader_selftest work/synthetic sdk main

run: smo_host
	./smo_host work/rewrite rtld sdk subsdk0 main --list

clean:
	rm -f smo_host loader_selftest

.PHONY: test run clean
