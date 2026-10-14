// HLE 注册层：按改编名把宿主实现接上 Switch 的 import。
// 派生自参考项目 d2r-mobile 的 dm/host/hle.h（MIT）。
//
// 两条与参考项目一致的规矩：
//   1. **强制接管（HLE_FORCE）**：服务层 nn:: 的客户端实现（SDK 里有定义）也要覆盖成宿主实现。
//      理由：SDK 的客户端要走 IPC（cmif/hipc）到真实的服务进程，而宿主里没有那些进程。
//      表由 smo/tools/gen_hle_force.py 从 smo/data/hle_imports.tsv（412 条）自动生成。
//   2. **未实现的导入接到记名桩**：调用时打印一行 `UNIMPL <名字>` 并返回 0（与参考项目一致），
//      真机日志里出现哪一行，就是下一批要补的实现。
#pragma once
#include "dm.h"

typedef struct { const char *name; void *fn; const char *why; } hle_force_ent;

// 生成的表（hle_force_gen.c）
extern const hle_force_ent g_hle_force[];
extern const unsigned g_hle_force_n;

// 未实现导入的记名桩：entry.S 里的静态桩表（iOS 不许运行时生成代码）
#define MAX_STUB 4096
extern void unimpl_stubs(void);
/* entry.S 的桩入口：x9 = 桩号；保存 x0..x8+lr 后调 unimpl_c */
void unimpl_c(unsigned idx, const uint64_t *regs);
/* 登记一个「没有实现的名字」到桩表，返回桩号（>= MAX_STUB 时返回 0） */
unsigned unimpl_register(const char *name);
void unimpl_report(void);

// 宿主实现登记（当前阶段：只有生成的桩；真实现按家族逐个补进来）
// 写法与参考项目一致：HLE(mangled, ret, cname, args...) 定义一个按名接管的函数
#define HLE(mangled, ret, cname, ...)                          \
    static ret cname(__VA_ARGS__);                             \
    __attribute__((used)) static const hle_force_ent hle_reg_##cname = {mangled, (void *)cname, "HLE"}; \
    static ret cname(__VA_ARGS__)

uint64_t dm_hle_force_lookup(const char *name);
