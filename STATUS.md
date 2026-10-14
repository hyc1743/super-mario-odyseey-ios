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

---

# 第五轮：GitHub 云端出包打通（CI 全绿，IPA 产出）

仓库：https://github.com/hyc1743/super-mario-odyseey-ios（名字里多打了一个 e，见文末）

## 2.17 两个 job

| job | runner | 做什么 | 状态 |
|---|---|---|---|
| 工具链自检（不需要游戏文件） | ubuntu | `pip install -r requirements.txt` + `selftest.py`（Yaz0 / SARC / NSO 头 / 改写器端到端） | ✅ |
| 构建 App 壳 → 未签名 IPA | macos | `brew install xcodegen` → `shell_build.sh` → 产物 `SMOMobile-shell.ipa`（28 KB）作为 artifact | ✅ |

**为什么云端 macOS runner 就够**：参考项目是在 Apple 芯片的 Mac 上用 `xcodebuild` 出包的；
GitHub 的 macOS runner 正好提供这台机器（含 Xcode）。于是「本机没有 Mac」不再是障碍。

## 2.18 打通这条链路时踩的 6 个坑（每个都真实挂过 CI）

| # | 现象 | 根因 | 修法 |
|---|---|---|---|
| 1 | `CFBundleShortVersionString` 非法 | CI 把 **40 位 git SHA** 塞进 `MARKETING_VERSION`；该键只允许「数字 + 点」 | 版本号归化：非 `v1.2.3`/`1.2.3` 形状一律回落 `0.1.0` |
| 2 | Xcode 直接拒绝 | `project.yml` 写死 `iOS 16.0`，新版 Xcode 不支持这么低的部署目标 | 从 SDK 的 `SDKSettings.plist` 读 `MinimumDeploymentTarget`，取与工程值的较大者 |
| 3 | 脚本静默中止 | **BSD `sort` 不支持 `-V`**（那是 GNU coreutils），而脚本带 `set -o pipefail` | 用 awk 比版本号 |
| 4 | 同上（隐患） | `sed … \| head -1` 在 `pipefail` 下可能被 **SIGPIPE** 判失败 | 改用 awk「打印后立即 exit」 |
| 5 | `VERSION: unbound variable`（本地怎么都复现不了） | **macOS 自带的 bash 3.2 是多字节不安全的**：`"$VERSION，来源"` 里紧跟变量的全角逗号被**吞进变量名**，于是去查 `VERSION，` 这个不存在的变量；`set -u` 当场中止。本地 bash 5.3 正确处理多字节，所以本地永远好的 | 变量**一律加花括号** `${VERSION}`；并加了扫描脚本查同类问题 |
| 6 | `Build input file cannot be found: …/Gen/Gen/Info.plist` | `xcodegen --project Gen` 会把 `project.yml` 里所有相对路径都拼到 `Gen/` 下面 | 改为在当前目录生成工程（`--project .`） |

## 2.19 一个可复用的诊断手法：check-run annotations

GitHub 的**运行日志要仓库管理员权限**才能下载（`.../actions/jobs/<id>/logs` 返回 403），
但 **check-run 的 annotations 是公开可读的**。所以 workflow 里加了：

- 构建步骤整段输出落盘（`tee smo/ios/out_step.log`）
- 失败时把日志尾部与 `error:` 行用 `::error::` 发成注解
- 同时把日志传成 artifact（管理员可直接下载）

这样在拿不到管理员权限的情况下，也能读到真实报错 —— 上面第 5、6 个坑就是这么定位的。

---

# 第六轮：facts 推导（并证明「缺省值本来就对」）

## 2.20 facts 从哪来：游戏自己的 GLSL

参考项目的 facts 来自 **PC 版**的反射；SMO 没有 PC 版，但这些事实就写在 `.sharc` 内嵌的 GLSL 里，
而且**同源**（编译成 Maxwell 的就是它）。`smo/tools/glsl_facts.py` 把它们读出来：

| facts | GLSL 出处 | 全量实测 |
|---|---|---|
| 贴图维度类型 | `uniform sampler2D/3D/Cube/…` | 2d ×172 · cube ×28 · 3d ×14 · 1d ×2 · 2darray ×2 |
| 顶点属性分量类型 | `layout(location=N) in vec4/…` | **全部 float（71 处）** |
| 片元输出分量类型 | `layout(location=N) out vec4/…` | **全部 float（41 处）** |
| 计算线程组 / 共享内存 | `layout(local_size_…)` | 1 个程序带 local_size；无 image 声明 |
| 变体轴 | `// @@ id="cSamplingType" choice="0,1,2,3"` | 85 种（cRenderType、cFogBlendType…） |

**两个实现要点（都踩过）**：
1. **必须先展开宏**：binding 写在宏里（`#define BINDING_SAMPLER_SRC_TEX layout(binding = 0)`），
   不展开一个贴图都读不出来；
2. **`#if`/`#endif` 必须按嵌套配对**：按「到第一个 #endif 为止」切会把声明切掉一半
   （顶点属性就是这么丢的）。

## 2.21 一个反直觉的结论：贴图维度不需要推

把 `alCubeMapShader`（GLSL 里是两个 `samplerCube`）**带 facts 与不带 facts 各译一遍**，
两边 MSL 里都是 7 个 `texturecube<`：

| 运行 | MSL 里的贴图类型 |
|---|---|
| 不带 facts | `texturecube<` ×7 |
| 带 facts | `texturecube<` ×7 |

原因：**Maxwell 的 TEX 指令本身编码了贴图维度**（TEX/3D/CUBE 是不同操作码），
所以翻译器从机器码就能知道。GLSL facts 在这里的作用是**交叉校验**，而不是提供信息。

真正**代码里没有、必须从管线状态拿**的只有：顶点属性的分量类型、片元输出的分量类型
（由顶点格式与 MRT 状态决定，不在着色器指令里）—— 而 SMO 的这两项**全是 float**，
恰好等于翻译器的缺省值。

**所以结论是**：之前「不带 facts」的那一遍（8269 个）对 SMO 的着色器本来就是对的；
这一轮的价值在于**证明**它是对的（而不是假设）。以后接入整数属性/MRT 的游戏版本时，
`--facts` 开关已经把这条路留好了。

## 2.22 `.bfsha` 是另一种容器（还没处理）

7 个 `.bfsha` 文件（magic `FSHA`）**不是 SARC**，里面也没有 GLSL（`@file`、`AGL_*_SHADER` 都搜不到），
只有 uniform 名（`cTextureDisplacement0`、`cTextureCubeMapGem0`、`_stcol0`…）。
它和 `.sharc`/`.sharcb` 是两个不同的东西，需要单独解析 —— 目前全量翻译覆盖的是 21 个 `.sharcb`
（8281 → 8269 个唯一着色器），这 7 个是剩下的口子。

---

# 第七轮：着色器全覆盖（18470 个）+ 宿主 HLE 工作清单

## 2.23 `.bfsha` 补完：着色器覆盖到 28 个归档

`.bfsha`（magic `FSHA`）**不是 SARC**、也没有 GLSL，但**着色器槽位布局与 `.sharcb` 完全一样**
（`+0x00` 标记 `0x12345678` / `+0x30` SPH 0x50 / `+0x80` 机器码），所以同一套切分就能覆盖。

