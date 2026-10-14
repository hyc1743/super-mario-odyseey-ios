# 参与进来

## 三条硬规矩

1. **不许有任何游戏内容进仓。** 游戏程序、美术、音频、文本、数据表、着色器本体、存档，一律不进。
   发布导出与 CI 都会做内容检查（见 [docs/DERIVED.md](docs/DERIVED.md)）。
2. **每条结论都要有可复现的证据。** 不许出现「应该能跑」这类结论。改一个工具的算法，
   就要在 PR 里写清依据（哪个格式文档、哪个参考实现的哪一行、或者哪个实测判据）。
   本仓库已经有过一次教训：Yaz0 按「标准文字描述」手推的实现自相矛盾，最后是拿公开实现
   `Cuyler36/Yaz0Decoder` 对照才定下位域语义（见 `smo/tools/nintendo_archives.py` 的注释）。
3. **改写规则必须逐条断言原字节并写清理由。** 规则表（`CUTS` / `OBS`）里每一项都要带：
   偏移、原字节、新字节、理由。改写前逐条断言，不符就中止——不许「差不多就上」。

## 提交前跑什么

不需要游戏文件就能跑的部分：

```bash
python3 -m pip install -r requirements.txt
python3 smo/tools/selftest.py          # Yaz0 / SARC / NSO 头 / 改写器端到端
```

有游戏文件时（本地，不进仓）：

```bash
PY=python3
NSP="<你的游戏包>"; K="<你的 prod.keys>"
$PY smo/tools/nsp_extract.py exefs "$NSP" "$K" work/game/ExeFS
$PY smo/tools/nso_info.py work/game/ExeFS/main --dump work/game/main.bin --group
$PY smo/tools/patch_nso.py work/game/ExeFS/main work/rewrite main
$PY smo/tools/rewrite_check.py work/rewrite        # 四个模块都要 OK
```

CI（`.github/workflows/build-shell-ipa.yml`）会在每个 PR 上跑自检与壳的出包；
改动工具或 iOS 壳时，请确认 CI 是绿的。

## 代码风格

- 工具是 Python 3.12，用标准库 + `requirements.txt` 里的依赖，不加新依赖除非有充分理由。
- 注释与理由用中文写清楚「为什么」，与参考项目一致：**读的人要能在不运行代码的情况下判断对不对**。
- 与参考项目 d2r-mobile 同名同义的部件，尽量保持同名（`patch_nso.py`、`loader.c`、`hle_*.c`…
  以及 `smo/tools/` 下的工具划分），便于两边对照。

## 许可

提交即表示同意以 MIT 许可（见 [LICENSE](LICENSE)）发布你的贡献。
