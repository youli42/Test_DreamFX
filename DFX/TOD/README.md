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

| 系统 | 说明 |
|---|---|
| `NS_TOD_DustMotes` | 环境浮尘:大体积低密度慢漂微粒,日间逆光氛围。M2 起可由天气状态机经用户参数调密度 |

## 与 C++ 的关系

- 时间与天体驱动在 `Source/Test_DreamFX/TOD/`(`UTODSubsystem` 世界时钟 + `ATODSkyManager` 光照装备)。
- 本目录只放粒子表现;天气状态机(后续里程碑)通过用户参数(如 `User.SpawnRate`)驱动这些系统。