| | 归档数 | 着色器 | 唯一键 |
|---|---|---|---|
| 只有 `.sharcb` 时 | 21 | 8281 | 8269 |
| **加上 7 个 `.bfsha`** | **28** | **18487** | **18470** |

贡献最大的是 `alRenderMaterial.bfsha`（9811 个 —— 那个 Uber 材质着色器的全部排列组合）。
全量翻译：8 片并行，每片 2311 个作业，**全部「成功、0 失败」**，产出 468 MB SPIR-V + 反射。

## 2.24 宿主 HLE 工作清单（`smo/tools/hle_inventory.py`）

参考项目的 `hle_inventory.py` 是从**自己的源码**里抽 HLE 条目（事后审计）；
本工具做**反过来**的事：从事先的游戏文件里算出**必须实现什么**（事前规划）。输出
`smo/data/hle_imports.tsv`（1341 条，逐条给出「哪个模块、家族、谁导出、宿主要做什么」）。

**按处置方式统计（4 个模块的 1341 条 import）：**

| 条数 | 处置 |
|---|---|
| **412** | **服务客户端 —— 宿主必须接管（HLE_FORCE）** |
| 398 | C 库/运行时（模块内或宿主 libc 直通） |
| 274 | 媒体框架（模块内，原样跑） |
| 156 | 模块内（本地库） |
| 60 | C++ 标准库（模块内） |
| 19 + 19 | 网络 socket / ssl（宿主：离线桩或 BSD socket） |
| 3 | 图形 NVN（`nv::InitializeGraphics` 等，宿主接管） |

**412 条服务客户端按家族**（这就是宿主服务层的工作面）：

```
nn::audio 106   nn::hid 53   nn::socket 28   nn::fs 25   nn::fontll 24
nn::sf 23       nn::oe 20    nn::account 19  nn::nfp 13  nn::web 10
nn::nifm 9      nn::friends 9   nn::time 8   nn::image 7  nn::crypto 7
nn::vi 6        nn::lmem 6   nn::fgm 6       nn::prepo 5  nn::ssl 4 …
```

**内核层**（改写器在各模块里数出来的宿主门）：`sdk` **106 处 svc + 29 处 cntpct**，
`rtld` 4 处 svc；`main` 与 `subsdk0` 没有裸 svc（全走 sdk 的 nn::os 包装）。

## 2.25 三个意外发现

1. **SMO 的过场视频用的是 Android 媒体框架**：`subsdk0` 从 `sdk` 引入 174 个
   `android::` 符号（`android::HeapObject`、`android::IInterface`…）以及 55 个
   `NvOs*/NvRm*`（NVIDIA 驱动层）。也就是说 Nintendo 把 libstagefright/binder 一系移植进了 SDK。
   对宿主的影响：`movie::`（11 条）与 `android::`（174 条）都是**跨模块、原样跑**；
   真正要 HLE 的是它底下依赖的解码/输出服务。
2. **图形初始化走的不止 `nvnBootstrapLoader`**：还有 `nv::InitializeGraphics` /
   `nv::SetGraphicsAllocator` / `nv::SetGraphicsDevtoolsAllocator`（3 条，`main → sdk`），
   宿主同样要接管。
3. **`NvRmChannel*/NvRmStream*/NvRmSurface*`**（NVIDIA Resource Manager，驱动层）
   也被 `main` 直接引入 —— 它们通常由 NVN 内部调用（而 NVN 被宿主绕过了），
   但这条要在真机上确认，不能想当然。

## 2.26 分类器踩的坑（都写进代码注释了）

- 家族提取不能用 `startswith("_ZN2nn")`：常量成员函数是 `_ZNK2nn…`、虚表是 `_ZTVN2nn…`，
  这样会漏 **774 条**（其中就有 `nv`（NVN）与 `android`（媒体框架））。
- 不先把 **C 风格符号**（`__cxa_*`、`acos`、`__errno_location`、`__nnmusl_*`）拦下来，
  它们会因为「被别的模块导出且不在本地库白名单里」被错归成服务客户端（实测错 **275 条**）。
- `St` 是 Itanium ABI 里 `std::` 的替代码（`_ZNSt3__112basic_string…`），不是普通命名空间。

---

# 第八轮：宿主装载器（核心跑通，并在真实产物上验证）

## 2.27 装载器做了什么（对应参考项目 `loader.c`）

`smo/host/loader_core.c` 是**平台无关**的那部分，逐条对应参考项目：

| 步骤 | 对应参考项目 | 本仓文件 |
|---|---|---|
| 读改写器表头（段边界、键页、8 个槽号） | `read_layout` | `loader_core.c` |
| 装键页：把「**实际**键号 × 8」写进产物键页，再设只读 | `install_key_page` | 同上 |
| 登记宿主门（`gates.tsv` 逐行） | `register_gates` | 同上 |
| 解析 MOD0 与动态段（SYMTAB/STRTAB/INIT/INIT_ARRAY） | `parse_dynamic` | 同上 |
| 符号名哈希表 + **线性扫描逐条对照**（参考项目 DM_SYMCHECK 的做法） | `module_sym` | 同上 |
| 四类重定位：RELATIVE / ABS64 / GLOB_DAT / JUMP_SLOT | `relocate` | 同上 |
| 解析顺序：**HLE_FORCE → 本模块 → 别的模块 → 宿主 import** | `resolve_sym` + `force_lookup` | `dm_hle_force()` 钩子 |

平台相关的只有三件事（`dm_image_alloc` / `dm_code_protect` / `dm_ro_protect` / `dm_slot_key`），
声明在 `dm.h` 里 —— 于是**同一份代码既能编进 iOS App，也能在本机当自检跑**（Windows/Linux 上
用 malloc + 空保护 + 假槽号）。这是在拿不到 Mac 的情况下能对装载器做的最强验证。

## 2.28 在真实产物上的实测结果

```
  rtld     映像   0x1013c  text_end   0x17a0  patch  0x10000  符号     22  键页 OK
  sdk      映像  0xa608bc  text_end 0x3de8b8  patch 0xa20000  符号  19866  键页 OK
  subsdk0  映像  0x7043bc  text_end 0x2f45b4  patch 0x6c0000  符号  11388  键页 OK
  main     映像 0x25c5350  text_end 0xbf1838  patch 0x25b0000  符号  91231  键页 OK

  重定位：RELATIVE 174723+8564+1751+13   ABS/GLOB 14850+14262+16416+17   JUMP_SLOT 639+6264+3527+2
  宿主门合计 139（sdk 135 = 106 svc + 29 cntpct，rtld 4）
  交宿主 import：调用 38 次，不同名字 25 个
  符号查询 48127 次，哈希表与线性扫描不一致 0 次
```

**两条交叉验证（两个完全独立的工具给出同一个数）：**

| 事实 | 装载器 | 独立工具 |
|---|---|---|
| `main` 的 JUMP_SLOT = **639** | 从重定位表逐条数 | `xref.py` 从指令里数 PLT 桩 = 639 ✓ |
| `main` 交宿主 import = **12**、`subsdk0` = **4** | 装载器解析结果 | `import_graph.py` 独立算的 12 / 4 ✓ |

## 2.29 一个重要的架构点（关系到最后的工作量估计）

