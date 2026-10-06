# DFX/NeighborGrid3D — NeighborGrid3D 工具箱文本源

由 Test_Niagara 的 NeighborGrid3D 演示内容迁移而来。所有 `.dfs` 直接构建**原位**资产
（`Name="NeighborGrid3D/..."` → `/Game/NeighborGrid3D/...`），不再走 `Decompiled/` 镜像命名空间。

## 当前状态与已知缺陷（2026-10-07）

四个 `*_QueryGrid` 已由 `.dfm` 原位接管（`WorldToUnitMatrix` 拆成 `W2URow0..3`，绕过 DFX4021 的 matrix 输入白名单）。

### 本次已修（文本层）

- **累加器零初始化**：原图 `CustomHLSL_0` 在 `#if` 之前有 `Out_Out_PenetrationOffset = float3(0,0,0);`，迁移时丢失 →
  累加器初值未定义（CPU 路径 `#if` 整段被编译掉时，等于每帧把未定义向量加到 `Position` 上）。四个 `.dfm` 已补回。
- **FX_Syst 恢复原模块**：FX_Syst 原版是**简化版** —— 阈值 `In_ParticleCollisionRadius * 2.0`，
  不读邻居 `Radius`/`Velocity`、无 `Hit` 输出、不写 `Velocity`。
  迁移时该模块被写成 NS_Out 版克隆，于是去读该发射器根本不存在的 `Particles.Radius`
  → 单次 PIE 刷 979 条 `Particle read DI is trying to access inexistent variable 'Radius'`，
  而且每次邻居读取都 `continue`，碰撞分离整体失效。现已按原资产恢复，调用点同步去掉 `In_Hit`/`In_PreviousPosition`/`In_Age`。

### 仍然不正确（本轮只记录，未修）

- ❌ **碰撞发光缺失**：发光逻辑在原 QueryGrid 模块图里，重写时没有搬进 `.dfm`。
  注意：现有 `.dfm` 里 `Out_Hit` 的累计与 `Particles.Hit` 的写入是**在**的，所以缺的并不是这两句 ——
  下一步要先还原原图 QueryGrid 的完整节点清单，再决定补哪一段。
- ❌ **球与球之间的碰撞结果不正确**：粒子间分离的最终表现与原资产不一致（待定位）。
- 速度**问题**已消失（`Particles.Velocity = In_Velocity` 直通，不再有出生即飞走的巨大初速），
  但这仍是**改写**而非等价还原：原式是 `Age < DeltaTime ? Velocity : (Position − PreviousPosition) * InvDeltaTime`，
  且读的是**修正后**的 Position；`In_PreviousPosition` / `In_Age` 目前在模块里是死输入。
- 四个 `*_QueryGrid` 之间仍是互相克隆（只有 FX_Syst 已按原图分化）。
- 进网格前的 `Transform Position(Simulation→World)` 未还原。当前发射器都是 world-space，恰好恒等；
  一旦发射器改成 Local Space，就会与 `Grid3D_CreateUnitToWorldTransform` 产出的世界空间矩阵对不上。

### 构建 / 提交策略

只提交文字；**编译产物不入库**（`Content/NeighborGrid3D/NS/Scripts/` 已在 `.gitignore` 中排除）。

## 系统（.dfs → /Game/NeighborGrid3D/NS/）

| 源文件 | 构建目标 | 说明 |
| --- | --- | --- |
| `NS/NS_NeighborGrid3D_Show.dfs` | `NS/NS_NeighborGrid3D_Show` | 展示版：彩色球 + 调试网格（Map 展示用） |
| `NS/NS_NeighborGrid3D.dfs` | `NS/NS_NeighborGrid3D` | 基础版 |
| `NS/NS_NeighborGrid3D_Out.dfs` | `NS/NS_NeighborGrid3D_Out` | 外部发射版 |
| `NS/FX_Syst_NeighborGrid3D.dfs` | `NS/FX_Syst_NeighborGrid3D` | 组件系统版 |
| `NS/NS_WhatsNeighborGrid.dfs` | `NS/NS_WhatsNeighborGrid` | 概念演示版（Leader 跟随） |

## 脚本（/Game/NeighborGrid3D/NS/Scripts/，20 个）

原为导出器抽取的独立脚本资产，`.dfs` 按路径引用。**2026-10 起逐个文本化**（schema → .dfm → build → 签名等价验证）：

