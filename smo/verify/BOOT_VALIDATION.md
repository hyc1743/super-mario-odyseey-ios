# 出包前的真实启动检查

`build-shell-ipa.yml` 在注入私有程序后，构建 `smo_host_embedded`，运行
`guest_boot_smoke.py`，通过才允许生成 IPA。它使用与 IPA 相同的 Embedded.S/C、
宿主初始化和 ARM64 门代码，不再仅靠合成单测推断真实程序能启动。

在 Apple Silicon Mac 上可手动执行（先按现有流程注入自己的程序）：

```sh
make -C smo/host smo_host_embedded
python3 smo/verify/guest_boot_smoke.py smo/host/smo_host_embedded --romfs /path/to/private/boot-romfs
```

通过条件：观察到进入 nnMain、StartThread 成功、至少 60 次 QueuePresentTexture 请求，
持续运行 30 秒，且无红色错误日志。提前退出、崩溃、只有超时而没有里程碑，均判失败。
失败自动通过 LLDB 再运行一次，卡住时定时中断，
输出所有线程回溯、关键寄存器和三个游戏模块的装载基址，可换算实际游戏偏移。
本地结果写到 `work/boot-smoke/`。不上传游戏二进制或内存转储到公开 artifact。

范围：这是早期启动检查。CI 从固定的私有提交下载并校验约 63 MiB 的启动资源测试包，
包含 SystemData、LocalizedData、LayoutData、ShaderData、EventData 和 EffectDataBase；
不带 --romfs 手动运行时使用空目录。没有验证完整资源、iOS 签名和系统差异、
Metal 绘制或可玩性；呈现请求也不代表画面正确。需要资源的后续路径应补充真实输入
再验证，不能通过屏蔽错误或伪造接口成功来让构建变绿。

2026-10-17 首次执行记录：

- 真机日志中的 MapMemory 在 16 KiB 宿主页上保护未对齐地址并 abort。
  svc_selftest 对所有保护调用强制 16 KiB 对齐后能复现，修复后覆盖 4 种偏移、
  跨页、共享宿主页的相邻栈解除、QueryMemory 空洞和非法输入。
- 云端运行 `38931543656` 在创建客体线程后捕获下一处 SIGSEGV，阻止出包。
- 云端运行 `38931947191` 的完整回溯确认 main+0x7abc38，
  `agl::utl::PrimitiveTexture::initialize` 向零地址写入；查询纹理存储大小的占位实现
  返回 0。此前的窄范围 NVN 回放没有覆盖这里。
- 同时修正 BufferBuilderSetStorage 的 pool 相对偏移语义，实现 BufferMap，
  通过实际 proc dispatch 检查映射地址和 CPU 写入范围。纹理尺寸查询接入已有块线性布局，
  不再返回占位的零值。后续云端结果以对应构建记录为准。
- 云端运行 `38932682148` 越过上述内置纹理初始化，创建更多客体线程，
  停在 main+0x73b4c0 `sead::PrimitiveDrawMgrNvn::prepareFromBinaryImpl`；
  此时使用空 RomFS，着色器输入数据为空。随后增加私有 SystemData 启动资源输入，
  不以跳过崩溃或伪造着色器数据的方式通过检查。
- 云端运行 `38933134253` 加载 SystemData 后到达控制器逐帧更新，停在
  main+0x75ba14。检查输入接口发现 TouchScreenState<1> 复用了写入 0x290 字节的
  <16> 版本，且 GetNpadStates 的参数和返回值签名也不正确。增加边界哨兵及真实签名测试，
  同时修正 JoyAssignment 查询和 StartSixAxisSensor 对 const 参数的错误写入。
- 云端运行 `38933610358` 越过控制器更新，推进到 RootTask/GameSystem 初始化。
  最新停点为 main+0x9caae8 `al::Resource::tryGetByml`，调用方为 EffectDataBase 构造；
  为私有测试输入追加真实 EffectDataBase.szs，再验证依赖加载，不伪造资源对象。
- 云端运行 `38934019465` 越过特效数据库，停在字体初始化调用的
  main+0xa7d2ec `al::ResourceSystem::createResource`。补充真实本地化、布局和着色器资源，
  并实现原先返回 0 的 LanguageCode::Make 与语言代码比较，验证目录选择所用的 ABI。
