// 规范共享模块（2026-10 合并）：基础版 / Out / Show / FX_Syst 四个系统共用本资产。
//   四份 *_FillGridModule 原本 Body 逐字节相同，仅 Name 不同，故合并为这一份；
//   Out/Show/FX_Syst 的同名脚本资产已无源文件、成为孤儿（见 README）。
// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_FillGridModule.NS_NeighborGrid3D_FillGridModule 的图。
// 原图为 CustomHLSL 与 FunctionCall 链（SimulationToUnit → UnitToIndex → AddParticle）双实现，逻辑一致；
// 本 Body 采用 CustomHLSL 全文（作者含中文注释），In_ExecIndex 由 ExecIndex() 内建等价替代。
// 变更: 原 WorldToUnitMatrix 输入（NiagaraMatrix，.dfm 输入类型白名单不含矩阵 DFX4021）拆为
//       W2URow0..W2URow3 四个 Vector4 输入，宿主 .dfs 调用点用内联 hlsl{} 从 System 矩阵取行。
// 用途: Stage FillGrid —— 把每个粒子按位置登记进 NeighborGrid3D 的对应单元（DI 写入阶段）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_FillGridModule", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "NeighborGrid3D";
        Description = "把每个粒子按世界坐标登记进邻域网格对应单元（SimulationToUnit → UnitToIndex → AddParticle）。";
    }

    Inputs = {
        DI<NeighborGrid3D> NeighborGrid3DBase;
        Vector4 W2URow0 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow1 = (0.0, 0.0, 0.0, 1.0);
        Vector4 W2URow2 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow3 = (0.0, 0.0, 0.0, 0.0);
    }

    Body = {
        float4x4 WorldToUnitMatrix = float4x4(W2URow0, W2URow1, W2URow2, W2URow3);

        // 注册粒子信息

        // 确保运行在 GPU 模拟阶段（In_NeighborGrid3DBase是GPU专用接口）
#if GPU_SIMULATION

        // 将粒子数量写入网格

        // 粒子位置从世界空间转换到单位空间
        float3 UnitPos;
        NeighborGrid3DBase.SimulationToUnit(Particles.Position, WorldToUnitMatrix, UnitPos);
        // NeighborGrid3DBase.SimulationToUnit(in float3 In_Simulation, in float4x4 In_SimulationToUnitTransform, out float3 Out_Unit);

        // 将单位空间位置转换为网格索引
        // int X, Y, Z = 0;
        int3 CellIndex;
        NeighborGrid3DBase.UnitToIndex(UnitPos, CellIndex.x, CellIndex.y, CellIndex.z);
        // NeighborGrid3DBase.UnitToIndex(in float3 In_Unit, out int Out_IndexX, out int Out_IndexY, out int Out_IndexZ);

        // 将粒子添加到网格中
        bool bSuccess;
        NeighborGrid3DBase.AddParticle(CellIndex.x, CellIndex.y, CellIndex.z, ExecIndex(), bSuccess);
        // NeighborGrid3DBase.AddParticle(in int In_IndexX, in int In_IndexY, in int In_IndexZ, in int In_ParticleIndex, out bool Out_Success);

#endif
    }
}
