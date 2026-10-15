# Test_DreamFX 工程导览

用文本写 Niagara 特效的 UE 5.8 试验工程。**文本是真源**：`Content/` 里由文本生成的 `.uasset` 是构建产物、
不入库，所以克隆后缺资产是正常的，构建一次就回来（编辑器先关掉；本机引擎没注册，要显式给引擎根）：

```bash
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build -All -Force -Engine 'C:\Program Files\Epic Games\UE_5.8'
```

## Content/ 下的效果目录

1. `NeighborGrid3D/` —— 邻域网格工具箱：把空间切成格子快速查「谁在我附近」，做粒子间碰撞与分离。
2. `Boids/` —— 鸟群 flocking：800 个 boid 用邻域网格找邻居，做分离 / 对齐 / 聚合。
3. `Samples/` —— 三份入门教程特效：一次爆裂、魔法喷泉、GPU 卷曲烟雾。
4. `Shared/` —— 不是独立效果，是从网格演示里抽出的公共设施，供上面几个效果按路径复用。
5. `Base/` —— 公共展示蓝图，负责在展示关里播放指定系统。

想直接看效果就打开这四张图：`Boids/Maps/LE_Boids`、`NeighborGrid3D/Maps/LE_NeighborGrid3D_Show`、
`NeighborGrid3D/Maps/LE_NeighborGrid3D`、`Samples/Map/L_NiagaraShowcase`。
网格类效果通用的配色自检：青蓝 = 邻居充足，黄 = 在边缘，红 = 一个邻居都没找到（出格或网格没盖住）。

## 逐个展开

### 1. `Content/NeighborGrid3D/` —— 邻域网格工具箱（核心）

- **效果**：粒子间碰撞与分离 —— 每个粒子查自己所在格子及 27 个邻格，和邻居算分离；配调试网格可视化。
- **`NS/`** 5 个系统：基础版、Show 展示版（彩色球 + 调试网格）、Out 外部发射版、FX_Syst 组件版、
  Whats 概念版（Leader 跟随）。`NS/Scripts/` 是模块产物。
- **`NM/`** 2 个编辑器手搭的图模块：按单元尺寸或轴数反推网格分辨率、把粒子摆进所在格子（用来把网格画出来）。
- **`NE/`** 调试网格发射器：每格一个粒子，配格子材质和格上文字。
- **`Maps/`** 两张演示关：基础关（蓝图生成系统）、展示关（彩色球 + 调试网格 + 水池 + 球砸水）。
- **`BP/` `LS/` `Material/` `Mesh/`**：系统生成器与展示蓝图、球砸水池的关卡序列、
  池水与调试格/格上文字/碰撞发光球材质、池子网格。
- 文本源在 `DFX/NeighborGrid3D/`，算法与迁移史见那里的 README。

### 2. `Content/Boids/` —— 鸟群 flocking

- **效果**：800 个 boid 用同一套邻域网格感知邻居，按分离 / 对齐 / 聚合三规则转向，成群飞行不散。
- `Niagara/NS_Boids` 是系统；`Niagara/NM/` 三个模块：初始化网格、把粒子登记进格子、27 邻格查询 + 三规则。
- `Maps/LE_Boids` 是演示关。文本源在 `DFX/Boids/`，参数硬约束（CellSize ≥ 感知半径等）见那里的 README。

### 3. `Content/Samples/` —— 入门教程特效

- **效果**：三份各教语言一个方面的示例系统 —— `NS_HelloBurst` 语言骨架与一次爆裂、
  `NS_MagicFountain` 用户参数与蓝图覆盖、`NS_CurlSmokeWisp` GPU 粒子卷曲烟雾。
- 三份同台展示在 `Map/L_NiagaraShowcase`。文本源在 `DFX/Samples/`。

### 4. `Content/Shared/` —— 公共网格设施（被复用）

- **效果**：本身不产生画面，是把网格基础能力抽出来给多个效果共用，避免每份系统各拷一套。
- `NeighborGrid/NM/Grid3D_CreateUnitToWorldTransform`：世界坐标 ↔ 单元坐标的矩阵图模块，被文本按路径引用。
- `NeighborGrid/Scripts/`：两份共享脚本模块（初始化网格、把粒子登记进格子）的构建产物，四个网格系统共用
  （本地还没生成，见文末差异）。文本源在 `DFX/Shared/NeighborGrid/Scripts/`。

### 5. `Content/Base/` —— 公共展示蓝图

- **效果**：`BP/BP_NiagaraShow` 负责在展示关里播放指定的网格系统；展示关和
  `BP_NiagaraShow_NeighborGrid` 都继承它。编辑器资产，没有文本源。

## 其他目录（不是效果，但用得上）

- **`DFX/`** —— 特效的**文本源**，要改特效改这里，分 `Boids` / `NeighborGrid3D` / `Samples` / `Shared` 四组。
  规矩：源文件放哪资产就建在哪（`Name=` 就是资产路径去掉 `/Game/`）。
- **`DShader/`** —— 材质文本源与自渲染通道注册表：目前只有 DreamShader 生成的空槽文件，
  工程还没写材质文本，`Content/` 里的材质都是编辑器资产。
- **`Plugins/`** —— `DreamFX`（特效语言，构建脚本 `.skill/dfx.ps1`）、`DreamShader`（材质语言）、
  `UE_MCP_Bridge`（让 AI 直接操作编辑器的桥）；前两个是 git 子模块。配套技能说明在 `.agents/skills/`。
- **`Source/`** —— C++ 游戏模块空壳；**`Config/`** —— DX12 + SM6、Lumen 与光追、默认关卡等 `.ini`。
- **`Binaries/` `Intermediate/` `DerivedDataCache/` `Saved/`** —— 编译与运行时生成物，可无视。

命名前缀：`NS_` / `NE_` / `NM_` = 系统 / 发射器 / 模块（`NM_` 多为手搭图模块）；
`M_` / `MI_` / `MF_` = 材质 / 材质实例 / 材质函数；`LE_` / `L_` / `LS_` / `BP_` = 演示关卡 / 关卡 / 关卡序列 / 蓝图。

## 常用命令

```bash
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build  DFX/Boids/Niagara/NS_Boids.dfs -Force -Engine '<引擎根>'  # 改完文本必做
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify DFX/Boids/Niagara/NS_Boids.dfs          -Engine '<引擎根>'  # 只读校验
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build  -All -Force -Engine '<引擎根>'                              # 全量重建
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 verify -All        -Engine '<引擎根>'                              # 全量校验
```

## 已知差异与坑

- **本地缺 3 个产物**：`NS_WhatsNeighborGrid`、`Shared/NeighborGrid/Scripts/` 的两份共享模块
  → `build -All -Force` 补齐。另有 2 个历史孤儿 `NS_NeighborGrid3D_InitializeGrid` / `_FillGridModule`
  留在 `NeighborGrid3D/NS/Scripts/`，已被 Shared 版取代，可在编辑器里删掉。
- **构建前关编辑器**：两边同时写同一个包，后保存的会静默覆盖前者。
- **改名要用 `dfx.ps1 rename`**：发射器名是稳定键，直接改文本会丢掉旧发射器（连同它的 GUID）。
- **路径即资产**：改 `Name=` 或挪源文件位置 = 换成另一个资产，旧资产变孤儿。
- **`.dfm` 的 Body 里**，`类型 Particles.X = ...;` 这类声明正上方不能有注释行，否则类型名会被当成代码。
- **别在编辑器里改**由文本生成的资产，下一次构建会覆盖掉。
