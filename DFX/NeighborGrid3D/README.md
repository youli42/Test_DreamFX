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

### 已修并验证：碰撞发光（2026-10-07 第二轮）

- **碰撞发光载体**：已定位 —— 原图 `UpdateHit` 在写完 `Hit` 之后还有
  `Particles.DynamicMaterialParameter = float4(Hit, Hit, Hit, Hit)`；材质 `M_FX_Spheres` 的
  Dynamic Parameter（"传递撞击事件"）× Glow 驱动 EmissiveColor。迁移时这一句整条丢失，
  于是 `Hit` 算得再对，材质端也恒为 0 → 完全没有碰撞发光（引擎碰撞与粒子间碰撞都没有）。
  三个 `*_UpdateHit.dfm` 已补回该写入，并把衰减公式对齐原图：
  `Hit = (Hit + NewHit) * (1 - clamp(FadeSpeed * DeltaTime, 0, 1))`（原图对累加值不做 saturate）。
  FX_Syst 原版就没有 UpdateHit / `DynamicMaterialParameter`，保持不加。**PIE 验证：闪光恢复。**

### 已修并验证：粒子间碰撞（2026-10-07 第三轮）

- **根因：速度写回被改成直通**。原图 `CustomHLSL_2` 是 PBD 的 velocity update：
  `Velocity = Age < DeltaTime ? Velocity : (修正后Position − Previous.Position) * InvDeltaTime`，
  把本帧实际位移（力 + 分离修正）折回速度；迁移时被改成 `Particles.Velocity = In_Velocity;`
  （当时的理由：调用点把 `In_PreviousPosition` 绑到了自定义 `MyPrevPos`，实测读不到值）。
  结果是分离只剩位置投影：积分每帧把球重新压回重叠，而修正又按 `CollisionCount` 取平均
  → 平衡态是很深的互相穿模（"穿模"），且没有动量外推 → 堆不起来、不溢出盒子。
- **修复**：Out / Show 两个 QueryGrid 模块恢复原式（`CorrectedPosition` 局部量 + 三元式），
  调用点把 `In_PreviousPosition` 改绑引擎维护的 `Particles.Previous.Position`（见下一节的纠错），
  并删掉死掉的 `MyPrevPos` 写入与声明。**PIE 验证：球互相推开、堆积并溢出盒子。**
  基础版与 FX_Syst 的原版模块就没有这段重建，不动。

### 仍然未还原 / 已知差异

- 四个 `*_QueryGrid` 之间仍是互相克隆（只有 FX_Syst 已按原图分化）。
- 进网格前的 `Transform Position(Simulation→World)` 未还原。当前发射器都是 world-space，恰好恒等；
  一旦发射器改成 Local Space，就会与 `Grid3D_CreateUnitToWorldTransform` 产出的世界空间矩阵对不上。
- 基础版 `NS_NeighborGrid3D` / FX_Syst 的 `.dfs` 还留着迁移期加的 `Vector Particles.MyPrevPos` 死声明（无害）。

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
| ⚠️ 与原图仍有差异（4） | `*_QueryGrid` ×4 | 四者互为克隆；进网格前的 `Transform Position(Simulation→World)` 未还原（当前 world-space 下恒等）。见「仍然未还原 / 已知差异」 |
| 🚫 输出引脚边界（3） | `Whats_GetLeaderPositionByID/ByIndex`、`Whats_SetLeaderParticle` | 返回值/输出引脚是它们的全部意义；`.dfm` 无 Outputs 节（DFX2017），DynamicInput 仅单表达式（DFX3037） |
| ⏳ 待引脚数据（1） | `Whats_UpdatePosition` | 写值型可转；算子/Select 接线见 `PinAudit.md` |

### 已沉淀的实现级规则（文档未载）

- `Settings.Usage` 对 Module/DynamicInput 均必填（DFX3030）；DynamicInput 必须写 `Usage = DynamicInput`（DFX3032）
- GPU 模拟路径禁止整数取模 `%`（DFX6006），用 `fmod` 浮点等价
- 自定义属性在 Body 内首次出现须带类型：`float Particles.Hit = ...旧值自读...;`（DFX3046）
- **`.dfm` Body 里 `Type Particles.X = ...;` 的声明正上方不能是注释行**：DFX 只在"语句起始"剥离类型名
  （`DreamFXModuleGenerator.cpp` 的 pass one：状态仅在 `;`/`{`/`}` 之后置位，任何非空白字符——**包括注释**——都会清掉它），
  漏剥时 DFX 的类型拼写会原样进 HLSL：`Vector4 Out_Write_… = …` → `error: use of undeclared identifier 'Vector4'`，GPU 编译失败。
  `.dfs` 的语句走另一条 lowering，不受影响。注释要放在声明**之后**，或放在局部变量声明之前。
- `ParseStackKind` 只认六种栈名，`.dfm` 无法声明 Stage 用法

### Stage 调用点模块输入绑定：一次误判与纠正（2026-10-06/07 → 2026-10-07 纠错）

- **当时的观察（成立）**：`In_PreviousPosition = Particles.MyPrevPos`（自定义属性，由**同一个模块**帧末写入）
  在 QueryGrid 阶段实测每帧读到出生默认 (0,0,0)。位置差重建因此退化为 `V = In_Position/dt`
  ——朝远离世界原点方向的巨大初速（被 SpeedLimit 钳制）；系统离原点越远越明显。
- **当时的结论（不成立，已纠正）**："`Particles.Previous.Position` 是无人维护的死属性，恒为 (0,0,0)"。
  它是**被引擎逐帧维护**的：生成的更新脚本里有
  `Context.MapUpdate.Particles.Previous.Position = Context.MapUpdate.Particles.Position;`
  （原资产与重建资产里都能搜到；写在 Update 之前，所以阶段读到的是"本帧起始位置"＝上一帧末位置）。
  ⚠️ 这条自拷贝只在**发射器引用了 `Particles.Previous.*`** 时才生成 —— 不引用就没有。
- **正确的修复（2026-10-07）**：不是删掉重建，而是换数据源 —— 调用点绑定引擎维护的
  `Particles.Previous.Position`（`.dfs` 里读它有先例：`NS_WhatsNeighborGrid.dfs` 的
  `Vector Particles.PreviousPosition = Particles.Previous.Position;`）。
  删掉重建会连带丢掉 PBD 的动量反馈 → 密集堆叠互穿（见上一节）。
- **教训**：自定义属性 `MyPrevPos` 读不到 ≠ 引擎的 `Previous.*` 通道不可用。
  先看生成代码里有没有那条自拷贝，再决定"换数据源"还是"删逻辑"。
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
