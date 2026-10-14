// NVN 宿主层公共头（接口层）。
//
// 游戏的 main 只 import 一个图形入口 `nvnBootstrapLoader`；宿主接管它，按名字返回函数地址。
// 函数一律用「8 个 uint64 参数、返回 uint64」的统一签名接收，再在实现里按 NVN 的真实签名取值——
// 理由与参考项目同理：苹果 AAPCS64 与 Switch 的差异主要在 <32 位参数的扩展与可变参数，
// 统一收 8 个寄存器参数能把这一层差异挡在接口层，实现里只取实际用到的那几个。
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NVN_LOG(...) do { fprintf(stderr, "[nvn] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define NVN_DIE(...) do { NVN_LOG("FATAL " __VA_ARGS__); abort(); } while (0)

typedef uint64_t (*nvn_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
typedef struct { const char *name; nvn_fn fn; } nvn_ent;

extern const nvn_ent g_nvn_table[];
extern const unsigned g_nvn_table_n;
extern const unsigned g_nvn_impl_n;

// 宿主实现的统一入口名字（生成器按名字拼出来）
uint64_t nvn_unimplemented(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
// nvnBootstrapLoader 本体（要被强制接管）
uint64_t nvn_BootstrapLoader(const char *name);
// 取调用统计（自检/诊断用）：未实现被调用的种数与次数
unsigned nvn_unimpl_kinds(void);
unsigned long long nvn_unimpl_calls(void);
