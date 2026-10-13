# DFX/Boids — 基于 NeighborGrid3D 的 Boids 集群（文本源）

工作区横跨两棵树（Niagara 文本源在 DFX，关卡在 UE Content）：

| 位置 | 内容 | 对应引擎路径 |
| --- | --- | --- |
| `DFX/Boids/Niagara/` | `NS_Boids.dfs`（Niagara 系统）文本源 | 构建到 `/Game/Boids/Niagara/NS_Boids`（原位） |
| `DFX/Boids/Niagara/NM/` | 3 份 `.dfm`（模块）文本源 | 构建到 `/Game/Boids/Niagara/NM/...`（原位） |
| `Content/Boids/Niagara/NM/` | 上述 `.dfm` 的构建产物（`.uasset`，不入库） | 同上 |
| `Content/Boids/Maps/` | 关卡。`.umap` 无法由 DFX 文本表达，在编辑器内创建后保存到这里（`.gitignore` 只收 `.uasset`/`.umap`，无需占位文件） | `/Game/Boids/Maps/...` |

## 命名约定

源文件所在目录镜像资产目录：`Name=` 就是资产路径去掉 `/Game/` 前缀，`Root="Game"` → 原位构建。
例：`DFX/Boids/Niagara/NM/BoidsQueryGrid.dfm` 的 `Name="Boids/Niagara/NM/BoidsQueryGrid"`
→ `/Game/Boids/Niagara/NM/BoidsQueryGrid`。

`NM/` 放模块、`Niagara/` 根放系统（与 NeighborGrid3D 的 `NS/` + `NM/` 分层同构）。
模块与系统分目录后，系统里的调用点必须写成 `/Game/Boids/Niagara/NM/<模块名>(...)`。

## 计划（Boids 管线，复用 NeighborGrid3D）

- **复用**（按路径引用现成资产，不复制；2026-10-10 起这些基础设施已上移至共享目录
  `DFX/Shared/NeighborGrid/`，与 NeighborGrid3D 演示目录解耦）：
  - `Shared/NeighborGrid/Scripts/NeighborGrid_InitializeGrid`（原 `NS_NeighborGrid3D_InitializeGrid`，四系统合一份）→ SystemSpawn 里 `SetNumCells`
  - `Shared/NeighborGrid/Scripts/NeighborGrid_FillGridModule`（原 `NS_NeighborGrid3D_FillGridModule`）→ Stage FillGrid
  - `Shared/NeighborGrid/NM/Grid3D_CreateUnitToWorldTransform`（图模块，编辑器内迁移）→ SystemUpdate
- **新写**（仅 2 个文本文件）：
  - `Niagara/NM/BoidsQueryGrid.dfm` — 27 邻格查询 + 分离/对齐/聚合三规则（steering 形式），
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

三个模块按 `/Game/Boids/Niagara/NM/...` 被系统引用，所以先建模块再建系统：

```bash
for m in BoidsInitializeGrid BoidsFillGrid BoidsQueryGrid; do
  pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build "DFX/Boids/Niagara/NM/$m.dfm" -Force
done
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build  DFX/Boids/Niagara/NS_Boids.dfs -Force
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify DFX/Boids/Niagara/NS_Boids.dfs
```

> 本机 `EngineAssociation`（5.8）未写入注册表，`dfx.ps1` 解析不到引擎，需追加
> `-Engine '<引擎根>'`（如 `C:\Program Files\Epic Games\UE_5.8`）或先设 `$env:UE_ENGINE_ROOT`；
> 否则报 `EngineAssociation '5.8' is not registered`。