| 状态 | 脚本 | 说明 |
| --- | --- | --- |
| ✅ 已文本化（16） | `*_InitializeGrid` ×4、`Whats_IntByTIme`、`*_UpdateHit` ×3、`*_FillGridModule` ×4、`*_QueryGrid` ×4 | `.dfm` 在 `NS/Scripts/`，`Name=` 与资产路径一致，构建即接管。矩阵输入统一拆成 `W2URow0..3` 绕过 DFX4021 |
| ⚠️ 已知缺陷（4） | `*_QueryGrid` ×4 | 碰撞发光未迁移、球间碰撞结果不正确；见「当前状态与已知缺陷（2026-10-07）」 |
| 🚫 输出引脚边界（3） | `Whats_GetLeaderPositionByID/ByIndex`、`Whats_SetLeaderParticle` | 返回值/输出引脚是它们的全部意义；`.dfm` 无 Outputs 节（DFX2017），DynamicInput 仅单表达式（DFX3037） |
| ⏳ 待引脚数据（1） | `Whats_UpdatePosition` | 写值型可转；算子/Select 接线见 `PinAudit.md` |

### 已沉淀的实现级规则（文档未载）

- `Settings.Usage` 对 Module/DynamicInput 均必填（DFX3030）；DynamicInput 必须写 `Usage = DynamicInput`（DFX3032）
- GPU 模拟路径禁止整数取模 `%`（DFX6006），用 `fmod` 浮点等价
- 自定义属性在 Body 内首次出现须带类型：`float Particles.Hit = ...旧值自读...;`（DFX3046）
- `ParseStackKind` 只认六种栈名，`.dfm` 无法声明 Stage 用法

### Stage 调用点模块输入绑定的实测陷阱（2026-10-06/07 修复记录）

- **Stage 调用点的模块输入绑定读不到上帧值**：`In_PreviousPosition = Particles.MyPrevPos`
  在 FillGrid（写入方）之后的 QueryGrid 阶段里，实测每帧读到出生默认 (0,0,0)。
  位置差重建 `V = (In_Position − In_PreviousPosition)/dt` 退化为 `V = In_Position/dt`
  ——朝离世界原点方向的巨大初速（|位置|/dt，被 SpeedLimit 钳制）。
  系统离世界原点越远越明显；SpawnRate 持续生成的系统表现为持续粒子束。
- **`Particles.Previous.Position` 是无人维护的死属性**：原始图的重建节点读它
  （作者假设引擎自动维护上帧位置），但整个 Niagara 运行时没有任何代码写入它
  （仅 NiagaraConstants.cpp 的类型注册表提及），恒为 (0,0,0)，同样触发上式。
- **修复**：QueryGrid 的速度写回改为 `Particles.Velocity = In_Velocity`——
  在 MyPrevPos 语义下位置差重建本就 ≈ 物理位移速度，直接保留物理速度语义等价，
  分离修正保持纯位置修正（标准 PBD），并免疫上述两个坑。四个系统统一应用。
- **基础版附带修复**（同日发现的两处转换丢失，对照 original.facts 还原）：
  - Grid/DebugGrid_0 发射器的 `FixedBounds = box(±100)` 被导出丢弃（DFX7101），已还原；
  - `InitializeParticle.Lifetime = 1.0` 被丢弃（→0，粒子永久存活后被 ±100 边界剔除），
    已还原；
  - `PointAttractionForce.KillWithinRadius = true` 下粒子出生在 400cm 吸引半径**内**
    → 出生即被杀，改为 `false`（吸引保留、击杀去除）。

## 发射器（.dfe）

- `NE/NE_DebugGrid.dfe` — 调试网格发射器（每格一个粒子，立方体 + 文字材质渲染）。
  `.dfe` 自身不生成资产（DFX5097）；在系统里用 `from` 拉入：
  `Emitter DebugGrid from "../NE/NE_DebugGrid" { ... }`（宿主需声明 `User.CellsX/Y/Z`）。
  注意：5 个系统的导出把对该发射器的继承**展平**了（DFX8014）——各系统内是独立拷贝，
  编辑本 `.dfe` 不会回灌到已构建的系统。

## 模块资产（保持二进制，被 .dfs 按路径引用）

这三个是**图模块**（多节点连线 + 多个 CustomHLSL 片段），DreamFXLang 的 `.dfm`
是一体化黑盒形态，无法忠实转写图模块；强行改写等于用未验证的新逻辑替换手工图。
2026-10 复核确认另有硬边界：`Grid3D_CreateUnitToWorldTransform`/`SetResolution` 有**输出引脚**（语言无 Outputs 节），
`NM_InitializeDebugCells` 有 **NiagaraMatrix 输入**（DFX4021）——三类边界与脚本同款，保留为资产、由文本引用管理：

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
