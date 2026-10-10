# DFX/Boids — 基于 NeighborGrid3D 的 Boids 集群（文本源）

工作区横跨两棵树（Niagara 文本源在 DFX，关卡在 UE Content）：

| 位置 | 内容 | 对应引擎路径 |
| --- | --- | --- |
| `DFX/Boids/Niagara/` | `.dfs`（Niagara 系统）、`.dfm`（模块）、`.dfe`（发射器）文本源 | 构建到 `/Game/Boids/Niagara/...`（原位） |
| `Content/Boids/Maps/` | 关卡。`.umap` 无法由 DFX 文本表达，在编辑器内创建后保存到这里（`.gitignore` 只收 `.uasset`/`.umap`，无需占位文件） | `/Game/Boids/Maps/...` |

## 命名约定

`Name="Boids/Niagara/<资产名>"`, `Root="Game"` → 原位构建到 `/Game/Boids/Niagara/<资产名>`。

## 计划（Boids 管线，复用 NeighborGrid3D）

- **复用**（按路径引用现成资产，不复制）：
  - `NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_InitializeGrid`（共享规范版；2026-10 起四系统合一份）→ SystemSpawn 里 `SetNumCells`
  - `NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_FillGridModule`（共享规范版）→ Stage FillGrid
  - `NeighborGrid3D/NM/Grid3D_CreateUnitToWorldTransform`（图模块）→ SystemUpdate
- **新写**（仅 2 个文本文件）：
  - `Niagara/BoidsQueryGrid.dfm` — 27 邻格查询 + 分离/对齐/聚合三规则（steering 形式），
    循环骨架与 `NS_NeighborGrid3D_QueryGrid.dfm`（共享规范版，算法以 Out 为准）一致，仅替换循环体；
  - `Niagara/NS_Boids.dfs` — 系统骨架抄 `NS_NeighborGrid3D_Out.dfs`，
    删 CurlNoise/Vortex/PointAttraction/UpdateHit，速度交由三规则驱动。
- **硬约束**：`CellSize = GridSize / Cells ≥ PerceptionRadius`（27 邻格必须完整覆盖感知球），
  Cells 按感知半径反推。
- **实现规则**：累加器在 `#if GPU_SIMULATION` 之前清零；矩阵输入拆 `W2URow0..3`（DFX4021）；
  `Type Particles.X = ...;` 声明行上方不得有注释；Stage 只写 `Velocity`（位置积分由引擎负责，
  Stage 运行在积分之后，勿双重积分）。
- **参数**：Wsep≈1.5 / Walgn≈1.0 / Wcoh≈1.0；Rsep≈0.3–0.5·R；
  MinSpeed < MaxSpeed ≤ SolveForces 的 SpeedLimit(500)。

## 构建 / 校验

```bash
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify DFX/Boids/Niagara/NS_Boids.dfs
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build  DFX/Boids/Niagara/NS_Boids.dfs -Force
```
