# DFX/Boids — 基于 NeighborGrid3D 的 Boids 集群（文本源）

工作区横跨两棵树（Niagara 文本源在 DFX，关卡在 UE Content）：

| 位置 | 内容 | 对应引擎路径 |
| --- | --- | --- |
| `DFX/Boids/Niagara/` | `NS_Boids.dfs`（Niagara 系统）文本源 | 构建到 `/Game/Boids/Niagara/NS_Boids`（原位） |
| `DFX/Boids/Niagara/NM/` | 4 份 `.dfm`（模块）文本源（含调试网格 `BoidsDebugGrid`） | 构建到 `/Game/Boids/Niagara/NM/...`（原位） |
| `DFX/Boids/Niagara/NE/` | `BoidsDebugGrid.dfe`（可复用调试发射器）。`.dfe` 自身不产资产（DFX5097），由 `.dfs` 的 `from` 拷入 | 无（只有文本源） |
| `Content/Boids/Niagara/NM/` | 上述 `.dfm` 的构建产物（`.uasset`，不入库） | 同上 |
| `Content/Boids/Maps/` | 关卡。`.umap` 无法由 DFX 文本表达，在编辑器内创建后保存到这里（`.gitignore` 只收 `.uasset`/`.umap`，无需占位文件） | `/Game/Boids/Maps/...` |

## 命名约定

源文件所在目录镜像资产目录：`Name=` 就是资产路径去掉 `/Game/` 前缀，`Root="Game"` → 原位构建。
例：`DFX/Boids/Niagara/NM/BoidsQueryGrid.dfm` 的 `Name="Boids/Niagara/NM/BoidsQueryGrid"`
→ `/Game/Boids/Niagara/NM/BoidsQueryGrid`。

`NM/` 放模块、`NE/` 放可复用发射器、`Niagara/` 根放系统（与 NeighborGrid3D 的 `NS/` + `NM/` + `NE/` 分层同构）。
模块与系统分目录后，系统里的调用点必须写成 `/Game/Boids/Niagara/NM/<模块名>(...)`；
发射器用 `Emitter <名字> from "./NE/<发射器名>" { }` 拉入（路径相对引用文件解析，扩展名可省）。

## 计划（Boids 管线，复用 NeighborGrid3D）

- **复用**（按路径引用现成资产，不复制；2026-10-10 起这些基础设施已上移至共享目录
  `DFX/Shared/NeighborGrid/`，与 NeighborGrid3D 演示目录解耦）：
  - `Shared/NeighborGrid/Scripts/NeighborGrid_InitializeGrid`（原 `NS_NeighborGrid3D_InitializeGrid`，四系统合一份）→ SystemSpawn 里 `SetNumCells`
  - `Shared/NeighborGrid/Scripts/NeighborGrid_FillGridModule`（原 `NS_NeighborGrid3D_FillGridModule`）→ Stage FillGrid
  - `Shared/NeighborGrid/NM/Grid3D_CreateUnitToWorldTransform`（图模块，编辑器内迁移）→ SystemUpdate
- **新写**：
  - `Niagara/NM/BoidsQueryGrid.dfm` — 27 邻格查询 + 分离/对齐/聚合三规则（steering 形式），
    循环骨架与 `NS_NeighborGrid3D_QueryGrid.dfm`（共享规范版，算法以 Out 为准）一致，仅替换循环体；
  - `Niagara/NS_Boids.dfs` — 系统骨架抄 `NS_NeighborGrid3D_Out.dfs`，
    删 CurlNoise/Vortex/PointAttraction/UpdateHit，速度交由三规则驱动；
  - 调试工具 2 份（`Niagara/NE/BoidsDebugGrid.dfe` + `Niagara/NM/BoidsDebugGrid.dfm`）——见下节，
    与系统解耦：系统侧只需一个 `from` 块，可以单独提交或搬到别的系统。
- **硬约束**：`CellSize = GridSize / Cells ≥ PerceptionRadius`（27 邻格必须完整覆盖感知球），
  Cells 按感知半径反推。
- **实现规则**：累加器在 `#if GPU_SIMULATION` 之前清零；矩阵输入拆 `W2URow0..3`（DFX4021）；
  `Type Particles.X = ...;` 声明行上方不得有注释；Stage 只写 `Velocity`（位置积分由引擎负责，
  Stage 运行在积分之后，勿双重积分）。
- **参数**：Wsep≈1.5 / Walgn≈1.0 / Wcoh≈1.0；Rsep≈0.3–0.5·R；
  MinSpeed < MaxSpeed ≤ SolveForces 的 SpeedLimit(500)。

## 调试工具：网格可视化（可单独拿走）

三份文本源就是一个自包含的调试工具，系统侧只多一个 `from` 块：

| 文件 | 作用 |
| --- | --- |
| `Niagara/NE/BoidsDebugGrid.dfe` | 可复用发射器：`CellsX×CellsY×CellsZ` 个粒子。MeshRenderer 画格子（占用≈满格、空格小点；绿=正常、红=溢出），SpriteRenderer 用下面的文字材质在格心打出**该格粒子数量** |
| `Niagara/NM/BoidsDebugGrid.dfm` | 摆位 + 计数 + 配色：`ExecIndex → (i,j,k)` → 单元中心；读该格登记计数；写 `Scale` / `SpriteSize` / `Color` / `DynamicMaterialParameter` |
| `DShader/Boids/Material/M_BoidsCellCount.dss` | 数字材质（构建到 `/Game/Boids/Material/M_BoidsCellCount`）：字形取引擎材质函数 `DebugScalarValues`，**颜色取粒子色**，于是数字随状态变色（绿/红）；`Gain` 是自发光增益 |

