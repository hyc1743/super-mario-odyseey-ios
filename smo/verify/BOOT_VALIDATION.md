# 出包前的真实启动检查

`build-shell-ipa.yml` 在注入私有程序后，构建 `smo_host_embedded`，运行
`guest_boot_smoke.py`，通过才允许生成 IPA。它使用与 IPA 相同的 Embedded.S/C、
宿主初始化和 ARM64 门代码，不再仅靠合成单测推断真实程序能启动。

在 Apple Silicon Mac 上可手动执行（先按现有流程注入自己的程序）：

```sh
make -C smo/host smo_host_embedded
python3 smo/verify/guest_boot_smoke.py smo/host/smo_host_embedded --romfs /path/to/private/boot-romfs
```

通过条件：观察到进入 nnMain、StartThread 成功，持续运行 30 秒，且无红色错误日志。
提前退出、崩溃、只有超时而没有里程碑，均判失败。崩溃自动通过 LLDB 再运行一次，
输出所有线程回溯、关键寄存器和三个游戏模块的装载基址，可换算实际游戏偏移。
本地结果写到 `work/boot-smoke/`。不上传游戏二进制或内存转储到公开 artifact。

范围：这是早期启动检查。CI 从私有输入分支下载并校验约 5 MB 的 SystemData 测试包；
不带 --romfs 手动运行时使用空目录。没有验证完整资源、iOS 签名和系统差异、
Metal 绘制或可玩性；30 秒存活也不能排除线程死锁。需要资源的后续路径应补充真实输入
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