装载器报「交宿主 import」只有 25 个名字，**远小于**第 2.24 节列出的 412 个服务客户端。
原因：**服务层那 412 个函数是被 `sdk` 模块导出的**，走的是跨模块解析，根本到不了
「交宿主」这一步 —— 它们必须靠 `dm_hle_force()`（对应参考项目的 `HLE_FORCE`）**强制接管**。

所以宿主的工作面是**两块相加**：
1. **412 个服务函数** → 需要 `HLE_FORCE` 表逐条登记 + 实现
2. **25 个无人提供的 import**（含 NVN 的 `nv::*`、`glslc*` 等）→ 需要 `resolve_import` 兜底

## 2.30 移植时自己踩的两个坑

1. **漏了跨模块解析**：第一版 `relocate` 里未定义符号直接交宿主，于是 `subsdk0` 报「交宿主 8482 次」
   （真值 4）。补上 `guest_sym()` 后数字立刻与 `import_graph.py` 对上。
2. **合成产物的 MOD0 被覆盖**：自检用的合成模块把 `MOD0` 摆在偏移 0，前 4 字节又写成了 ELF 头，
   加载器报「找不到 MOD0」。合成器现在前 0x10 字节留给头部。

## 2.31 CI 也有了装载器覆盖

CI 里没有游戏文件，所以加了 `smo/tools/make_synthetic_artifacts.py`：按**同一个格式**造一份最小产物
（MOD0 + 动态段 + 符号表 + 四类重定位 + fixups/gates），CI 上 `gcc -Werror` 编出装载器再跑它：

```
gcc -O2 -Wall -Wextra -Werror -o /tmp/loader_selftest smo/host/loader_core.c smo/host/tests/loader_selftest.c
/tmp/loader_selftest work/synthetic sdk main
```

真实产物四个模块的强断言（门 139、交宿主 20–60 个名字）只在跑真实集合时才生效。

---

# 第九轮：内核层清单 + HLE 接管表 + 宿主骨架（Apple 侧）

## 2.32 内核层的确切清单：106 个 svc 号

从改写产物 `work/rewrite/*.gates.tsv` 里直接读出 SMO 用到的 svc 号（这是**权威**的，不是估的）：

```
sdk：106 个不同的 svc 号，覆盖 #0x01–#0x7f，每个恰好 1 个调用点
     （即 SDK 给每个裸系统调用包了一个包装函数）
rtld：4 个（#0x06 / #0x26 / #0x27 / #0x28）—— 装载器自己用的
cntpct：29 处，目标寄存器 x0 ×12、x8 ×6、x20 ×8、x21 ×2、x23 ×1
     （⇒ 门必须回读任意通用寄存器，entry.S 里 x0..x29 全回读不是保守，是必需）
```

**注意**：106 个 svc **都被改写**（改写器不看可达性），但不都被**执行**。所以宿主的做法与参考项目一致：
核心的几个（内存、线程、同步、计时、调试输出）认真实现，其余接到**会响的桩**上，
真机日志里出现哪一行就是下一批要补的。这比"照单全写 106 个"现实得多。

## 2.33 服务进程清单（游戏自己声明的）

`main.npdm`（magic `META`，类型 `Application`）里列着 SMO 请求的服务名，共 33 个：

```
acc:u0  aoc:u  appletOE  audin:u  audout:u  audren:u  banana  bcat:u  bsd:u  bsdcfg
caps:su  csrng  friend:u  fsp-srv  htc:tenv  htcs  hwopus  ldn:u  ldr:ro  mii:u  mm:u
nfc:mf:u  nfc:user  nfp:user  nifm:u  nsd:u  nvdrv  pctl  pl:u  prepo:u  sfdnsres
time:u  vi:u
```

几个值得记下来的：
- **`nvdrv`**：游戏直接要 NVIDIA 驱动服务 —— 与 §2.25 里 `NvRmChannel*/NvRmStream*` 被 main 直接引入相互印证
- **`audren:u`**：音频渲染器（对应 nn::audio 那 106 个 import）
- **`hwopus`**：Opus 解码（过场/语音）
- **`fsp-srv` / `vi:u` / `acc:u0` / `friend:u` / `nfp:user` / `bsd:u`**：文件系统、显示、账号、好友、amiibo、socket
- **`banana`**：Nintendo 内部服务名（保留观察）

入口符号：**`nnMain`@0x536f90**（`_init`@0x24 是模块初始化，与参考项目的 DT_INIT 处理一致）。

## 2.34 HLE 强制接管表：446 条（自动生成）

`smo/tools/gen_hle_force.py` 从 `smo/data/hle_imports.tsv` 生成 `smo/host/hle_force_gen.c`：

| 家族 | 条数 | | 家族 | 条数 |
|---|---|---|---|---|
| nn::audio | 212 | | nn::sf | 46 |
| nn::hid | 106 | | nn::fs | 42 |
| nn::socket | 94 | | nn::oe | 40 |
| nn::fontll | 48 | | nn::account | 38 |
| nn::ssl | 46 | | nn::nfp | 26 |

**为什么这 446 条必须"强制"接管**（即使 sdk 模块里有定义）：SDK 的客户端实现要走 IPC
（cmif/hipc）到真实的服务进程，而宿主里没有那些进程。这就是 §2.29 那个数字差的来源：
装载器报"交宿主 import"只有 25 个，而这 446 个是被 sdk 导出的、走跨模块解析的那一批。

生成器踩的两个坑（都已写进注释）：
1. 头部模板里 `LOG("…%s…")` 的 `%s` 被 Python 的 `%` 格式化吃了 ⇒ 改用字符串替换；
2. 筛选口径写错（只筛了「宿主接管」前缀）⇒ 只生成出 3 条（nv::*），漏掉了 443 条
   「模块内(服务客户端，仍需接管)」。现在按「含"服务客户端" or 宿主接管/网络前缀 or 家族是 nv」筛。

## 2.35 宿主骨架（Apple 侧）

| 文件 | 内容 | 验证状态 |
|---|---|---|
| `loader_core.c` | 装载核心（上一轮） | ✅ 真实产物上通过 |
| `hle.c` + `hle_force_gen.c` | 接管表查询 + 记名桩（`UNIMPL <名字>`） | ✅ 本机 `-Werror` 编过 |
| `entry.S` | 门公共入口（存 x0..x29/q0..q31/NZCV/FPSR/FPCR → `gate_dispatch` → 全回读，x18 不回读）+ 4096 个静态记名桩 + 客体线程起点 | ⏳ CI 编（macOS） |
| `thread.c` | 四个线程槽（K18/KTLS/KTLR/KGATE）+ **TSD 布局自证**（`tpidrro_el0 + 8×键号`，不符就中止） | ⏳ CI 编 |
| `loader_apple.c` | 映像分配与段权限。macOS 走 `mmap(MAP_JIT)` + `mprotect(RX)`；**iOS 那条（三段内嵌签名段 + `mach_vm_remap`）只留了接口与命名约定** | ⏳ CI 编 |
| `main.c` | bootstrap：建线程键 → 装载 → 跑各模块 DT_INIT → 跳 `nnMain`（或 `--entry` 指定符号、`--list` 只装载） | ⏳ CI 编 |

`Makefile` 提供 `make -C smo/host`（编宿主）、`test`（合成产物跑装载器自检）、`run`（真实产物只装载）。

