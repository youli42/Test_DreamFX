// 整理自 /Game/NeighborGrid3D/NS/Scripts/FX_Syst_NeighborGrid3D_QueryGrid.FX_Syst_NeighborGrid3D_QueryGrid 的图。
// 原模块是 FX_Syst 专属的简化版：只做穿透分离 —— 阈值 = In_ParticleCollisionRadius * 2.0，
// 不读邻居 Radius / Velocity、不累计命中（无 Hit 输出）、也不写 Velocity。
// 原资产函数签名：(In_Position, WorldToUnitMatrix, ExecutionIndex, In_ParticleCollisionRadius) -> Out_PenetrationOffset
// 变更: WorldToUnitMatrix 拆为 W2URow0..3（DFX4021，.dfm 输入类型白名单无 matrix），
//       调用点用内联 hlsl{} 从 System 矩阵取行。
// 用途: Stage QueryGrid —— 粒子间碰撞分离（纯位置修正，标准 PBD）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
//
// 修复(2026-10-07): 迁移时本文件被写成了 NS_Out 版的克隆（多出邻居半径读取、命中强度累计、
// Hit/Velocity 写入），而 FX_Syst 的 Grid 发射器从来没有 Particles.Radius
// → 运行期每次邻居读取都 invalid 并 continue，日志刷
//   "Particle read DI is trying to access inexistent variable 'Radius' in emitter 'FX_Syst_NeighborGrid3D.Grid'"
//   （单次 PIE 979 条），且碰撞分离整体失效。现按原资产恢复简化版逻辑。
Module(Name="NeighborGrid3D/NS/Scripts/FX_Syst_NeighborGrid3D_QueryGrid", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "NeighborGrid3D";
        Description = "查询邻域网格做粒子碰撞分离（穿透修正），只写 Position。";
    }

    Inputs = {
        DI<NeighborGrid3D> NeighborGrid3D;
        DI<ParticleRead> AttributeReader;
        Vector4 W2URow0 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow1 = (0.0, 0.0, 0.0, 1.0);
        Vector4 W2URow2 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow3 = (0.0, 0.0, 0.0, 0.0);
        float In_ParticleCollisionRadius = 10.0;
    }

    Body = {
        float4x4 WorldToUnitMatrix = float4x4(W2URow0, W2URow1, W2URow2, W2URow3);

        // ==== 捕获旧值（写回前的原始属性） ====
        float3 In_Position = Particles.Position;
        int In_ExecutionIndex = ExecIndex();

        // 原 FX_Syst 版 CustomHLSL：累加器在 #if 之前显式清零（与原图一致）
        float3 Out_PenetrationOffset = float3(0.0, 0.0, 0.0);

#if GPU_SIMULATION
        float3 UnitPos;
        NeighborGrid3D.SimulationToUnit(In_Position, WorldToUnitMatrix, UnitPos);

        int3 CellIndex;
        NeighborGrid3D.UnitToIndex(UnitPos, CellIndex.x, CellIndex.y, CellIndex.z);

        int3 NumCells;
        NeighborGrid3D.GetNumCells(NumCells.x, NumCells.y, NumCells.z);

        int MaxNeighborsCount;
        NeighborGrid3D.MaxNeighborsPerCell(MaxNeighborsCount);

        int CollisionCount = 0;

        for(int x = -1; x <= 1; x++)
        {
            for(int y = -1; y <= 1; y++)
            {
                for(int z = -1; z <= 1; z++)
                {
                    const int3 CellIndexToCheck = CellIndex + int3(x, y, z);

                    if(CellIndexToCheck.x >= 0 && CellIndexToCheck.x < NumCells.x &&
                    CellIndexToCheck.y >= 0 && CellIndexToCheck.y < NumCells.y &&
                    CellIndexToCheck.z >= 0 && CellIndexToCheck.z < NumCells.z)
                    {
                        for(int i = 0; i < MaxNeighborsCount; i++)
                        {
                            int NeighborLinearIndex;
                            NeighborGrid3D.NeighborGridIndexToLinear(CellIndexToCheck.x, CellIndexToCheck.y, CellIndexToCheck.z, i, NeighborLinearIndex);

                            int NeighborIndex;
                            NeighborGrid3D.GetParticleNeighbor(NeighborLinearIndex, NeighborIndex);

                            if(NeighborIndex == -1 || NeighborIndex == In_ExecutionIndex) continue;

                            float3 NeighborPosition;
                            bool bNeighborPositionValid;
                            AttributeReader.GetPositionByIndex<Attribute="Position">(NeighborIndex, bNeighborPositionValid, NeighborPosition);

                            if(!bNeighborPositionValid) continue;

                            const float3 DeltaPosition = NeighborPosition - In_Position;
                            const float SqrdDistanceToNeighborParticle = dot(DeltaPosition, DeltaPosition);

                            if(SqrdDistanceToNeighborParticle < 0.0001) continue;

                            // 原 FX_Syst 版：阈值 = 模块半径 × 2，不读邻居 Radius（该发射器没有 Particles.Radius）
                            const float CollisionDistanceThreshold = In_ParticleCollisionRadius * 2.0;
                            const float CollisionDistanceThresholdSqrd = CollisionDistanceThreshold * CollisionDistanceThreshold;

                            if(SqrdDistanceToNeighborParticle < CollisionDistanceThresholdSqrd)
                            {
                                const float DistanceToNeighborParticle = sqrt(SqrdDistanceToNeighborParticle);
                                const float3 DirectionToNeighborParticle = DeltaPosition / DistanceToNeighborParticle;

                                Out_PenetrationOffset += DirectionToNeighborParticle * -(CollisionDistanceThreshold - DistanceToNeighborParticle);
                                CollisionCount++;
                            }
                        }
                    }
                }
            }
        }

        if(CollisionCount > 1)
        {
            Out_PenetrationOffset /= CollisionCount;
        }
#endif

        // ==== 写回（原版只写 Position） ====
        Particles.Position = In_Position + Out_PenetrationOffset;
    }
}