- 对照参考项目及 SDK+0x14f56c，进一步修正 GetDesiredLanguage：返回 8 字节语言代码，
  原先返回的是枚举数字。运行 `38935138605` 达到 30 秒存活，但最后停在未实现的
  GetSharedFontLoadState；这不能算启动完成，因此加强呈现里程碑并增加卡住时抓栈。
- 同时修正计时：计数器门与 GetSystemTick 原先用纳秒乘 192/10，快了 1000 倍，
  且各用不同起点。现在共享单调时钟，按每秒 19,200,000 tick 换算；
  自检验证确定的秒数及两个读取接口的共同时间轴。
- 运行 `38936490291` 验证了加强后的完整诊断链路：进入 nnMain、线程启动、
  30 秒存活，但没有 60 次呈现请求，正确阻止 IPA 打包；LLDB 定时暂停成功，
  主线程在 nanosleep，客体回溯返回 LayoutSystem 构造的 main+0x8ba35c。
  静态导入与调用点确认 main+0x8bab38 查询 GetSharedFontLoadState，
  +0x8bab3c 比较返回值是否为 1，否则跳回睡眠重试。当前通用桩始终返回 0。
  这是明确的启动阻塞，不能把进程存活报告成启动成功。

共享字体后续实现：`hle_pl.c` 从 `SMO_FONT_DIR` 加载实际的未加密 TTF/OTF，
验证 SFNT 表范围，通过才报告已加载；返回的地址和文件大小在进程生命周期内保持有效。
`pl_selftest` 覆盖缺失文件、损坏文件、越界字体表、非法类型及缓存生命周期；
另用实际 6 个字体文件验证三个服务接口。`font/fontll` 的解码与排版保留 SDK 原实现，
避免对纯本地字体引擎的 24 个函数错误接管。

用户选择先使用开源字体验证启动。`prepare_shared_fonts.py` 下载固定提交的 Noto CJK 和
Noto Sans Symbols 2，逐文件校验 SHA256，并随 App 附带 OFL 许可证及来源清单。
这不保证 Nintendo 专用字形与原版一致。文件名依次为 `standard.ttf`、`simplified.ttf`、
`simplified-extension.ttf`、`traditional.ttf`、`korean.ttf`、`symbols.ttf`；后缀不限制 SFNT
内部使用 TrueType 还是 OpenType/CFF。宿主可自行设置 `SMO_FONT_DIR`，iOS 使用包内 SharedFonts。

同时实现 MemoryPoolGetBufferAddress 的真实存储地址；ProgramSetShaders 按阶段数复制
最多 6 个 shaderData 描述符，而非把数组当有长度的控制段。回放层不再扫描没有长度证据的
control 指针，着色器控制结构的完整翻译仍未完成。尚未验证画面或可玩性。

云端运行 `38939008710` 已实际加载 JP/SC/TC/KR/Symbols 开源字体，越过字体初始化，
随后在音频初始化主动 svcBreak（退出 42）。日志确认 InitializeAudioRendererParameter 和
IsValidAudioRendererParameter 仍被错误接到通用零返回桩。静态检查 SDK+0x173550 起的
实现后，保留 4 个参数/配置本地函数；音频服务边界继续由宿主接管。增加回归断言，
防止配置函数再被覆盖，同时保持 GetAudioRendererWorkBufferSize 的服务接管。
LLDB 重放在 svcBreak 分派前条件断点，避免正常退出导致丢失调用栈。
本地快速回归 7/7 通过；后续启动是否推进以新的云端结果为准。

运行 `38940142864` 的 LLDB 重放确认参数检查已通过，并调用更多音频配置接口。
进一步按 SDK 实现逐项保留 83 个本地音频函数（显式符号白名单），包括混音、
声音、效果、内存池及缓冲区元数据；Open/Start/RequestUpdate 等服务接口仍接管。
新增 GetAudioRendererWorkBufferSize：按 SDK 的公开/内部参数布局转换，调用 SDK 自带的
AudioRenderSystem::GetWorkBufferSize 纯计算函数，避免固定魔数或 IPC。单测验证参数验证、
字节布局、保留位清零和重复查询。此时完整音频服务及扬声器输出仍未实现。

运行 `38940780255` 确认 SDK 实际计算音频服务工作区为 503,808 字节。
运行 `38941040530` 的 LLDB 重放捕获另一个提前崩溃：4 KiB 客体线程栈上执行
svc 日志，libsystem_c 的 localeconv_l 压栈越过底部保护页。这也是宿主代码必须有
足够栈空间的直接证据。门分派改用当前 pthread 的原生栈，返回时恢复客体栈；
嵌套门保留当前宿主 SP。新增 ARM64 汇编自检，在带保护页的 4 KiB 客体栈调用门、
执行大于 16 KiB 的宿主帧及嵌套门，核对寄存器回写和栈恢复。