**明确没说到的**（下一步）：SDK 运行时的初始化顺序、iOS 的三段内嵌出包脚本、446 条服务的真实实现、
NVN → Metal 图形层、前端。现在这些位置都是会响的桩，真机日志会告诉我们先卡在哪。

## 2.36 CI 新增三项

```
① 编宿主里平台无关的那部分（hle.c / hle_force_gen.c），-Werror
② 装载器自检（合成产物）——上一轮已加
③ macOS 上 `make -C smo/host smo_host` —— 对 entry.S / thread.c / loader_apple.c / main.c 的真实检查
④ gen_hle_force.py --check —— 生成的接管表必须与 import 清单一致（改了清单忘了重跑就红）
```

---

# 第十轮：内核内存族 svc（实现 + 单测 20 条断言）

## 2.37 内存族做了什么（对应参考项目 `hle_svc.c`）

`smo/host/hle_svc.c` 实现 Horizo n 内存族的 6 个系统调用，做法与参考项目一致：

| svc | 名字 | 宿主做法 |
|---|---|---|
| 0x01 | SetHeapSize | 在预留块里按游戏要的大小放开/收回堆区读写；返回 `x1` = 堆基址 |
| 0x02/0x03 | SetMemoryPermission / SetMemoryAttribute | 直接成功 |
| 0x04 | MapMemory | **内容拷贝**模拟地址别名（SDK 建线程时把堆上的栈搬进栈区） |
| 0x05 | UnmapMemory | 内容搬回源区 + 消登记 |
| 0x06 | QueryMemory | 按分区/登记表回报区间（基址、大小、状态、权限） |

地址空间模型：预留**一整块**（5 GB，只占地址不占内存）→ alias 512 MB / stack 256 MB /
heap 从 +1 GB 处起 4 GB。状态码沿用参考项目的实测值：堆 = 5、栈/映射 = 0xB、空闲 = 0、代码 = 3。

**两条从参考项目抄来的实测经验**（都写进注释了）：

1. **应用内存池要报大一点**：游戏会把宿主报的总量**整块**设成自己的堆，再交给自己的分配器。
   参考项目报 3 GB 时主堆只剩约 18 MB，出现「128 KB 请求返回空而原版代码不检查空指针就写」的崩；
   他们改成 3285 MB + 512 MB。**SMO 的这个数还没在真机上量过** —— 代码里先沿用同一个数，
   并用 `dm_app_pool()` 单独放一处，等真机日志再定。
2. **QueryMemory 对宿主栈要回报整段栈**，不能回报「它所在的一页」：SDK 初始化主线程记录时拿
   当时的 sp 查这里，之后 `nn::os::GetCurrentStackInfo` 会断言 sp 仍在那个区间内，不在就中止。
   参考项目踩过（原因是宿主栈顶位置随环境变量长度移动），本项目照改。

## 2.38 单测：20 条断言全过（本机 + CI，不需要游戏文件）

`smo/host/tests/svc_selftest.c`，把平台页操作换成朴素实现（calloc + 空保护 + 假栈），
于是同一份 `hle_svc.c` 能在 Windows/Linux 上跑：

| 组 | 断言 |
|---|---|
| 分区 | 堆基址非 0、正好在预留块 +1 GB 处 |
| SetHeapSize | 成功返回 0 且账本跟着走 |
| **超池重试** | 超池返回 `0x10801`，**按 2 MB 递减重试后成功**（游戏启动就是这么重试的） |
| MapMemory/Unmap | 内容往返一致、登记表进出正确、重复解除映射被拒（`0xD401`） |
| 目标非法 | 不在 stack 区 → `0xDC01` |
| QueryMemory | 堆=5、映射=0xB、空闲=0、**宿主栈回报整段**（大小 = 整段栈，不是一页） |

```
$ ./svc_selftest
== 1. 地址空间分区 ==    ✅ ✅
== 2. SetHeapSize ==     ✅ ✅
== 3. 超出应用内存池… ==  ✅ ✅ ✅
== 4. MapMemory/Unmap == ✅ ✅ ✅ ✅ ✅
== 5. 目标非法 ==        ✅
== 6. QueryMemory ==     ✅ ✅ ✅ ✅ ✅ ✅
全部通过
```

## 2.39 还没做的（线程族与后面）

- **线程族 svc**（`hle_thread.c` 的对应物）：`CreateThread`/`StartThread`/`WaitSynchronization`/
  `ArbitrateLock`/事件与 `ProcessWideKey`… —— 参考项目那份 671 行，是下一块。
  这一轮已经给它留好接口（`svc_thread()`，内存族不认识就转给它）。
- SDK 初始化顺序、iOS 三段内嵌出包、446 条服务实现、NVN → Metal。

---

# 第十一轮：内核线程族 svc（实现 + 真并发单测 15 条断言）

## 2.40 线程族做了什么（对应参考项目 `hle_thread.c`）

`smo/host/hle_thread.c`：**每个客体线程 = 一个宿主 pthread**，跑在 SDK 给的客体栈上。
SDK 的 `nn::os` 原样执行，只把它底下的内核换成宿主。

| 类 | svc |
|---|---|
| 线程 | 0x08 CreateThread · 0x09 StartThread · 0x0A ExitThread · 0x0B SleepThread · 0x0C/0x0D 优先级 · 0x0E/0x0F 核掩码 · 0x10 当前核号 · 0x25 线程号 · 0x19 CancelSynchronization |
| 锁 | 0x1A ArbitrateLock · 0x1B ArbitrateUnlock（标签字 = 持有者句柄 \| 0x40000000 有等待者） |
| 条件 | 0x1C WaitProcessWideKeyAtomic · 0x1D SignalProcessWideKey |
| 地址 | 0x34 WaitForAddress · 0x35 SignalToAddress |
| 事件 | 0x45 CreateEvent · 0x11 SignalEvent · 0x12 ClearEvent · 0x17 ResetSignal · 0x18 WaitSynchronization · 0x16 CloseHandle |

**调度顺序按公开内核行为**：优先级数值小者优先，同优先级**先登记者**优先（不是随意先到先得）。
所有等待挂在一把全局锁 + 每线程一个条件变量上 —— 先求正确，性能后量（这是参考项目的取舍，也是本项目的）。

**一条顺序上的关键点**（参考项目拿它当单测的负对照，我照抄并写明理由）：
`WaitProcessWideKeyAtomic` 必须**先把手上的锁交出去、再置条件键**。反过来会出现
「有人已经 Signal 过、而这里才登记等待」的窗口 ⇒ **唤醒丢失**。

## 2.41 单测：15 条断言全过（真并发）

`smo/host/tests/thread_selftest.c` 自己扮演客体代码，直接按 ABI 调 svc：

| 组 | 断言 |
|---|---|
| 主线程登记 | 句柄非 0 |
| 线程 | CreateThread + StartThread 真的跑起来；事件等待被唤醒并返回下标 0 |
| **争锁互斥** | 两线程各 2000 次加锁/解锁 → **临界区零重叠**、计数 4000、解锁后锁字回 0 |
| 条件变量 | 等者被 `SignalProcessWideKey` 唤醒，且**唤醒时锁已在手**（内核交回） |
| 地址仲裁 | 有线程确实在 `WaitForAddress` 上等之后，`SignalToAddress` 能把它唤醒 |
| 超时 | 无人触发时 `WaitSynchronization` 返回 `0xEA01` |
| CloseHandle | 事件两端都关才释放 |

