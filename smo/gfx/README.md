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

## 2.5 接口层的真实机制（第十四轮实测，替代先前「按名字索取」的粗略理解）

先把一条先前的错误结论更正掉：我曾说「469 个名字里只有 2 个被代码装载」——那是**工具 bug**，
adrp 的掩码写成 `0x9F00001F`（把 Rd 也算进固定位），于是只有 `rd=x0` 的 adrp 能匹配。
改成 `0x9F000000` 后，469 处装载全部找到（`smo/tools/rawdis.py`、`smo/gfx/tools/nvn_getproc.py` 都已修）。

实际机制（每一步都有反汇编证据）：

1. `main` 静态链入了 NVN 的 **C 过程蹦床库**，并**导出** `nvnLoadCProcs`（0x720414，12936 字节）。
2. `nvnLoadCProcs(x0=device, x1=getProc)` 是一段**展开的序列**：对 **461 个名字**各做
   `adrp/add 名字 → blr getProc → str 结果到 GOT 槽`。461 × 28 字节 ≈ 12936 字节，与符号大小吻合。
3. 游戏（AGL）调它的地方在 0x7312a8 附近：先用 `nvnBootstrapLoader` 取 **`nvnDeviceInitialize`**
   与 **`nvnDeviceGetProcAddress`**（就是 `.text` 里仅有的两处名字装载），再把后者当 `getProc` 传进去。
4. 之后游戏调用 NVN 函数的形状是 `adrp x8,0x1e59000 ; ldr x8,[x8,#off] ; ldr x8,[x8] ; blr x8`。
   实测：**461 个槽全部被代码引用**（说明名字↔槽配对正确），共 173 处直接调用点、涉及 28 个名字。
   其余调用走 AGL 自己缓存的表（真机 `UNIMPL` 日志会把它们补全）。

**这对我们的设计意味着什么（好消息）**：需要接管的入口只有两个 ——
`nvnBootstrapLoader`（唯一 import）与 `nvnDeviceGetProcAddress`（按名字返回）——
而这两个正好用同一张 469 项的名字表伺候。`nvnLoadCProcs` **不用接管**：它是客体代码，
它会来调**我们**的 GetProcAddress 461 次。也就是说，第十三轮建好的接口层形状是对的。

`slots.tsv` 是这次挖出来的表（名字 / 槽 / 解析点 / 调用点数 / 前几个调用点），
`pfnc_` 前缀的名字池（0x164dc47，451 个）就是 NVN C 过程表的字段名。

## 2.6 顺手验证的一条全局前提：x18

宿主 C 代码要在客体代码中间运行（门 → 宿主函数 → 返回客体），前提是**客体不再写苹果的平台寄存器 x18**
（门的出口保存了 x18 但不回读，见 entry.S）。用 capstone 的读写集合精确检查（区分 `x18` 与 `s18/d18`）：

```
smo/tools/check_x18.py     →  main / sdk / subsdk0 / rtld 四个改写产物：0 条指令写通用 x18  ✅
```

不变量成立，共用 x18 的设计站得住。

## 2.7 状态层（第十五轮）

`nvn_state.c/h`：客体对象指针 → 宿主侧状态的开放寻址表。**不写客体对象内存** ——
所以不依赖 NVN 结构的版本细节（我们手上只有第三方重建的 nvn.h，从别的游戏的 DWARF 重建）。
删除用「整表重排」而不是后移/墓碑：对象几百个、删除很少，简单且不容易写错。

`nvn_core.c`：38 个手写实现（Device / MemoryPool / Buffer / Sync / Sampler / Program）。
每次新增实现，要把名字加进 `smo/tools/gen_nvn_records.py` 的 `SKIP`，否则生成的记录桩会和它重复定义。

`tests/nvn_core_selftest.c`：38 条断言，不需要游戏文件、不需要 Metal（客体对象就是几个本地数组）。

**两个坑（都写进了代码注释与自检）**：

1. **float 参数不在 x1，在 s0**：统一 8×u64 签名对 `nvnSamplerBuilderSetLodBias(builder, float)` 不成立。
   这类函数必须用真正的 `float` 形参写实现（表里仍按统一函数指针存，因为调用方是客体、按自己的约定传参）。
   同理适用于 CommandBufferSetAlphaRef 等带 float 的接口。
2. **sdk 自己导出 `nvnBootstrapLoader`**（0x2f05ec，184 字节）：它的实现要去和真 NVN 驱动（nvdrv 的 ioctl）
   说话，在 iOS 上不通。装载器的解析顺序是「① 强制接管表 → ② 本模块 → ③ 别模块导出 → ④ 桩」，
   所以必须在接管清单里抢在它前面（`gen_hle_force.py` 的 `EXPLICIT`），CI 会检查这一条。
