# 引脚核对清单（转换 .dfm 所需）— 2026-10 复核后仅剩一项

> 多数条目已通过算子指纹 / CustomHLSL 全文 / schema 自答；Fill/Query（矩阵输入）、
> GetLeader×2 / SetLeaderParticle（输出引脚）、NM×3 已确认触语言边界，保留二进制（见 README）。
> **当前仅剩下表一项** —— 提供后即可完成最后一个可转换脚本。

## UpdatePosition（NS_WhatsNeighborGrid_UpdatePosition）— 12 节点，写值型

模块输入已知：`AttributeReader`（DI<ParticleRead>）。
图内 4 个 FunctionCall 已确认：`Get ID At Spawn Index`（FC_2）、`Get Vector By ID`（FC_10）、
`Get Vector By Index`（FC_20）、`Get ID By Index`（FC_25）。

需要你从图里读的：

| 项 | 问题 |
| --- | --- |
| Leader 的定位方式 | `Get ID At Spawn Index` 的 Spawn Index 连的什么常量/参数？（Leader 是不是固定为某一生成序号） |
| Select_0 | 条件输入是什么、两个分支分别来自哪个 FC 的输出 |
| Op_98 | 运算类型（加/减/乘/除/max/min…）和两个操作数 |
| ParamMapSet_6 | 写回的属性（预期 Particles.Position）和来源（预期 原位置+朝向 Leader 的位移） |
| Convert_4 / Convert_5 | 转换内容（预期 Position↔Vector 类转换） |

## 已完成 / 已定案（无需提供）

- ✅ 4× `*_InitializeGrid`、`Whats_IntByTIme`、3× `*_UpdateHit` —— 已转 `.dfm` 并 schema 等价验证
- ✅ FillGridModule ×4 —— CustomHLSL 全文 + FunctionCall 链自洽，但被**矩阵输入边界**（DFX4021）堵死
- 🚫 QueryGrid ×4 —— 同上（矩阵输入）
- 🚫 GetLeaderPosition ×2、SetLeaderParticle —— 输出引脚（语言无 Outputs 节）
- 🚫 NM ×3 —— 2 个输出引脚 + 1 个矩阵输入