顺手加了一个诊断口 `dm_thread_waiting(kind)`（当前各类等待的线程数）—— 真机上"卡住时谁在等什么"
就靠它；单测也用它把时序变成确定的（否则测的是竞态，不是语义）。

**自己踩的两个坑**（都写进注释了）：
1. 等待链登记写错（`if (!t->next) t->next = g_wait; g_wait = t;`）会重复挂链/丢链；
2. `thread_exit` 里重复加全局锁会死锁，而且 `pthread_exit` 前**必须放锁**，否则全局锁永远锁死。
   另外单测一开始写成了「先 yield 一会儿再 signal」——那是竞态；改成用 `dm_thread_waiting` 固定时序。

## 2.42 现在单测矩阵（全部不需要游戏文件）

```
loader_selftest   装载器：真实产物上的强断言（本机）+ 合成产物（CI）
svc_selftest      内核内存族：20 条
thread_selftest   内核线程族：15 条（真并发）
```

---

# 第十二轮：fs 服务（26 个函数，单测 30 条断言）

## 2.43 做了什么

`smo/host/hle_fs.c`：真机上这些走 `fsp-srv` 服务进程，宿主直接接宿主文件系统。

| 组 | 函数 |
|---|---|
| 挂载 | QueryMountRomCacheSize · MountRom · MountTemporaryStorage · EnsureSaveData · MountSaveData · Commit/CommitSaveData · Unmount |
| 文件 | OpenFile · ReadFile（两种签名）· WriteFile · GetFileSize · SetFileSize · FlushFile · CloseFile · GetFileOpenMode |
| 目录 | OpenDirectory · ReadDirectory · CloseDirectory · GetEntryType |
| 改动 | CreateFile · DeleteFile · CreateDirectory · DeleteDirectoryRecursively · RenameFile |

挂载名 → 宿主目录：`rom` → `$SMO_ROMFS`（只读）、`save` → `$SMO_SAVE`（事务式）、`temp` → `$SMO_TEMP`。
路径形如 `rom:/System/Version.txt`；`DirectoryEntry` 布局按 Horizon ABI 逐字段对齐（0x310 字节，
`_Static_assert` 钉住）；错误码按模块语义（`0x202` = PathNotFound、`0x402` = PathExists）。

**存档做成事务式**（抄参考项目的设计）：挂载时把已提交版整树拷成工作目录，游戏的读写都落在工作目录；
`Commit` 时把工作目录换成已提交版（Apple 上 `clonefile` + `renamex_np(RENAME_SWAP)` 原子换名；
其它平台退化为「挪开旧版再换上新版」，代码里写明这是有窗口的退化）。

## 2.44 单测 30 条断言（不需要游戏文件：自己造 romfs 与存档目录）

除了常规读写，专门钉住了几件容易错的事：
- **只读挂载上写入必须被拒**（拿写方式打开、建文件两条路径）
- 不存在的文件返回 **0x202**（不是通用失败 —— 游戏会按它分支）
- **DirectoryEntry 布局**：type 在 0x304、size 在 0x308，逐项校验
- 存档事务：**提交之前已提交目录里看不到新文件**、提交之后能看到且内容一致

## 2.45 一个把测试坑到自己的 bug：断言宏求值两次

我的 `CHK(cond, msg)` 第一版写成 `printf(...(cond)...); if (!(cond)) ...` —— 条件**求值两次**，
于是每个带副作用的断言都跑了两遍：日志成对出现、`ReadDirectory` 第二遍时游标已到底、
`cnt` 被写成 0（我一度以为是目录读取实现坏了）。四个测试文件的宏都改成单次求值，
并在注释里写明这个坑。

**另外**：这个环境的 bash heredoc 会吃掉反斜杠（`\n` 变成真换行），写含 `\n` 的代码要用 edit 工具而不是 heredoc。

---

# 第十三轮：NVN 图形接口层（469 个名字的接管骨架）

## 2.46 做了什么

| 位置 | 内容 |
|---|---|
| `smo/gfx/nvn/names.txt` | 游戏向 `nvnBootstrapLoader` 索取的 **469 个名字**（从 main.bin 抽出的字符串，事实记录） |
| `smo/gfx/nvn/hle_nvn.c` | 接管 `nvnBootstrapLoader`；命令记录（Metal 重放的接缝）；未实现入口 |
| `smo/gfx/nvn/nvn_table.c` | 【生成】名字 → 宿主实现（469 项，248 已接核心家族，221 接未实现入口） |
| `smo/gfx/nvn/nvn_record_names.inc` | 【生成】记录桩（统一签名下一条宏展开 248 个） |
| `smo/gfx/README.md` | 这一层到哪一步、下一步要什么 |

**统一签名**：所有 NVN 函数都按 `(a0..a7) → u64` 收。理由与参考项目一致：苹果 AAPCS64 与 Switch
的差异主要在 <32 位参数的扩展与可变参数，统一收 8 个寄存器参数把差异挡在接口层。

## 2.47 生成器的名字一致性检查抓到了我两次手写错误

我按记忆手写了「要实现哪些 NVN 函数」，生成器的检查报出 **21 个名字游戏根本没要**：
- 游戏**不调用任何 `*BuilderCreate`** —— 它直接在对象上 `nvnXxxInitialize`
- `BeginRecord` / `EndRecord` 的真名是 **`BeginRecording` / `EndRecording`**
- `nvnSyncIncrement/GetValue/Reset` 不存在（游戏用的是别的名字）

所以现在**不手写名字**：按家族前缀（Device/MemoryPool/Buffer/Sync/Window/Program/Texture/Sampler/
Queue/Event）从真实名字表里选，生成器带 `--check` 保证生成物与名字表一致（CI 会跑）。

## 2.48 单测（不需要游戏文件、不需要 Metal）

```
✅ 表里名字数 > 400（469）
✅ 每个名字都能按名取到函数地址
✅ 已实现数与生成时登记的一致（248）
✅ 不在表里的名字返回 0（NVN 的真实行为：取不到就是 0，游戏自己处理）
✅ 空名字返回 0
✅ 调用进命令记录；可以清空（每帧重放前清）
✅ 未实现入口累计调用次数
```

## 2.49 图形层下一步（明确要什么）

1. **NVN 的真实结构布局**（各 `NVNxxx` 对象的大小与字段偏移）：公开的 `nvn.h` 里有声明，
   参考项目收录在 `gfx/nvn/include/nvn.h`（89 KB，MIT）。**本仓还没有这份头** —— 没有它，
   记录桩就只能记录、不能正确读写对象字段。这是下一批的第一件事。
2. **Metal 重放**（参考项目 `nvn_mtl.m` 626 KB，这一层最大的工作量）。
3. **着色器取用**：18470 个着色器已离线翻译完；`nvnProgramSetShaders` 收到控制段后算键查表即可。

---

# 第十四轮：NVN 接口层的真实机制（挖清了，并更正一条错误结论）

## 2.50 先更正错误：我上一轮的「只有 2 个名字被装载」是工具 bug

