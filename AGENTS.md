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
/Content/NeighborGrid3D/NS/Scripts/      6 个共享模块（InitializeGrid / FillGridModule / UpdateHit / QueryGrid 等）
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