音频后续使用 SDK 自带 AudioRenderSystem 处理真实配置序列与状态，使用
CommandListProcessor 执行 CPU 音频命令。宿主提供会话生命周期和 5 ms 事件通知；
SDK 调用在已有 TLS 的客体音频线程执行。单测覆盖初始化失败清理、更新错误传播、
启动/停止/关闭、关闭句柄拒绝访问、预算及手动/自动模式。扬声器输出仍待接入，
这些单测不代表真实游戏或音频播放已通过，需以随后原生运行结果核实。

运行 `38941913700` 的 macOS 自检通过真实 gate_stack_selftest，启动日志确认
`AUDIO_RENDERER_OPEN slot=0 bytes=503808 manual=0` 与
`AUDIO_RENDERER_CPU_FRAME bytes=1692`。游戏已越过音频服务初始化，随后在
main+0xaa4bec SoundArchiveFile::FileHeader::GetInfoBlockSize 读取声音档案失败。
私有资源包追加 14 个 SoundData 档案元数据及启动音库文件，固定提交
`0124cc558c9026a0fa3eaf2697fc880e5e05ca4b`，ZIP 大小 81,004,275 字节，
SHA256 `ac42f9abba8b94f4462f0e0a71808ffe78efa2591482b9d0032ff3705e70b330`。
完整 SoundData、关卡与对象资源仍未全部放入云端测试包。

复查 CI 日志另发现 Linux C 单测的 make 失败被 tee 的成功退出掩盖；该步骤补上
pipefail，并修正已暴露的两个忽略返回值编译错误（CreateFile 截断失败正确返回错误；
测试目录清理失败明确终止）。后续 Linux 结果必须真实通过。

运行 `38942696378` 的 Linux 整套自检真正通过，含之前被 tee 掩盖的 SMOA CRC
错误诊断路径：其 snprintf 在偏移后的指针上错误传入整个数组长度，触发 glibc
FORTIFY 中止，已改为实际剩余容量。原生启动再次通过音频，停在世界地图数据构造；
新增缺失文件跟踪确认 `content:/ObjectData/Texture2dMap.szs` 不在测试输入中。
私有包追加该真实资源，更新到 `527b041e45f7a9d584432287e7862a4d0f1760aa`，
84,968,352 字节，SHA256 `05419cd018e530bba47a5df4ff58ebc70a7042cf8dbda30ef871bf5678ca2551`。

Run `38943030416` passed Texture2dMap loading and crashed at main+0x9caae8
`al::Resource::tryGetByml`, called by GameDataHolder+0x14b8. File tracing and
the literal at main+0x52e5f4 identify missing `ObjectData/WorldWarpHole.szs`.
To avoid one build per missing file, include all 61 existing ObjectData literal
paths found in this main (43,656,656 bytes uncompressed) in a separate private
boot-objectdata.zip: 38,688,365 bytes, commit
`3012ecc50d7ee8e4ca526f2e033a624b46ada8b2`, SHA256
`ced0215b4d8ba7cabb0b5cede52d98346aabc3e4a7b8c56f9c9766de77ffecbd`.
Dynamically constructed resource names are not covered. Preserve errno across
optional missing-file logs; the filesystem self-test passes.

Run `38943647640` passed GameDataHolder and entered first-scene initialization.
The main thread aborted at SDK+0x87924 from al::AudioSystem::update, polling a
SystemEvent never initialized by the generic AcquireAudioDeviceSwitchNotification
stub. That API now binds a real SDK SystemEvent without periodic false device
switch notifications. Device queries expose the CPU renderer's single stereo
route with the SDK's verified 0x100-byte name ABI. Audio regression covers
binding, output bounds, capacity handling and consistent endpoint queries.
Host speaker delivery remains pending; these queries do not prove audio playback.
A background loader also requested missing EffectData/EffectPtcl.szs; add that
real resource in private boot-effects.zip, 61,838,135 bytes, commit
`7ea76748437f37fe04b6263ddcea3ff80401fc47`, SHA256
`e67294786c2cff43785701e02d944f52db91fa61e36b64e025125c7a927a4126`.
No successful boot gate, rendered image or real-device fix is claimed yet.

