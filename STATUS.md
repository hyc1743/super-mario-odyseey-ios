# Super Mario Odyssey → iOS 移植 · 状态与计划

**目标**：把 Switch 版《超级马里奥 奥德赛》移植到 iPhone（iPhone 17 Pro Max / iOS 26 起），
技术路径**完全照搬参考项目 [jhaizhou-ops/d2r-mobile](https://github.com/jhaizhou-ops/d2r-mobile)**：
不是模拟器，而是「Horizon 兼容层 + AArch64 原生执行」——游戏的 aarch64 程序直接在手机上跑，
只把「不能在苹果内核上原样执行的少量指令」改写掉，再自己实现它依赖的 Switch 系统服务与图形接口。

本文件记录**做到哪一步、依据是什么、下一步做什么**。所有结论都带可复现的命令与实测输出，
不含推测。

---

## 一、参考项目的技术路径（先看清楚再动手）

参考项目 `dm/` 的结构与分工：

| 部件 | 参考项目位置 | 做什么 |
|---|---|---|
| 程序改写器 | `dm/tools/patch_nso.py` | 离线改写 NSO：`x18`/`tpidr_el0` 读写 → 线程槽；`svc`、`mrs cntpct_el0` → 宿主门 |
| 宿主 | `dm/host/` | 加载器（多模块映射、跨模块重定位、import 解析）+ Horizon 内核 svc + 系统服务 HLE |
| 图形 | `dm/gfx/` | NVN（523 个函数）→ Metal 重放；Maxwell 着色器离线翻译成 MSL |
| iOS 壳 | `dm/ios/` | App、触屏、界面 |
| 验证 | `dm/verify/` | 改写等价性（Unicorn）、逐拍对拍、截图闸门 |

关键前提（参考项目文档原话）：**「main 只 import 一个图形入口 `nvnBootstrapLoader`，宿主强制接管它，
SDK 里 NVN 驱动的指令一条都不执行」**。下面的侦察就是验证 SMO 是否也满足这条前提。

---

## 二、已完成（每一步都有实测证据）

### 2.1 环境

本机是 Windows 10（无 Mac、无 Xcode）。已就位：

| 项 | 位置 | 状态 |
|---|---|---|
| Python 3.12.10 | `%LOCALAPPDATA%\Programs\Python\Python312` | ✅ 含 pillow numpy lz4 capstone unicorn pycryptodome |
| LLVM/clang 23.1.3 | `C:/toolchain/llvm/LLVM/bin` | ✅ clang / llvm-objcopy … |
| mingw-w64 GCC 16.2.0 | `C:/toolchain/mingw64/bin` | ✅ g++（Windows 上唯一可用的完整 C/C++ 链接环境） |
| CMake 3.31.6 / Ninja 1.12.1 | `C:/toolchain/cmake/bin`、`C:/toolchain/ninja` | ✅ |
| .NET SDK 8.0.404 | `C:/toolchain/dotnet` | ✅ 编参考项目的着色器翻译器要用 |
| **SPIRV-Cross 1.4.357.0** | `C:/toolchain/spirv-cross/bin/spirv-cross.exe` | ✅ **本机源码编译**（vulkan-sdk-1.4.357.0，参考项目验证过的版本；换版本着色器翻译结果会变） |
| 参考项目源码 | `C:/tmp/d2r-mobile` | ✅ 对照用 |
| hactool 源码 | `C:/tmp/hactool` | ✅ 作为 NCA 解密算法的**权威对照**，不用于构建 |

### 2.1.1 开发闭环的硬约束（必须早说清楚）

参考项目是在 **Apple 芯片的 Mac** 上开发的：宿主与图形层直接在那台 Mac 上编、跑、看画面。
本机是 Windows x86-64，而**客体代码是 aarch64**，因此：

- **不能在 Windows 上原生执行游戏程序**（同一条路走不通的原因不是改写器，而是 CPU 架构）。
- 能用的三种验证方式，按可信度排序：
  1. **真机 iPhone + xtool**：把宿主编成 iOS App 装上去，靠日志迭代。最真实，但每轮要出包+装机（参考项目的 NO_XCODE 路线就是这么做的，已在 Windows 上端到端验证过）。
  2. **Mac**（若有）：与参考项目完全一致的最快闭环。
  3. **本机 Unicorn 解译**：`unicorn` 已装，参考项目也用它做改写等价性验证；可以拿它做「改写块行为等价」与「HLE 逻辑」的单元级验证（这是它原本的用途），但不适合整游戏跑。

所以后面的推进方式会是：**能在本机验证的（改写等价性、HLE 单元、着色器翻译、资源生成）在本机做；
整机行为只能在真机上看日志。**

### 2.2 游戏文件解包（自己写的，等价于参考项目的 hactool 那一步）

`hactool` 依赖 mbedtls，Windows 上从源码构建风险高，因此按 hactool 源码**逐条对照**写了
`smo/tools/nsp_extract.py`（算法出处写在文件头注释里，每条都指到 hactool 的具体函数）：

```
NCA 头 XTS      nca.c nca_decrypt_header()      aes_xtls_decrypt(header_key, 0xC00, sector=0, 0x200)
XTS tweak       aes.c get_tweak()               sector 按大端写进 16 字节 tweak
XTS 单元        mbedtls_aes_crypt_xts()         T0=AES_enc(tweak_key,iv)，逐块 gf128mul_x_ble
gf128mul        mbedtls/library/gf128mul.c      两个小端 u64，溢出异或 0x87
密钥区          nca.c nca_decrypt_key_area()    AES-128-ECB(key_area_key_application[ct], 0x40)
内容密钥        nca.c:462                       带 rights_id：AES-128-ECB-Dec(titlekek[ct], ticket 标题密钥)
                nca.c:534                       不带 rights_id：密钥区第 3 项
CTR             nca.c:493                       ctr[0..7]=反转(section_ctr)，ctr[8..15]=大端(sec_off>>4)
分区偏移        utils.h media_to_real()         media_start_offset * 0x200
PFS0 位置       nca.c:1000                      段起点 + 超级块 pfs0_offset
RomFS 数据层    nca.c nca_process_ivfc_section() 固定第 IVFC_MAX_LEVEL-1 = 5 层
```

实测结果（SMO US v0，标题 `0100000000010000`）：

```
Program NCA 0f26bd42cae0e4cefda4b5bbf7ae3d50  5,539,168,256 B  rights_id=01000000000100000000000000000003
  内容密钥 = AES-128-ECB-Dec(titlekek_02, tik[0x180:0x190])      ← 穷举全部 16 字节对齐偏移 + 两种取法，
                                                                  用「段内 PFS0 偏移处解得 PFS0 magic」做判据
  段0 = ExeFS（PFS0, CTR）  段1 = RomFS（IVFC, CTR, 5.5 GB）  段2 = logo（PFS0, 不加密）
```

**ExeFS（`work/game/ExeFS/`）**：

| 文件 | 大小 | 格式 | 构建号 |
|---|---|---|---|
| `main` | 15,388,240 B | NSO0 | `3ca12dfaaf9c82da` |
| `sdk` | 4,179,745 B | NSO0 | `ae34e75d02925f44` |
| `subsdk0` | 3,175,954 B | NSO0 | `798c30e126697f22` |
| `rtld` | 7,099 B | NSO0 | `a75512be30bb2a8c` |
| `main.npdm` | 1,476 B | META | — |

**RomFS（`work/game/RomFS/`）**：4636 个文件 / 5.50 GB。顶层目录：
`EffectData EventData LayoutData LocalizedData MovieData ObjectData ShaderData SoundData StageData SystemData`

### 2.3 模块侦察（决定移植可行性的关键事实）

`smo/tools/nso_info.py`（NSO 解压 + MOD0/动态段/符号表）、`smo/tools/import_graph.py`（解析图）、
`smo/tools/xref.py`（import ← PLT 桩 ← bl 调用点）。

| 事实 | 证据 |
|---|---|
| **`main` 只 import 一个图形入口 `nvnBootstrapLoader`** | `main` 的 700 个 import 里，图形相关只有它一条；调用点 3 处（`0x731288 / 0x731298 / 0xae9d5c`）。**与参考项目的核心前提一致** |
| 游戏通过该入口按名字取 NVN 函数 | `main.bin` 里有 **469** 个不同的 `nvn*` 名字字符串（参考项目是 523） |
| `glslc*`（Nintendo 的 GLSL 编译器）**零调用点** | 8 个 `glslc*` 是无人导出的死 import，`xref.py` 查不到任何 bl 调用点 ⇒ **着色器是预编译的 Maxwell 二进制**，可以离线翻译 |
| 引擎是 **agl + sead** | `main.bin` 字符串：`agl::TextureSampler / RenderDisplay / WorkHeap / lght …`、`sead::ThreadMgr / ResourceMgr / FileDeviceMgr …` |
| 着色器在 RomFS | `RomFS/ShaderData/` 27 个 `al*.szs`（Yaz0 + SARC，`al` = agl） |
| `main`/`subsdk0` **没有裸 `svc`** | 系统调用全走 `sdk` 里的 nn::os 包装；`sdk` 有 106 处 svc、29 处 `cntpct_el0` |

`main` 的 import 分布（这是宿主 HLE 的工作面）：

```
nn::audio 92  nn::os 71  nn::hid 48  nn::socket 28  nn::util 26  nn::fontll 24
nn::fs 21  nn::oe 20  nn::account 19  nn::diag 14  nn::nfp 13  nn::nifm 9
nn::time 8  nn::mem 8  nn::web 8  nn::crypto 7  nn::friends 6  nn::vi 6
nn::prepo 5  nn::image 5  nn::ssl 4  nn::pl 3  nn::lmem 3  nn::settings 2
```

模块间解析图：

| 模块 | 符号 | import | export | 无人导出（必须宿主提供） |
|---|---|---|---|---|
| `main` | 91,231 | 700 | 90,530 | 12（含 8 个 `glslc*` 死引用） |
| `subsdk0` | 11,388 | 609 | 10,776 | 4 |
| `sdk` | 19,866 | 13 | 19,850 | 5 |
| `rtld` | 22 | 19 | 0 | 16（`__rel_dyn_*` 等装载器符号） |

> 注意：**「sdk 导出了 `nn::fs::OpenFile`」不等于宿主可以不管它**。参考项目的做法是对服务层
> `nn::` 用 `HLE_FORCE` **强制接管**（因为 SDK 的客户端实现要走 IPC，而宿主里没有服务进程）。
> 所以真正的 HLE 清单是「游戏实际调用的服务层函数」，比上表大得多。

### 2.4 程序改写器（移植 + 扩展，四个模块全部通过）

`smo/tools/patch_nso.py`（由 `smo/tools/port_rewriter.py` 从参考项目生成：只搬运通用机制，
去掉其 D2R 专属的 CUTS / OBS / PARAMS / touch-run-v4 规则表），新增了 SMO 需要的跳转表形状。

**为什么需要新增**：参考项目认的是 D2R 编译器生成的形状

```
adr x18, table ; ldrb w?, [x18, idx] ; add x18, x18, w?, lsl #2 ; br x18
```

而 SMO 的 `sdk`/`subsdk0` 里是（`xB` 由 adrp+add 变成常量，表里存 **4 字节有符号偏移**）

```
ldrsw xR, [xB, xIdx, lsl #2] ; add x18, xR, xB ; br x18        共 6 处（sdk 2 / subsdk0 4）
```

`main` 恰好没有这种表（所以 `main` 一次就过）。新增的处理：改写期把整张表读出来（表在 rodata 是常量），
把 `br x18` 换成「按槽 K18 里的目标值分发」的比较链，每条目标一个直接分支垫片，
到目标前恢复借用的 `x16/x17`——**任何寄存器都不会带着被改过的值到达目标**，表本身也不动。
兜底是 `brk #0x180`（原版有 `b.hi` 越界守卫，理论不可达；真到了要响亮地崩）。

实测（`smo/tools/patch_nso.py <NSO> work/rewrite <模块名>`）：

| 模块 | 改写统计 | 补丁大小 | 宿主门 |
|---|---|---|---|
| `main` | plain 2127 · cond 33 · pcrel 6 · window 2 | 86,864 B | 0 |
| `sdk` | plain 6286 · cond 87 · pcrel 30 · **jt_smo 2** · svc 106 · cntpct 29 · tpidr 50 · window 7 | 264,380 B | 135 |
| `subsdk0` | plain 6699 · cond 174 · pcrel 23 · **jt_smo 4** | 279,484 B | 0 |
| `rtld` | plain 3 · pcrel 1 · svc 4 | 316 B | 4 |

改写机制（照搬参考项目）：**运行时绝不改客体代码**。补丁块放在映像尾部，键页（只读）告诉它用哪个线程槽；
每个改写点原址换成一条 `b 补丁块`。所以游戏代码在装载时就是「只读+可执行」的，符合 iOS 的要求。

**结构自检**（`smo/tools/rewrite_check.py`，参考项目 `verify/structck.py` 的等价物）——全部通过：

```
main     映像 0x025c5350 改写点 2166（+窗口 2）        门 0                            补丁区 0x25b0000..  OK
rtld     映像 0x0001013c 改写点 8                     门 4 {svc:4}                    补丁区 0x10000..    OK
sdk      映像 0x00a608bc 改写点 6594（+窗口 7）        门 135 {svc:106, cntpct:29}     补丁区 0xa20000..   OK
subsdk0  映像 0x007043bc 改写点 6900                    门 0                            补丁区 0x6c0000..   OK
```

检查项：每个改写点确实是 `b` 且目标落在补丁区；每个宿主门的返回地址落在补丁区；键页区全 0（由装载器运行时填）。

6 张跳转表读出结果（`work/rewrite/*.jt.tsv`）：

```
sdk      0x3ea2c  表 0x73ab0c  4 项    0x3ea30 0x3ea4c 0x3ea84 0x3eadc
sdk      0x6f1dc  表 0x741928  4 项    0x6f224 0x6f1e0 0x6f1f0 0x6f238
subsdk0  0x1a77fc 表 0x4a8f30  25 项   …
subsdk0  0x1a8884 表 0x4a9388  27 项   …
subsdk0  0x1aae0c 表 0x4a9bd8  25 项   …
subsdk0  0x1abea8 表 0x4aa030  27 项   …
```

（共 6 处，与逐条反汇编找到的 `br x18` 处数一致；目标全部落在 .text 内。）

---

## 三、下一步（按依赖顺序）

| # | 任务 | 说明 | 依赖 |
|---|---|---|---|
| 1 | **着色器翻译链** | `ShaderData/*.szs` = Yaz0→SARC→agl 着色器程序（Maxwell SASS）。复用参考项目的 `gfx/tools/dmsh_*`（Ryujinx 解码器 → SPIR-V → spirv-cross → MSL）。需要先写 SARC 解包与 agl 程序格式解析 | LLVM/spirv-cross/.NET 安装 |
| 2 | **宿主加载器** | 移植 `host/loader.c` + `entry.S` + `thread.c`：多模块映射、线程槽、键页、跨模块重定位、按名解析 import | 改写器产物 |
| 3 | **svc + SDK 层 HLE** | `sdk` 有 106 处 svc（线程/内存/同步/时钟）+ 29 处 `cntpct`。nn::os 本身可以原样跑，只要内核层被替换 | #2 |
| 4 | **服务层 HLE（工作量最大）** | 按 `main` 实际调用的服务函数逐族实现：fs / hid / oe / account / vi / **audio(Audren 92 项)** / nifm / nfp / settings / socket / ssl / crypto / diag / prepo / web / image / pl / lmem / fontll | #3 |
| 5 | **NVN → Metal** | 469 个名字的宿主函数表 + 命令记录/重放（参考项目 `gfx/nvn/` 的 `hle_nvn.c` + `nvn_mtl.m`，203 个 NVN 函数已实现，可大量复用） | #1 #2 |
| 6 | **触屏/手柄输入** | 参考项目的触屏方案是重建的；SMO 是 3D 平台跳跃，摇杆+按键映射要重新设计 | #4 |
| 7 | **iOS App + IPA** | 需要 WSL2 + Swift 工具链 + **Xcode.xip（你的 Apple ID）** 供 xtool 生成 iOS SDK | 全部 |

**已知的外部依赖（需要你出面）**：WSL2 安装（管理员+重启）、Xcode.xip（8–10 GB，Apple ID 登录）、
Apple 账号类型（免费个人号签不了内存上限/扩展虚拟寻址两个 entitlement）。

---

## 四、本仓目录

```
Super Mario Odyssey [0100000000010000][US][v0].nsp   你提供的游戏包
ProdKeys.NET-v22.5.0/{prod.keys,title.keys}          你提供的密钥
smo/tools/nsp_extract.py     NSP/NCA 解密与提取（等价于 hactool）
smo/tools/nso_info.py        NSO 解压 + 动态段/符号表/import/export
smo/tools/import_graph.py    模块解析图（谁导出、谁必须宿主 HLE）
smo/tools/xref.py            import ← PLT 桩 ← bl 调用点
smo/tools/patch_nso.py       SMO 版 NSO 改写器（通用机制 + SMO 跳转表形状）
smo/tools/port_rewriter.py   从参考项目生成 patch_nso.py 的装配脚本
work/game/ExeFS/             解出的 4 个 NSO + npdm
work/game/RomFS/             解出的 5.2 GB 游戏资源
work/game/*.bin              摊平的模块映像（分析用）
work/rewrite/                改写产物（*.patched.bin / *.sites.tsv / *.gates.tsv …）
work/logs/                   运行日志
```

## 五、复现命令

```bash
PY=%LOCALAPPDATA%/Programs/Python/Python312/python.exe
NSP="Super Mario Odyssey [0100000000010000][US][v0].nsp"
K=ProdKeys.NET-v22.5.0/prod.keys

$PY smo/tools/nsp_extract.py info  "$NSP" "$K"                    # 看 NCA 结构与密钥来源
$PY smo/tools/nsp_extract.py exefs "$NSP" "$K" work/game/ExeFS    # 解出 4 个 NSO
$PY smo/tools/nsp_extract.py romfs "$NSP" "$K" work/game/RomFS    # 解出 5.2 GB 资源
$PY smo/tools/nso_info.py work/game/ExeFS/main --dump work/game/main.bin --group
$PY smo/tools/import_graph.py work/game/ExeFS work/game/import_graph.tsv
$PY smo/tools/xref.py main --who nvnBootstrapLoader glslc
$PY smo/tools/patch_nso.py work/game/ExeFS/main work/rewrite main
```

---

# 第二轮：着色器链起步 + 云端出包 + 仓库骨架

## 2.5 着色器解包（Yaz0 → SARC → sharc）

`smo/tools/nintendo_archives.py` 实现了 Nintendo 归档格式的读写。这三样**离线与宿主都要用**
（宿主运行时要解 romfs 里的 `.szs`，所以这份实现之后要照抄成 C）。

实测：`RomFS/ShaderData/` 27 个 `.szs` **全部解开**，每个里面是 `*.sharc` / `*.sharcb` / `*.bfsha`
—— 即 **agl 的 ShaderArchive**（`sharc` = shader archive），着色器程序的容器。

| 例子 | 压缩 → 解压 | 内含 |
|---|---|---|
| `alReducedShader.szs` | 4,993 → 49,152 | `alReducedShader.sharc` |
| `alDeferredShading.szs` | 944,193 → 2,859,008 | `alDeferredShading.sharc` |
| `alRenderMaterial.szs` | 22,466,853 → 81,172,560 | `alRenderMaterialUber.bfsha` |
| 合计 27 个 | 29.6 MB → 约 128 MB | — |

**两次踩坑都留下了证据（都在文件注释里）：**

1. **Yaz0 的位域语义**：按「标准文字描述」手推的实现自相矛盾 —— 第一个 code 字节 `0xFF` 解出的
   8 个字面量恰好是 SARC 头 `SARC\x14\x00\xff\xfe`，紧接着的回引用却越界。于是拿公开实现
   `Cuyler36/Yaz0Decoder` 对照，改正为：**code 字节只提供标志位；长度高 4 位与距离的高 4 位
   都在回引用的第一个字节里**（`b1>>4` 是长度减 2，`b1&0xF` 与 `b2` 组成距离-1）。
2. **SARC 头字段偏移**：`file_size`/`data_offset` 我原先从 `0x0C` 起读，实际应从 `0x08` 起
   （`0x04` 头长 / `0x06` BOM / `0x08` file_size / `0x0C` data_offset）。这个 bug 让所有归档的
   数据偏移整体错 4 字节 —— **是自检（`selftest.py`）抓出来的**，正好说明自检值得写。

## 2.6 自检（不需要游戏文件）

`smo/tools/selftest.py`，CI 每次都会跑：

| 组 | 覆盖 |
|---|---|
| 1. Yaz0 | 全字面量流、回引用流（自造压缩流，不依赖游戏文件） |
| 2. SARC | 自造两文件归档，校验名字/偏移/数据 |
| 3. NSO 头 | 自造 NSO0，校验段布局与构建号 |
| 4. **改写器端到端** | 自造合成 NSO（含 `svc` / `mrs tpidr_el0` / `x18` 读写 / **SMO 形状跳转表** + MOD0 + eh_frame_hdr），跑 `patch_nso.py`，再跑 `rewrite_check.py` 校验产物结构 |

第 4 组把「改写器没坏」变成 CI 每次都能证明的事，而且不需要游戏文件。造合成 NSO 的过程中又暴露了两个
小坑（都记在 selftest 注释里）：`adrp` 的立即数相对**指令所在页**；`sub` 的立即数形式是 `0x51xxxxxx`
而不是寄存器形式的 `0x4Bxxxxxx`。

## 2.7 云端出包（不需要 Mac）

`.github/workflows/build-shell-ipa.yml`：GitHub 的 **macOS runner 自带 Xcode**，正好补上本项目
缺的那台 Mac。两个 job：

- `selftest`（ubuntu）：`pip install -r requirements.txt` + `selftest.py`
- `shell-ipa`（macos）：`brew install xcodegen` → `smo/ios/shell_build.sh` → 产出**未签名 IPA**
  作为 artifact；打 `v*` tag 时自动挂到 Release

签名与装机交给使用者（Sideloadly / AltStore + 自己的 Apple ID），与参考项目 `docs/NO_XCODE.md` 一致。
壳自己也会做内容检查：一旦包内出现游戏内容文件名就中止出包。

**注意**：这一版出的是**空壳**（没有宿主、没有图形层），装上去只会看到一个状态页 —— 页面上会列出移植
进度并检查游戏文件在不在。这是故意这样做的：先把出包链路跑通并稳定下来，避免「装上去什么都不显示、
你以为坏了」。

## 2.8 仓库骨架

对齐参考项目的口径：MIT、双语 README、`docs/GAME_FILES.md`（游戏文件怎么准备）、
`docs/DERIVED.md`（仓库里有哪些与游戏有关的事实、没有哪些内容）、`CONTRIBUTING.md`。
`.gitignore` 主动排除游戏包、密钥、解包产物、`*.sharc` / `*.szs` / `*.patched.bin` 等，
提交前核对过：**26 个文件、全部是本项目自己的代码与文档，无任何游戏内容。**

## 2.9 下一步（按依赖顺序）

| # | 任务 | 具体做什么 |
|---|---|---|
| 1 | **解析 `sharc`（agl ShaderArchive）** | 从 `*.sharc` 里取出着色器程序与反射信息；这一步是接翻译链的入口 |
| 2 | **接上翻译链** | 参考项目 `gfx/tools/`：Ryujinx 解码器（C#，用本机 .NET 8 编）→ SPIR-V → 本机编好的 spirv-cross 1.4.357.0 → MSL |
| 3 | 宿主加载器 | 移植 `loader.c` / `entry.S` / `thread.c`：多模块映射、线程槽、键页、跨模块重定位、按名解析 import |
| 4 | svc + SDK 层 HLE | `sdk` 有 106 处 svc、29 处 cntpct；nn::os 可原样跑，只要内核层被换掉 |
| 5 | 服务层 HLE | 按 `main` 实际调用逐族实现（含 Audren 92 项） |
| 6 | NVN → Metal | 469 个名字的函数表 + 命令记录/重放 |
| 7 | 触屏 | SMO 是 3D 平台跳跃，摇杆与按键映射要重新设计 |

---

# 第三轮：着色器翻译链打通（SMO 着色器 → Maxwell → SPIR-V → MSL）

## 2.10 `.sharc` / `.sharcb` 是什么（实测）

`.szs` 解开后是两个文件（名字共享后缀，SARC 允许）：

| 文件 | magic | 内容 |
|---|---|---|
| `X.sharc` | `AAHS`（"SHAA" 反写） | **定义**：着色器名、变量名、UBO 名、`#define` 名（`SAMPLING_TYPE` / `IS_OUT_LINEAR`），以及 **GLSL 源码**（`alDefineVarying.glsl`、作者注释 "Matsuda Hirokazu (C)Nintendo"） |
| `X.sharcb` | `BAHS`（"SHAB" 反写） | **二进制**：编译好的 Maxwell 机器码 |

`.sharcb` 内部：每个着色器变体是一个槽位，以 `0x12345678` 标记开头；
`+0x30..+0x80` 是 **SPH（着色器程序头）**，`+0x80` 起是 Maxwell 指令。

## 2.11 翻译链已经在**本机**跑通

| 环节 | 状态 |
|---|---|
| 翻译器（Ryujinx 着色器库 + `dmsh_xlate`，C#） | ✅ 本机 .NET 10 编译成功（0 警告 0 错误） |
| SPIR-V → MSL | ✅ 本机编译的 spirv-cross 1.4.357.0 |
| SMO 着色器 → SPIR-V | ✅ **顶点着色器已译出**：blob0/blob5 译出 `attr=2`，正好对上 `.sharc` 里的 `aPosition` + `aTexCoord1` |

**怎么确认 SPH 在那里的（判据，不是猜）**：Ryujinx 的 `ShaderHeader`
（它解的就是真实 Switch 着色器）规定 `word0` bit[10:13] 是 stage、`+0x18` 是 imap、`+0x48/+0x4C` 是 omap。
把 `+0x30` 当 SPH 解，imap 得到 `0x0a`（X/Y 都是 Perspective 插值）、omap 得到 `0x0f`（写深度），
位置与语义全部吻合；而按 Ryujinx 排法解 word0 得到 stage=5（非法），说明 **Nintendo 的 NVN 头里
word0 的位排法与 GPU SPH 不同** —— 这一项还没定下来，是下一步第一件事。

绕开它的办法（已验证有效）：**`+0x30..+0x80` 的 imap/omap 段原样抄，只把 word0 的 stage 位按
Ryujinx 排法写**，再把 `word3` 的 OutputTopology 填成 `TriangleStrip`（不填会抛
`Invalid output topology "0"` —— 这个报错也是判据的一部分）。这样做顶点侧就能译出正确的
顶点属性数量。

## 2.12 还没通的

- **片元着色器**：能译完（不再抛异常、不再是空循环），但译出的反射里还没有贴图/颜色输出。
  可能原因（按可能性排序）：片元侧还需要 `c1` 常数段（`BRX` 间接跳转的跳转表，参考项目
  `dmsh_xlate` 的注释里写过：不给 c1，粒子着色器的控制流会错）；或者变体与 stage 的配对还不对。
- **facts（贴图类型/格式、顶点属性类型、片元输出类型、计算线程组大小）**：参考项目从 PC 版的反射
  里取，SMO 没有 PC 版 —— 这些要从 `.sharc` 的定义（`SAMPLING_TYPE` 等 `#define`、UBO 成员）
  自己推。这是这条链剩下的主要工作。

---

# 第四轮：着色器全量翻译通过（8281 → 8269，0 失败）

## 2.13 blob 布局定下来了

```
+0x00  u32 0x12345678        槽位标记
+0x04  0x2C 字节              （实测全零）
+0x30  SPH 0x50 字节          标准 Maxwell 着色器程序头
+0x80  Maxwell 机器码          到下一个标记为止
```

**SPH 原样喂给翻译器即可**（`dmsh_xlate` 要的就是「SPH + 指令」）：不需要合成 SPH，也不需要 `c1`。
对应工具：`smo/tools/sharcb.py`。

## 2.14 一次栽跟头：Ryujinx 的 ShaderStage 枚举从 0 起

我按「Vertex=0」去解 SPH 的 stage 位，得到一堆非法值，白折腾了两轮。实测映射（逐个取值试出来的）：

| SPH bit[10:13] | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| Ryujinx 解出 | Vertex | TessCtrl | TessEval | Geometry | Fragment |

原因是 Ryujinx 的枚举是 `Compute=0, Vertex=1, TessellationControl=2, TessellationEvaluation=3,
Geometry=4, Fragment=5`（`Compute` 在最前面）。所以 **Nintendo 的 SPH 本来就是标准格式**，
blob0/5 的 stage=1（顶点）、其余 stage=5（片元）。

## 2.15 全量结果

`sharcb.py` 从 21 个 `*.sharcb` 里取出 **8281 个着色器**（181 顶点 / 8097 片元 / 3 几何），
去重后 **8269 个唯一键**（12 个是内容完全相同的变体），全部经 `dmsh_xlate` 译成 SPIR-V：

```
8 片并行，每片 1036 个作业：全部「1036 成功，0 失败」
```

**语义逐条对上了**（拿 `.sharc` 里内嵌的 GLSL 源码当判据）：

| 译出的 MSL | 对应 GLSL |
|---|---|
| 顶点：`out.m_21.xy = in.m_18.xy;`（vTexCoord = aTexCoord1）<br>`gl_Position.xy = aPosition.xy * 2.0` | `alHalfResBuffer.glsl` 的 `VERTEX_SHADER_QUAD_TRIANGLE__CALC_POS_TEX` 宏 |
| 片元：`fp_t_tcb_8.sample(...)` → `out.m_26 = _36`<br>采样坐标前一串 `1/gl_FragCoord.w × (v × w)` 的透视插值 | `oColor = texture(uSrcTex, vTexCoord)`，且 SPH 的 imap X/Y = 2 = Perspective ✓ |
| 片元：`gather(` + `[[depth]]` | `GATHER_DEPTH_MAX / MIN / AVG`（`textureGather`）✓ |
| 片元：`[[color]]` + `[[depth]]` | `IS_OUT_LINEAR == 1` 时输出 `oLinearDepth` ✓ |

`.szs` 里还有 7 个 `.bfsha`（单文件，没有 `.sharc`/`.sharcb` 对）—— 还没处理，是同类归档的另一种打包。

## 2.16 还没做的两件事

1. **facts（贴图维度类型 / 格式、顶点属性类型、片元输出类型、计算线程组大小）**：
   参考项目从 **PC 版**的反射里取，SMO 没有 PC 版。现在的翻译按缺省值走（`Texture2D` +
   `R8G8B8A8Unorm`），所以**语义对、细节待定**。取法有两条，都要做：
   - 从 `.sharc` 定义里推：`SAMPLING_TYPE` 等 `#define`、UBO 成员类型、着色器名
   - 真机跑一遍，把宿主实际看到的管线状态（贴图格式、属性布局）dump 出来，回头重出着色器库
2. **metallib**：Windows 上没有 Apple 的 Metal 编译器。按参考项目的 NO_XCODE 路线：
   MSL 源码进包，首次启动在手机上编译一次（系统会缓存）。