adrp 的掩码我写成了 `0x9F00001F`（**把 Rd 字段也算进固定位**），于是只有 `rd=x0` 的 adrp 能匹配 ——
"3 个装载点"全是 x0 就是线索。改成 `0x9F000000` 后，469 处名字装载全部找到。

教训写在这里，因为它值得记：**指令解码的掩码写错，会静默丢掉 99% 的匹配，而结果看起来"像"一个结论**
（"游戏只按名字要了 2 个函数"听起来完全合理）。所以现在只要出现"数量少得可疑"，都要用另一种方法交叉验一遍。

顺手做出来的工具（后面全程要用）：

| 工具 | 作用 |
|---|---|
| `smo/tools/rawdis.py` | 把摊平映像包成最小 ELF → 用 **LLVM 真反汇编器**读代码（手写反汇编只适合模式匹配） |
| `smo/gfx/tools/nvn_getproc.py` | 全 `.text` 找「装名字字符串」的位置 → 名字 ↔ 解析点 |
| `smo/gfx/tools/nvn_slots.py` | 名字 ↔ GOT 槽 ↔ 调用点（完整链条） |

## 2.51 机制（每一步都有反汇编证据）

1. `main` 静态链入了 NVN 的 **C 过程蹦床库**，并**导出** `nvnLoadCProcs`（0x720414，12936 字节，见符号表）。
2. `nvnLoadCProcs(x0=device, x1=getProc)` 是**展开的序列**：对 **461 个名字**各做
   `adrp/add 名字 → blr getProc → str 结果到 GOT 槽`。461 × 28 字节 ≈ 12936，与符号大小吻合。
3. 游戏（AGL）在 0x7312a8 附近调它：先用 `nvnBootstrapLoader` 取 **`nvnDeviceInitialize`**（0x731270）
   与 **`nvnDeviceGetProcAddress`**（0x731290）—— 这就是 `.text` 里仅有的两处名字装载；
   再把后者当 `getProc` 传给 `nvnLoadCProcs`。
4. 之后调用 NVN 的形状：`adrp x8,0x1e59000 ; ldr x8,[x8,#off] ; ldr x8,[x8] ; blr x8`。
   实测 **461 个槽全部被代码引用**（名字↔槽配对正确的独立证据），共 **173 处直接调用点、28 个名字**。
   其余调用走 AGL 自己缓存的表（真机 `UNIMPL` 日志补全）。
5. `pfnc_` 前缀的名字池（0x164dc47，451 个）就是 NVN C 过程表的**字段名**。

**对设计的验证**：需要接管的入口只有 `nvnBootstrapLoader`（唯一 import）与 `nvnDeviceGetProcAddress`
（按名字返回）—— 两个都由同一张 469 项名字表伺候 ✓✓。`nvnLoadCProcs` **不用接管**（它是客体代码，
会来调**我们**的 GetProcAddress 461 次）。→ 第十三轮建好的接口层形状是对的。

产物：`smo/gfx/nvn/slots.tsv`（名字/槽/解析点/调用点数/调用点，466 行；只有 NVN 公开 API 名，无游戏内容）。

## 2.52 顺手验证一条全局前提：x18

宿主 C 代码要在客体代码中间运行，前提是**客体不再写苹果的平台寄存器 x18**（门的出口保存 x18 但不回读）。
用 capstone 的读写集合精确检查（能区分 `x18` 与 SIMD 的 `s18/d18` —— 自己抠位会误判）：

```
smo/tools/check_x18.py  →  main / sdk / subsdk0 / rtld：0 条指令写通用 x18   ✅
```

这条不变量成立，"宿主与客体共用 x18" 的设计站得住 —— 这是整个「宿主函数插在客体中间」方案的前提之一。

## 2.53 下一步

1. NVN 状态层：把 248 个记录桩按家族换成真实现（宿主侧按对象指针索引的状态表，不写客体对象内存，
   因此不依赖 NVN 结构的版本细节）。
2. 枚举值：`smo/gfx/nvn/include/nvn.h`（h1k421 的 MIT 重建版，从 Darksiders II 的 DWARF 重建）
   提供 enums，用于后面的 Metal 翻译；结构布局只作交叉参考。
3. Metal 重放层。

---

# 第十五轮：NVN 核心家族的真实现（38 个函数）

## 2.54 状态层的设计选择：宿主侧表，不写客体对象内存

`nvn_state.c` 用「客体对象指针 → 宿主侧状态」的开放寻址表。**宿主一个字节都不写客体对象**，
理由：客体对象的大小/字段布局取决于**游戏编译时的 NVN 版本**，我们手上只有第三方重建的 nvn.h
（从别的游戏的 DWARF 重建），版本未必一致 —— 往客体对象里写字段就有踩出分配范围的风险。
不写它，就完全不依赖布局。代价是要一张表 + 类型检查（同址异类由 `type` 挡住）。
删除用「整表重排」而不是后移/墓碑：对象几百个、删除很少，简单且不容易写错。

**例外**：出参指针（如 `nvnDeviceGetInteger` 的 out）必须写客体内存，宽度只能靠调用点定 ——
所以这一条现在只写 0 并打日志（写错宽度比不写更糟）。

## 2.55 本轮实现的 38 个（都是游戏真实索取的名字）

| 家族 | 函数 |
|---|---|
| Device | BuilderSetDefaults/SetFlags、Initialize/Finalize/GetInteger、**GetProcAddress** |
| MemoryPool | BuilderSetDefaults/SetDevice/SetStorage/SetFlags、Initialize/Finalize、**Map**、GetFlags、Flush/InvalidateMappedRange |
| Buffer | BuilderSetDefaults/SetDevice/SetStorage、Initialize/Finalize、GetAddress |
| Sync | Initialize/Finalize/Wait（+ 宿主侧 `nvn_sync_signal`） |
| Sampler | BuilderSetDefaults/SetDevice/SetMinMagFilter/SetWrapMode/SetLodBias/SetMaxAnisotropy/SetCompare/SetReductionFilter、Initialize/Finalize |
| Program | Initialize/Finalize/SetShaders |

几个关键判断：

- **`nvnDeviceGetProcAddress` 是整个图形层最关键的一个函数**：AGL 先用 `nvnBootstrapLoader` 取到它，
  再交给 main 里的 `nvnLoadCProcs` 去解析 461 个 NVN 函数。我们返回什么，游戏的每一次 NVN 调用就打到哪里。
- **`nvnMemoryPoolMap` 直接返回客体给的存储地址**（不需要拷贝、也不需要"映射"）：
  客体代码在本机原生执行，客体地址就是宿主地址 —— 这是"原生执行"路线相对模拟器的一处实打实的便宜
  （实测它是调用点最多的一个：122 处）。
- `Flush/InvalidateMappedRange` 是**空操作而不是"没实现"**：主机与 GPU 看到同一块内存。
- `nvnProgramSetShaders` 把控制段（Maxwell 着色器 blob）记下来并打印头 16 字节 —— Metal 重放层
  接上后按它去查那 18470 个已译好的着色器。

## 2.56 两条新学到的坑（都写进了代码注释）

1. **float 参数不在 x1 里，在 s0**：统一 8×u64 签名对 `nvnSamplerBuilderSetLodBias(builder, float)` 不成立。
   这类函数必须用真正的 `float` 形参写实现（表里仍旧按统一函数指针存，因为调用方是客体、按自己的约定传参）。
   自检里专门验了这条：`SetLodBias(1.5f)` 存进去的位模式必须是 `0x3FC00000`。