Run `38944179187` confirms audio device notification initialization passes and
observes the first QueuePresentTexture request. Native execution then traps
with EXC_BAD_INSTRUCTION at SDK+0x86384, `mrs x9, CTR_EL0`, in FlushDataCache
called by AddAuxImpl on the sound thread. An aligned-instruction scan of all
three original text segments finds five cache operations, all in SDK functions
FlushDataCache and __clear_cache; main and subsdk0 have none. Host boundaries
now replace those functions and FlushEntireDataCache. CPU renderer memory is
coherent and uses a full ordering fence; instruction invalidation uses Darwin's
sys_icache_invalidate. Metal resource synchronization is outside this change.
Force-table consistency and the host-boundary regression pass locally; native
startup still needs to confirm this path and reach 60 presentation requests.

Run `38944680623` passes the cache boundary and reaches particle initialization.
The NVN state table fills its fixed 1024 slots; MemoryPool/Buffer creation then
loses state, BufferGetAddress returns zero and nn::vfx::Primitive writes near
zero at main+0xb513e8. Replace the fixed table with a growing chained hash index
and stable heap entries. A map mutex protects concurrent index mutation by guest
main/resource threads. Growth and deletion no longer move retained entries;
reinitialization clears all 16 state words (formerly only 12 were cleared).
Same-object use/destruction remains caller-synchronized. Regression creates
16,384 objects on four threads, checks every entry and retained pointer, deletes
half, tests type reuse and resets the expanded table. Core/interface/replay and
texture regressions pass locally. Native startup validation follows.

Run `38945340348` executes for 30 seconds without a crash or fatal log. The
expanded state index reaches 65,536 buckets / at least 24,576 live objects;
particle buffers now load without the former zero-address crash. The gate
correctly fails: only one presentation request, not 60. LLDB interrupts the main
thread in nvn_SyncWait from main+0x732378 waitForGpuDone. The wrapper at
main+0x73a1cc passes UINT64_MAX as the timeout. QueueFenceSync and
QueueSubmitCommands currently only record calls; neither reports real GPU
completion. The Metal code is an offline test backend, not a guest queue bridge.
The reference project's fence uses its last Metal submission and waits for the
actual completion value. Signaling the current fence unconditionally would hide
this missing implementation and is not a valid startup fix.

Also correct the independently verified SyncWait ABI: 0=already signaled,
1=satisfied while waiting, 2=timeout, 3=failure; zero timeout polls, and completed
fences remain signaled. The former implementation treated zero as infinite,
returned the wrong timeout value, and auto-reset fences. Atomic completion
loads/stores support cross-thread delivery. Regression verifies polling, bounded
timeout, persistent completion and a concurrent signal. Long waits now log
NVN_FENCE_WAIT, captured explicitly in startup result.json. This diagnostic does
not report a stall as a crash or bypass GPU completion. No new IPA is released.

Final verification run `38945870815` (commit `5e54236`): Linux tests pass; ARM64
macOS startup survives the full 30-second window without fatal errors. The
result explicitly reports fence_wait_observed=true and presented_60_frames=false.
LLDB confirms main-thread SyncWait with timeout UINT64_MAX, and the new
NVN_FENCE_WAIT diagnostic identifies the unsignaled fence. The packaging gate
fails as intended; no IPA was built/released. Remaining work is the real guest
NVN command submission -> Metal execution -> completion/fence bridge, followed
by window acquisition/presentation and rendering verification. No unconditional
fence signal or dummy rendered frame was added. iOS hardware behavior and host
speaker output remain unverified.

The next implementation adds guest-memory-backed NVN command streams, stable
ended handles, segment rollover through the guest's memory callback, deep
CopyCommands and ordered CallCommands. Uniform update payloads are captured at
record time. A runtime backend submits real Metal buffer clear/update/copy
commands and ordered fence markers; SyncWait polls actual completion/error
sequences. Missing commands are collected per submission and reject it before
commit. Portable command/fence tests pass locally; the Metal hardware test and
actual ARM64 startup must still run in CI. Rendering, swapchain presentation,
image correctness and device stability are not proven by this change.

The packaging gate now also requires real GPU submission/completion diagnostics
and 60 GPU-completed frame presentations. QueuePresentTexture request counts
alone cannot pass. The runtime does not emit that frame-completion marker yet;
it must be attached to real rendering/presentation in the subsequent work.
