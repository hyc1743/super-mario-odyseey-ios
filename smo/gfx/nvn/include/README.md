# smo/gfx/nvn/include

## nvn.h —— 来源与许可证

* 文件：`nvn.h`（1756 行）
* 作者：h1k421，**MIT 许可证**（文件头保留完整许可声明）
* 来源：参考项目 d2r-mobile 收录的同一份（`dm/gfx/nvn/include/nvn.h`）
* 文件头自述：**从 Darksiders II 的 DWARF 信息重建**的 NVN 类型（对应 NVN 53.105，可能包含更高版本的成员）

## 我们怎么用它（以及不用它做什么）

* **用它**：取**枚举值**。`smo/gfx/nvn_enum.py` 把它抽成 `smo/gfx/nvn/enums.tsv` 与 `enums.h`
  （660 条：`NVNformat` 158、`NVNdeviceInfo` 99、混合/环绕/比较/阶段等），Metal 翻译要用这些数值。
* **不用它**：**不拿它的结构体布局去写客体对象内存**。它是从**另一个游戏**重建的，版本未必与 SMO 一致；
  宿主的状态全部放在宿主侧（`nvn_state.c`），客体对象被当作不透明句柄，一个字节都不写 ——
  所以布局差异影响不到我们。这条设计选择的由来见 `smo/gfx/README.md` 与 STATUS.md 2.54。

## 许可合规

这份头是 MIT 的，可以随仓库分发，**前提是保留其版权与许可声明**（文件头已保留）。
它不是任天堂 SDK 材料，也不是游戏内容。