2. **sdk 自己导出 `nvnBootstrapLoader`**（0x2f05ec，184 字节）—— 它的实现要去和真 NVN 驱动
   （nvdrv 的 ioctl）说话，在 iOS 上那条路不通。装载器的解析顺序是「① 强制接管表 → ② 本模块 →
   ③ 别模块导出 → ④ 桩」，所以必须把名字放进接管清单（`gen_hle_force.py` 的 `EXPLICIT`）才能先接管。
   这一条已经写进 CI 检查（清单里没有它就红）。

## 2.57 自检（38 条断言，不需要游戏文件、不需要 Metal）

```
nvn_core_selftest：builder → Initialize → Map/GetAddress → Sync 超时/信号/自动复位
                   → 采样器 float 走 s0 → 类型检查 → ProgramSetShaders → 名字表整体  全部通过 ✅
nvn_selftest（接口层）：469 个名字都能按名取到、取不到返回 0、命令记录可用          全部通过 ✅
```

Makefile（`make -C smo/host test`）与 CI 都已接上；图形源文件也已进宿主构建
（`SRC` 增加 4 个 gfx 源 + `hle_nvn_reg.c`），macOS job 会编它。

---

# 第十六轮：着色器桥（运行时控制段 → 已译好的 18470 个着色器）

## 2.58 键的算法与索引

离线那轮（第七轮）用 `sha256(SPH+机器码) 前 16 位` 当**目录名**。运行时另外要一个能在 ~2 万条目里
不撞的 64 位键：**FNV-1a 64**，覆盖整个 blob（SPH 头 0x50 + 机器码）。宿主侧不需要加密码库。

- `smo/gfx/shader_index.py`：递归扫离线产物（`work/shaders_all/*/code_full.bin`）→ `work/shaders.idx`
  文本索引（`<fnv64>\t<stage>\t<sha256前16位>.metal`），带 `--check` 与 `--stats`。
  实测 **18470 条**：vs 2662 / ps 15805 / gs 3（与第七轮"18487 → 18470 去重"一致）。
- `smo/gfx/nvn/nvn_shader.c`：载入索引、精确查找、控制段查找。
- `smo/host/hle_nvn_reg.c`：宿主启动时载入（路径取 `SMO_SHADER_IDX`，否则工作目录 `shaders.idx`）。

## 2.59 控制段布局：先按归档槽试，再滑动扫描（并留证据）

已确认的是游戏归档里每个着色器槽的布局（`+0x00` marker `0x12345678` / `+0x30` SPH 0x50 /
`+0x80` 机器码，第七轮实测）。AGL 是把归档字节拷进控制段的，所以先按同一布局取；
取不到再按 8 字节对齐滑动扫描（控制段 ≤ 64 KiB 时）。**控制段的确切布局仍然没有真机证据**，
所以 `nvnProgramSetShaders` 会打印实际头 16 字节与大小 —— 真机日志一到就能把布局钉死，
然后把滑动扫描换成"按控制段给出的每段长度精确取"。

## 2.60 验证

```
shader_selftest（合成）：精确查找 / 长度不对查不到 / 归档槽布局 / 滑动扫描 / 太短的控制段   全部通过 ✅
shader_selftest work/shaders.idx <400 个真实 blob>：
    载入 18470 条；命中 400，未命中 0  → 「运行时算的键 == 离线算的键」被直接验证 ✅
```

最后一条是这一轮的关键：**它说明离线译出来的东西在运行时能被准确认回来**，
Metal 重放层接上后就是「SetShaders → 查键 → 取 MSL → 编 Metal 管线」这条直线。

---

# 第十七轮：iOS 三段内嵌出包（生成 + 本机可验证的校验）

## 2.61 契约（移植参考项目 dm/ios/tools/gen_embed.py，MIT）

iOS 真机不许运行时生成/改代码，所以客体映像在出包时就要拆三段编进签名段：

| 段/节 | 内容 | 权限 |
|---|---|---|
| `__TEXT,__dm<模块>x0` | 原代码 `[0, 圆整16K(TEXT_END))` | RX |
| `__DM<模块>W,__img` | ro 余部 + data + bss `[圆整16K(TEXT_END), PATCH_OFF)` | RW |
| `__TEXT,__dm<模块>x1` | 补丁区 `[PATCH_OFF, 圆整16K(文件尾))` | RX |

两条硬约束一起满足：苹果只允许**一个**执行段（ITMS-90999，所以两段代码都得进 `__TEXT`），
而 data/bss 必须可写（所以中间那段单独放可写段）。运行时装载器保留一段连续地址，
把三段按原相对位置重映射回去 → 运行时布局与原映像逐字节相同，PC 相对寻址、补丁区相对跳转、
宿主「基址 + 原偏移」的引用全都不用改。

## 2.62 本机可验证：把三段拼回原映像

`check_embed.py` 解析生成的 `.S`（`.section`/`.incbin`/`.space`），把每段字节拼出来，然后检查：

1. 每段的 sha256 与 manifest 一致（**拼回来的字节 == 原映像 + 补零**）
2. 同一模块三段首尾相接、无空洞、每段 16K 整数倍
3. 段名/节名 ≤ 16 字节（Mach-O 上限）、`PATCH_OFF`/键页 16K 对齐、键页落在 ro/data 段内

实测（真实产物）：

```
rtld 3 段 0x14000 / sdk 3 段 0xa64000 / subsdk0 3 段 0x708000 / main 3 段 0x25c8000   ✅
合成产物（CI）：sdk/main 各 3 段，同样通过 ✅
```

**检查器抓到我一次查错对象**：我最初检查 `.space` 的长度是不是页整数倍 —— 错的，
真正把节大小凑成页整数倍的是**节尾的 `.p2align 14`**。这条已写进注释。

## 2.63 接线与还差的一步

- `smo/ios/tools/gen_embed.py`：生成 `.S` + `<.S>.manifest.tsv`，打印链接参数（可写段的 `-segprot`）
- `smo/ios/shell_build.sh`：**本机有改写产物时**自动生成并校验（CI 没有游戏数据，会跳过并说明）
- CI：合成产物上跑生成 + 校验

还差的一步（明确写在这里）：把打印出来的 `-Wl,-segprot,__DM<模块>W,rw,rw` 接进 Xcode 链接，
以及 `loader_apple.c` 的 iOS 分支（`vm_allocate` 保留 + 三段 `mach_vm_remap` 回同一基址）。
两者都要在 macOS 上才算验完。

---

# 第十八轮：Metal 重放层的第一层（记账，本机可测）

## 2.64 这一层做什么

`nvn_replay.c` 把记录下来的 NVN 调用**重建**成「绘制计划」，**刻意不含任何 Metal 调用** ——
所以它能在本机测。做的是记账：

- 按调用名分类：绑定 / 设置 / 绘制 / 分派 / 清屏 / 拷贝 / 呈现 / 同步
- 每次绑定 program 都在宿主侧状态表里查对象；**查不到就报红**（说明游戏绑了没 Initialize 的对象，
  或我们的状态层漏了某个 Initialize —— 两种都必须当场暴露，不能拖到 Metal 层去猜）
