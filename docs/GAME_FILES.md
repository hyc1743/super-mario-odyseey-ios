# 准备游戏文件

**你必须拥有这个游戏。** 本项目只让你把自己已经买的 Nintendo Switch 版《超级马里奥 奥德赛》
跑在自己的 iPhone 上。本仓库不含任何游戏文件，以后也不会含。不要向别人索要游戏文件或分享游戏文件，
不要在 issue / PR 里贴游戏文件或下载链接。

中文 · English: 见文末

## 需要哪些文件

从你自己 Switch 上 dump 的《超级马里奥 奥德赛》**本体**（base game）。本项目目前的开发与实测都基于：

| 项目 | 值 |
|---|---|
| 标题 ID | `0100000000010000`（US） |
| 版本 | v0（初版，无更新包） |
| 键代 | KeyGeneration 3 |

你需要它的两部分：

| 部分 | 内容 | 大小 |
|---|---|---|
| 程序文件（ExeFS） | `main`、`sdk`、`subsdk0`、`rtld`、`main.npdm` | 约 22 MB |
| 资源（RomFS） | `ObjectData/`、`StageData/`、`ShaderData/`、`SoundData/` … | 约 5.5 GB |

版本要对得上：`main` 的构建号是 `3ca12dfaaf9c82da`。工具会逐项核对，对不上会直接报出来，
而不是悄悄出一个跑不起来的包。

## 从你自己的主机上 dump

用你自己的 Switch、dump 你自己买的那份。常用且长期维护的开源工具是 **nxdumptool**：

<https://github.com/DarkMatterCore/nxdumptool>

dump 时选游戏的 **Program**（NCA），把它的 **ExeFS** 与 **RomFS** 两部分取出来。

## 如果你手上已经是整包（.nsp / .xci）

本项目自带解包工具，不需要额外装 hactool：

```
smo/tools/nsp_extract.py info  <你的游戏包> <你的 prod.keys>              # 先看结构与密钥来源
smo/tools/nsp_extract.py exefs <你的游戏包> <你的 prod.keys> work/game/ExeFS
smo/tools/nsp_extract.py romfs <你的游戏包> <你的 prod.keys> work/game/RomFS
```

`prod.keys` 是从你自己主机上导出的密钥文件（同 nxdumptool 的作者生态，
例如 Lockpick_RCM 之类的工具）。**游戏包与密钥只在你自己的机器上被读取，不会被上传到任何地方。**

解包工具的算法不是猜的：每一步都逐条对照 hactool 的源码实现，出处写在
[`smo/tools/nsp_extract.py`](../smo/tools/nsp_extract.py) 的文件头注释里。

## 放到哪里

工作区默认是仓库里的 `work/`：

```
work/game/ExeFS/{main,sdk,subsdk0,rtld,main.npdm}
work/game/RomFS/{ObjectData,StageData,ShaderData,…}
```

把游戏文件放到 iPhone 上时，与参考项目一致：把 `ExeFS` 与 `RomFS` 两个目录拷进 App 自己的目录
（用「文件」App > 我的 iPhone > App 名，或 Finder 里拖到 App 上）。App 启动时会检查并列出找到了什么。

## 为什么是 Switch 版而不是别的版本

iPhone 与 Switch 是同一类处理器。Switch 版的程序几乎可以一个字节不改地在 iPhone 上执行，
这是能做到流畅的前提；PC 版是给另一种处理器编的，做不到这一点。两版的游戏数据也不通用
（纹理、界面美术、音频格式都不一样）。

## Why the Switch release and not the PC release

iPhone and Switch use the same class of processor. Almost all of the Switch release's program can run
on an iPhone without changing a single byte, which is what makes smooth play possible; the PC release
is built for a different kind of processor and cannot do that. The game data is not interchangeable
either: textures, interface art and audio use different formats in the two releases.
