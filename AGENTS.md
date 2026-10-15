# AGENTS.md —— 本仓库的 Agent 与协作者约定

## 1. 可以从文本编译出来的 uasset 不进入 git 管理

**凡是可以由 DreamFXLang 文本源（`DFX/**/*.dfs`，以及生成模块的 `.dfm`）编译出来的 `.uasset`，一律不纳入 git 管理。**

理由：文本源是唯一真源，资产是 `dfx.ps1 build` 的产物，随时可由文本重建；把产物入库会让同一次改动产生两份需要保持同步的副本（编辑器手改、构建重写、合并冲突），并让仓库体积随每次构建增长。

具体要求：

- 不要 `git add` 这类资产，也不要以「让克隆后开箱即用」为由把它们提交回来；
- 新增文本源时，同时在 `.gitignore` 里排除它的输出路径，避免产物被顺手提交；
- 已经入库的这类资产，应连同 `.gitignore` 规则一起在**同一次提交**里删除；
- `.dfe` 是例外——它自身不生成任何资产（DFX5097），只被系统以 `from` 语句引用，因此没有需要排除的产物。

### 已排除的路径

```
/Content/NeighborGrid3D/NS/*.uasset      NS_NeighborGrid3D、NS_NeighborGrid3D_Out、NS_NeighborGrid3D_Show、
                                         NS_WhatsNeighborGrid、FX_Syst_NeighborGrid3D
/Content/NeighborGrid3D/NS/Scripts/      4 个脚本（UpdateHit / QueryGrid / FX_Syst_QueryGrid / IntByTIme）
/Content/Shared/NeighborGrid/Scripts/    2 个共享模块（InitializeGrid / FillGridModule，2026-10-10 自 NeighborGrid3D 迁入）
/Content/Samples/Niagara/*.uasset        NS_HelloBurst、NS_CurlSmokeWisp、NS_MagicFountain
/Content/Decompiled/                     旧的反编译镜像位置（已废弃）
```

### 预期后果

克隆本仓库后，上述路径下的资产并不存在，引用它们的
`Content/NeighborGrid3D/Maps/LE_NeighborGrid3D.umap`、`Content/Samples/Map/L_NiagaraShowcase.umap`、
`Content/NeighborGrid3D/BP/BP_SpawnNiagara.uasset` 等会显示资产缺失。

**这是预期状态，不是拉取失败、也不是仓库损坏。** 恢复方式是在本地构建一次：

```powershell
pwsh -File Plugins/DreamFX/.skill/dfx.ps1 build -All -Force
```

构建前请关闭编辑器：两个进程同时写同一个包会静默互相覆盖。

同理，`dfx.ps1 verify` 的漂移门在全新克隆上会先报错，因为它需要资产存在才能与源对比——先 `build`，再 `verify`。

## 2.所有提交必须带 issue 单号

**每条提交信息都必须以 `#<issue 号>` 开头**，格式为 `#12 type(scope): 摘要`：

```
#6 feat(Boids): 边界支持 Sphere/Box 可选
#2 fix(Boids): 软边界回收替代硬阈值，消除贴壳切向环流
```

理由：改动的"为什么"在 issue 里（现象、复现、验收标准、讨论结论），提交信息只装得下"做了什么"。单号是这两者之间唯一的链接——没有它，半年后 `git blame` 只能看到一句 `fix: 调整边界参数`，无法还原当时要解决的现象、也没法判断验收标准是否还成立。本仓库的 issue（`gh issue list`）就是设计与验收的记录处，别让它和代码脱钩。

具体要求：

- 单号写在**最前面**，`type(scope)` 与摘要照旧跟在后面（与 `#2` 那条历史提交一致）；
- 一次提交只对应一个 issue；确实横跨多个 issue 时**拆成多次提交**，而不是在一行里堆单号；
- 没有对应 issue 就先开一个（哪怕只有现象和验收），不要用"无单号"提交绕过；
- 修复类提交在摘要里点明现象，别只写"修复 bug"。

这条同样适用于 agent：**提交前先确认改动对应的 issue 号**；拿不准就停下来问，不要自己编一个号。
