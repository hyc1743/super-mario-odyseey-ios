# smo/gfx —— 图形层（NVN → Metal）

## 这条路为什么成立（实测，见 STATUS.md 2.3）

游戏的 `main` **只 import 一个图形入口 `nvnBootstrapLoader`**（700 个 import 里图形只有这一条，
调用点 3 处）。其余全部图形函数按名字向它索取（实测 469 个名字，从 main.bin 里抽出来）。
宿主接管这一个入口，就等于接管了整个图形接口 —— SDK 里 NVN 驱动的指令一条都不会执行。

## 目录

| 位置 | 内容 | 状态 |
|---|---|---|
| `nvn/names.txt` | 游戏向 `nvnBootstrapLoader` 索取的 **469 个名字**（从 main.bin 抽出的字符串） | 事实记录 |
| `nvn/nvn_host.h` | 接口层的公共头：统一签名（8×u64 → u64）与表结构 | — |
| `nvn/hle_nvn.c` | 接管 `nvnBootstrapLoader`；命令记录；未实现入口 | ✅ 本机单测 |
| `nvn/nvn_table.c` | 【生成】名字 → 宿主实现的表（469 项） | ✅ 生成器带一致性检查 |
| `nvn/nvn_record_names.inc` | 【生成】记录桩的实现（一条宏展开 248 个） | ✅ |
| `nvn/tests/nvn_selftest.c` | 按名取函数、取不到返回 0、记录可用 | ✅ |

生成器：`smo/tools/gen_nvn.py`（表）、`smo/tools/gen_nvn_records.py`（记录桩）。

## 现在到了哪一步

- **接口层骨架完成**：469 个名字都能按名取到函数地址；不在表里的返回 0（NVN 的真实行为）。
- **248 个「核心家族」的名字**（Device / MemoryPool / Buffer / Sync / Window / Program / Texture /
  Sampler / Queue / Event）现在是**记录桩**：把调用与参数写进命令记录，首次调用打印一行。
- 其余 221 个（含全部 CommandBuffer 的 bind/draw/set 系列）接**未实现入口**，只计数并在首次调用时报一行。

## 下一步（按顺序）

1. **状态实现**：把记录桩按家族换成真实现。需要 NVN 的**真实结构布局**（各 `NVNxxx`
   对象的大小与字段偏移）—— 公开的 `nvn.h` 里有声明，参考项目把它收录在 `gfx/nvn/include/nvn.h`
   （89 KB，MIT）。本仓还没有这份头；没有它就只能记录、不能正确读写对象里的字段。
   注意：游戏**不调用任何 `*BuilderCreate`**（它直接在对象上 `nvnXxxInitialize`），
   这一点是靠生成器的名字一致性检查发现的（我手写名字时写错了 21 个）。
2. **Metal 重放**：从命令记录取出调用，翻译成 Metal（内存池 → 零拷贝 `MTLBuffer`、
   纹理 → 块线性解交错后上传、管线状态按哈希缓存、出图 → `CAMetalLayer`）。
   参考项目对应 `nvn_mtl.m`（626 KB），是这一层最大的工作量。
3. **着色器**：已经离线翻译好 18470 个（SPIR-V + MSL，见 STATUS.md 第七轮）。
   `nvnProgramSetShaders` 收到控制段后算键（sha256 前 16 位）→ 查表 → 取 metallib。
   在手机上编译 MSL 需要 Metal 编译器（Xcode / CI 的 macOS runner 上有）。