- 每次绘制/分派记一条计划项：当时的 program、绘制类型与参数，以及**从控制段查出的着色器阶段**
  （`0xFFFF` = 查过索引但没命中；`0xFFFE` = 当时还没 SetShaders。两者必须分开，否则真机上看不出
   是"索引没载入"还是"顺序不对"）
- 记录与重放已接线：`nvn_record_call` 里直接喂给 `nvn_replay_call`

## 2.65 自检（13 条断言，不需要 Metal / 游戏文件）

```
replay_selftest：造出设备+程序+控制段 → 喂「绑定/设置/两次绘制/清屏/分派」
  ✅ 计划项 3 条、绘制 2 次、未解析 0
  ✅ 计划项带着当时的 program 与绘制参数
  ✅ 着色器未载入索引 → 阶段提示 0xFFFF（与 0xFFFE 区分开）
  ✅ 绑了没初始化过的 program → 计数抓 1 次 + 记下是哪个调用
```

## 2.66 下一步（Metal 那一半）

把计划项翻成 Metal：内存池 → `MTLBuffer`（注意 iOS 上 GPU 可见内存必须来自 IOKit/MTLBuffer，
客体地址不能直接给 GPU，所以这一步要拷贝或改用共享堆 —— 这是真机上最需要实测的地方）、
纹理块线性解交错、管线状态按（program, 着色器键）缓存、出图 → `CAMetalLayer`。
这部分的**逻辑**（资源映射表、管线键）仍然可以在本机用空后端测，只有真正调 Metal 的部分要 macOS。

---

# 第十九轮：收录 nvn.h 取枚举；重放层加管线键

## 2.67 nvn.h（h1k421，MIT）与枚举表

- `smo/gfx/nvn/include/nvn.h`：1756 行，从 Darksiders II 的 DWARF 重建的 NVN 类型，
  版权与许可声明原样保留；`include/README.md` 写明来源与**「只用枚举、不用它的结构体布局」**的理由
  （它是别的游戏重建的，版本未必与 SMO 一致；宿主状态全在宿主侧，客体对象是不透明句柄，不受影响）。
- `smo/gfx/nvn_enum.py` → `enums.tsv` + `enums.h`：抽出 **660 条**枚举值
  （`NVNformat` 158 / `NVNdeviceInfo` 99 / `blendAdvancedMode` 48 / `blendFunc` 20 / `logicOp` 17 /
  `drawPrimitive` 16 …），带 `--check`，CI 已接。

## 2.68 管线键（Metal 层要按它缓存管线状态）

`nvn_replay.c` 里加了「状态哈希」：绑定/设置类调用按顺序混进一个 FNV 哈希；
绘制时的**管线键 = 状态哈希 + 当前 program 的着色器键**。

为什么用混哈希而不是逐字段解析状态：字段语义要靠真机日志逐个确认，而**键只需要"状态变了键就变"**
这一个性质。混哈希现在就能用，且不会因为某个字段解析错而把两条不同状态的绘制悄悄合成一条管线
（那种 bug 在真机上表现为"偶尔画错"，极难定位）。

自检断言：两次绘制之间没改状态 → 键相同（可复用管线）；改一次深度模式再画 → 键变了。

**测试里我自己取错了索引**（拿第 2 项当"刚画的那次"，实际是之前的分派项）—— 已改成取最后一项。

---

# 第二十轮：按参考项目的致谢补齐我们的致谢，并记下三条可用线索

参考项目最近加了致谢（README 的 Acknowledgements）。照它的口径把我们**实际用到**的东西列进
`README.md` / `README.en.md`：参考项目本体（派生关系写在每个派生文件的文件头）、Ryujinx（原样收录）、
nvn.h（h1k421，只用枚举）、SPIRV-Cross、hactool（解包算法来源）、LLVM（反汇编）、Capstone、XcodeGen、.NET。

**透明起见照抄一条口径**：`port_rewriter.py` 里那批「让游戏离线可跑」的改写规则，参考项目注明来自社区的
**LFS Mod Offline Fix** 补丁，且没有可靠的原始出处记录 —— 我们沿用同一说明（原始出处同样不可考）。

## 2.69 从致谢里拿到的三条可用线索（已记进 STATUS 与 README）

1. **deko3d（devkitPro, zlib）**：参考项目的 `dm/gfx/nvn/nvn_tex.c` 的**纹理布局尺寸算法**移植自它。
   我们的纹理层还是记录桩，将来算块线性布局尺寸时照这条走（zlib 许可，可用）。
2. **bcdec（iOrange, MIT/Unlicense）**：参考项目用它**在设备不支持 BC 格式时解码 BC 贴图** ——
   苹果 GPU 不支持 BC（用 ASTC），所以 iOS 上这条是必需的，不是可选优化。
   这与我们 README 里「A16 及更早机型参考项目需要额外 ASTC 贴图包」那一档直接相关。
3. **Unicorn 做「改写前后等价性校验」**：参考项目在致谢里提到用 Unicorn 做改写的等价性检查。
   这条对我们的「每个结论都要有可复现证据」很有价值 —— 现在的改写验证是「断言原字节 + 逐条规则自检」，
   而等价性校验能进一步做到「同一输入下改写前后寄存器/内存效果一致」。**打算纳入**（见 2.70）。

## 2.70 下一步（把 Unicorn 等价性校验排进来）

用 Unicorn 跑 aarch64 的桩环境：给同一组寄存器/内存初值，分别执行「原指令序列」与「改写后的序列」，
比较末态。优先覆盖三类改写（它们的语义等价最难靠肉眼断定）：
- `svc #n` → 门调用（出参在 x0..x8，要按各 svc 的约定比）
- `mrs xN, cntpct_el0` → 计时器门（比较"是否只影响目标寄存器"）
- x18 跳转表 → 比较链分发（比较"是否到达同一目标地址"）

---

# 第二十一轮：动态等价性校验（Unicorn）—— 起了个头，标明半成品

参考项目的致谢里提到用 Unicorn 做改写的等价性检查。这条路对我们的"每个结论都要有可复现证据"很有价值：
改写器现在的自检是**静态**的（断言原字节、逐条规则的形式检查），静态检查证明不了"同一输入下效果一致"。

## 2.71 方法已定，工具半成品

`smo/tools/equiv_check.py`：用 Unicorn 把两侧都跑一遍，比较末态。

- **已经能跑**：原始一侧 —— 在站点执行原指令（`svc #n`），用**同一个效果模型**代替真正的 svc，
  快照 x0..x29（跳过 x18）与 SP，并确认续点是"原指令的下一条"。
- **还没跑通**：改写后一侧。站点是 `b <门块>`，门块在补丁区，最后通过字面量池里的地址 `blr` 到
  `dm_gate`；那个字段在**未重定位**的 patched.bin 里是 0，所以模型里要把"PC 跑到映像外"当成宿主边界。
  现在报 `UC_ERR_READ_UNMAPPED`，还没定位到是哪一条、哪个地址 —— 这是下次接着做的第一件事。
- 因此：**目前不引用这个工具的任何结论**，也**没有接进 CI / make test**（不让它假称已验证）。

比较项（已经想清楚，写进了文件头）：① 续点一致 ② x0..x29 一致（跳过 x18）③ SP 一致
（门块压栈/出栈不配平，在真机上就是返回时 SP 错位）。
