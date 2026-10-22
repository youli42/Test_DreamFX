# DFX/TOD —— 昼夜循环系统 Niagara 文本源

TOD(Time of Day)系统的 DreamFXLang 文本源。由 `Plugins/DreamFX/.skill/dfx.ps1 build` 编译为
Niagara 资产,产物落地 `Content/TOD/Niagara/`(见根 `.gitignore`,产物可随时重建,不入 git)。

## 目录

```
DFX/TOD/Niagara/   Niagara 系统源(.dfs)与脚本模块(.dfm)/发射器(.dfe)
```

## 资产命名

| 类型 | 前缀 | 例子 |
|---|---|---|
| Niagara 系统 | `NS_TOD_` | `NS_TOD_DustMotes` |
| Niagara 脚本模块(.dfm) | `TOD` + 用途 | `TODSpinDust.dfm` |
| 测试地图 | `L_TOD_` | `L_TOD_Test` |
| C++ 类 | UE 惯例 `A`/`U` | `ATODSkyManager` / `UTODSubsystem` |

## 构建

```powershell
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build DFX/TOD/Niagara/NS_TOD_DustMotes.dfs -Force
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build -All -Force   # 全量
```

构建前关闭编辑器:两个进程写同一个包会静默互相覆盖(AGENTS.md 约定)。

## 现有系统

| 系统 | 说明 | 状态 |
|---|---|---|
| `NS_TOD_DustMotes` | 环境浮尘:大体积低密度慢漂微粒,日间逆光氛围 | 构建 OK |
| `NS_TOD_Rain` | 雨丝:固定盒雨幕,`User.SpawnRateScale` 由天气文件的「天气特效强度表」驱动 | **搁置**(见下) |
| `NS_TOD_Snow` | 雪花:Curl 侧摆飘落,`User.SpawnRateScale` 由天气文件的「天气特效强度表」驱动 | **搁置**(同 Rain) |

## 已知问题:SpawnRate 型发射器不渲染(2026-10-20 诊断,issue #11)

同工程对照实验证据:

- **Burst 路径正常**:NS_Boids(SpawnBurst_Instantaneous)在同一张 L_TOD_Test、同一 -game 进程里肉眼可见(石阵上方黑色鸟群)。
- **SpawnRate 路径异常**:NS_TOD_Rain 穷举变量后仍不渲染——CPU/GPU 模拟、字面量/hlsl 输入、粒子放大至 8×140cm、固定包围盒 + QualityLevelMask 均已尝试;-game 晴天/雷雨两帧的像素统计(垂直/水平边缘计数)无显著差异。Burst 对照被 WarmupTime 3s > 粒子寿命 2.2s 清空,不可判读(记录教训:诊断用 burst 需寿命 > warmup)。
- 场景接线、C++ 天气驱动(`ATODSkyManager` 每帧写 `User.SpawnRateScale`)均验证过无误;疑为 DreamFX 生成 SpawnRate 型发射器的资产缺陷,待上游修复。
- **修复后点亮路径零改动**:`dfx.ps1 build -All -Force` 重跑即可,地图/接线/天气协议都已就位。

## 与 C++ 的关系

- 时间与天体驱动在 `Source/Test_DreamFX/TOD/`(`UTODSubsystem` 世界时钟 + `UTODWeatherSubsystem` 天气状态机 + `ATODSkyManager` 光照装备)。
- 本目录只放粒子表现;天气文件(`WP_*`)的「天气特效强度表」把系统与强度曲线配对,
  天空装备自动 spawn 并每帧写入 `User.SpawnRateScale`(0-1),强度归零即销毁——晴天下没有雨雪,
  这是设计行为。
