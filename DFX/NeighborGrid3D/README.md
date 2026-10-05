# DFX/NeighborGrid3D — NeighborGrid3D 工具箱文本源

由 Test_Niagara 的 NeighborGrid3D 演示内容迁移而来。所有 `.dfs` 直接构建**原位**资产
（`Name="NeighborGrid3D/..."` → `/Game/NeighborGrid3D/...`），不再走 `Decompiled/` 镜像命名空间。

## 系统（.dfs → /Game/NeighborGrid3D/NS/）

| 源文件 | 构建目标 | 说明 |
| --- | --- | --- |
| `NS/NS_NeighborGrid3D_Show.dfs` | `NS/NS_NeighborGrid3D_Show` | 展示版：彩色球 + 调试网格（Map 展示用） |
| `NS/NS_NeighborGrid3D.dfs` | `NS/NS_NeighborGrid3D` | 基础版 |
| `NS/NS_NeighborGrid3D_Out.dfs` | `NS/NS_NeighborGrid3D_Out` | 外部发射版 |
| `NS/FX_Syst_NeighborGrid3D.dfs` | `NS/FX_Syst_NeighborGrid3D` | 组件系统版 |
| `NS/NS_WhatsNeighborGrid.dfs` | `NS/NS_WhatsNeighborGrid` | 概念演示版（Leader 跟随） |

每个系统内嵌的 CustomHLSL 脚本由导出器抽取为独立脚本资产，位于
`/Game/NeighborGrid3D/NS/Scripts/`（`NS_<系统名>_<脚本名>`），`.dfs` 按路径引用它们。

## 发射器（.dfe）

- `NE/NE_DebugGrid.dfe` — 调试网格发射器（每格一个粒子，立方体 + 文字材质渲染）。
  `.dfe` 自身不生成资产（DFX5097）；在系统里用 `from` 拉入：
  `Emitter DebugGrid from "../NE/NE_DebugGrid" { ... }`（宿主需声明 `User.CellsX/Y/Z`）。
  注意：5 个系统的导出把对该发射器的继承**展平**了（DFX8014）——各系统内是独立拷贝，
  编辑本 `.dfe` 不会回灌到已构建的系统。

## 模块资产（保持二进制，被 .dfs 按路径引用）

这三个是**图模块**（多节点连线 + 多个 CustomHLSL 片段），DreamFXLang 的 `.dfm`
是一体化黑盒形态，无法忠实转写图模块；强行改写等于用未验证的新逻辑替换手工图。
因此按 DreamFX 设计保留为资产、由文本引用管理：

| 资产 | 在 .dfs 中的调用签名 | 内部结构 |
| --- | --- | --- |
| `NM/Grid3D_CreateUnitToWorldTransform` | `(CameraFacing, Offset, Rotation, WorldGridExtents)` → 输出 `UnitToWorld` / `WorldToUnit` | 20 个 CustomHLSL 节点 + 图连线，组合平移/旋转/缩放矩阵 |
| `NM/NM_InitializeDebugCells` | `(NeighborGrid3D, UnitToWorldMatrix, CellMeshScaleOffset, CellSpriteScale)` | 2 个 CustomHLSL（邻居计数查询变体）+ 粒子缩放写入 |
| `NM/NeighborGrid3D_SetResolution` | `(WorldGridExtents, NumCellsMaxAxis | WorldCellSize)` → `NumCellsX/Y/Z`, `Out_WorldGridExtents` | 2 个 CustomHLSL（两种分辨率算法，由 SetResolutionMethod 选择） |

若日后想文本化某个模块：先读 `dfx.ps1 schema <模块>` 与 `Docs/language/dfm.md`，
用 `.dfm` 重写后**必须** `build` 并对比模拟结果，确认等价后再替换资产。

## 构建 / 校验

```bash
# 编辑器需关闭（build 会写包）
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/NeighborGrid3D/NS/<name>.dfs -Force
# 只读校验（编辑器开着也安全）
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify DFX/NeighborGrid3D/NS/<name>.dfs
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 lint  -All
```

已知保留项（导出器 gap，各系统头部注释有记录）：发射器继承被展平（CompletelyEmpty 模板、
NE_DebugGrid）；内联算术以等价 dynamic-input 链还原。