宿主系统需要满足：声明 `User.CellsX/Y/Z` 与 `User.GridSize`、有 `DI<NeighborGrid3D> System.MyNeighborGrid3D`
且每帧有人往里面登记粒子、把该发射器声明在填充网格的发射器**之后**（前两条由 `DFX3043` 在 `from` 行上检查）。
系统里的写法（`Settings` 按 key 合并、栈与渲染器是整块替换，可在块里覆盖）：

```cpp
Emitter DebugGrid from "./NE/BoidsDebugGrid" { }
```

为什么不复用 NeighborGrid3D 的 `NE/NE_DebugGrid.dfe` + `NM/NM_InitializeDebugCells`：那套依赖
`Grid3D_CreateUnitToWorldTransform` 的 UnitToWorld 矩阵，而 Boids 的索引是自算的，引矩阵就多一套口径
（Owner 旋转/缩放非单位值时可视化与实际查询错位）。

模块与材质的通道约定：`Particles.DynamicMaterialParameter.x` = 是否显示（数量 > 0）、`.y` = 数量；
`Particles.Color` = 状态色。`M_BoidsCellCount` 把 `.y` 交给 `DebugScalarValues`（最多 2 位）出字形，
用 `Particles.Color` 当自发光、字形遮罩 × `.x` 当透明度 —— **数字与格子同色**。
（NeighborGrid3D 演示的 `M_Debug_CellText` 把数字颜色写死成红色常数，所以那边数字不随状态变；
需要"数字恒红"时把 `SpriteRenderer Counts` 的 `Material` 换回去即可。）
`In_SpriteScale = 0` 只关掉数字、保留网格。

数字语义 = 该格的**尝试登记数**（`GetParticleNeighborCount`）：引擎 `AddParticle` 先 `InterlockedAdd`
计数、再判有没有空槽，所以这个数**可以超过 `MaxNeighborsPerCell`** —— 超过即"有 boid 没进网格"，
也就是在丢邻居。想让每格显示得更"物理"（封顶在实际写入的槽数）就要改成逐槽扫
`GetParticleNeighbor != -1`，但那样溢出信号就没了。

颜色：**绿 = 正常占用，红 = 超过每格登记上限（溢出，正在丢邻居），暗蓝 = 空格**。
格子色写在模块里（三处颜色字面量就是全部旋钮），数字色跟着它走。

外观：占用格接近满格尺寸、空格缩成小点。若想换成自己的材质，在 `from` 块里整块替换渲染器即可。

## 构建 / 校验

四个模块按 `/Game/Boids/Niagara/NM/...` 被系统引用，所以先建模块再建系统：

```bash
# 数字材质（DreamShader 文本管线；check 只读，compile 落盘 Content/Boids/Material/）
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check   DShader/Boids/Material/M_BoidsCellCount.dss
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Boids/Material/M_BoidsCellCount.dss -Force

for m in BoidsInitializeGrid BoidsFillGrid BoidsQueryGrid BoidsDebugGrid; do
  pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build "DFX/Boids/Niagara/NM/$m.dfm" -Force
done
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build  DFX/Boids/Niagara/NS_Boids.dfs -Force
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify DFX/Boids/Niagara/NS_Boids.dfs
```

> 本机 `EngineAssociation`（5.8）未写入注册表，`dfx.ps1` 解析不到引擎，需追加
> `-Engine '<引擎根>'`（如 `C:\Program Files\Epic Games\UE_5.8`）或先设 `$env:UE_ENGINE_ROOT`；
> 否则报 `EngineAssociation '5.8' is not registered`。
> **⚠ 系统重建请用 headless `dfx.ps1 build`，不要在编辑器里让 watcher 增量重建。** 2026-10-15 实测：
> 改 `.dfe`（换 `SpriteRenderer.Counts` 的材质）后，编辑器的 watcher 在重建 `NS_Boids` 时崩溃
> （`EXCEPTION_ACCESS_VIOLATION`，栈为 `DreamFXEditor!FNiagaraAdapter::AddUserVariable`
> ← `ApplyPlan` ← `FGenerator::Generate` ← `DreamFXSourceWatcher::DrainQueue`），整个编辑器退出；
> **同一份源码用 `dfx.ps1 build ... -Force` 一次通过**（1 built / 0 error）。所以改系统相关文本后：
> 关掉编辑器 → headless 构建 → 再开编辑器看。编辑 `.dss` 不受影响（DreamShader watcher 只重生成材质，
> 且已落盘的材质会"rebuild and save on disk"）。另外别在 watcher 重建同一批包的同时跑第二个引擎实例
> （`dsc` / `dfx`），两次引擎写同一批 `.uasset` 是另一个已知风险。
>
> `.dfe` 不产资产，没有单独的构建步骤；改它会通过源码依赖图重建引用它的系统（`from` 是拷贝语义，
> 已构建的系统不重建就不会跟着变）。
>
> **构建安全门（两类都会遇到）**：
> 1. 调试网格写 `Particles.DynamicMaterialParameter` 之后，渲染器的材质参数通道变为有效
>    （facts 里 `MaterialParamValidMask` 由 0 变 15）；
> 2. 模块输入若**吃默认值**而调用点没点名（例如 `In_SpriteScale`），一旦资产里的常量与模块默认值不同，
>    就会被判成"源码无法表达的漂移"。
>
> 两者都会让 `build` 报 `DFX8018` 并拒绝保存（"the text does not name this input"）。第 1 类确认
> `Saved/DreamFX/BuildSafety/` 里丢的只有那一条后加 `-Force`；第 2 类更干净的做法是在调用点把该输入
> **显式点名**（本工具里 `In_SpriteScale = 0.6` 就是这么写的）。

