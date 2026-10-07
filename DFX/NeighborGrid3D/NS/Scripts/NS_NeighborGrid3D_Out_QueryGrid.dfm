// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Out_QueryGrid.NS_NeighborGrid3D_Out_QueryGrid 的图（与 NS 版同构）。
// 原 CustomHLSL_0：27 格邻居循环碰撞检测；CustomHLSL_2：位置差重建速度。
// 常量已按原资产引脚值烘焙：HitMin=0 / HitMax=1 / HitRangeMin=10 / HitRangeMax=250 / HitFalloff=1。
// 变更: WorldToUnitMatrix 拆为 W2URow0..3；In_Hit/In_PreviousPosition/In_Age 升级为模块输入（规避 DFX3046）。
// 修复(2026-10-07): 原 CustomHLSL_2 的速度重建曾被改成 `Particles.Velocity = In_Velocity;` 直通
//   （当时的理由：阶段调用点绑定 MyPrevPos 读不到值）。结果是分离只剩位置投影、没有动量反馈 →
//   密集堆叠互相穿模且不溢出盒子。现按原图恢复重建，并把 In_PreviousPosition 改绑
//   Particles.Previous.Position（引擎每帧开头会把 Position 自拷贝给它，这条通道是活的）。
// 用途: Stage QueryGrid —— 粒子间碰撞分离与命中强度计算（写入 Position/Hit/Velocity）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Out_QueryGrid", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "NeighborGrid3D";
        Description = "查询邻域网格做粒子碰撞分离（穿透修正）与撞击强度计算，写入 Position/Hit/Velocity。";
    }

    Inputs = {
        DI<NeighborGrid3D> NeighborGrid3D;
        DI<ParticleRead> AttributeReader;
        Vector4 W2URow0 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow1 = (0.0, 0.0, 0.0, 1.0);
        Vector4 W2URow2 = (0.0, 0.0, 0.0, 0.0);
        Vector4 W2URow3 = (0.0, 0.0, 0.0, 0.0);
        float In_ParticleCollisionRadius = 10.0;
        float In_Hit = 0.0;
        Vector In_PreviousPosition = (0.0, 0.0, 0.0);
        float In_Age = 0.0;
    }

    Body = {
        float4x4 WorldToUnitMatrix = float4x4(W2URow0, W2URow1, W2URow2, W2URow3);

        // ==== 捕获旧值（写回前的原始属性） ====
        float3 In_Position = Particles.Position;
        float3 In_Velocity = Particles.Velocity;
        float In_DeltaTime = Engine.DeltaTime;
        int In_ExecutionIndex = ExecIndex();

        // 原 CustomHLSL_0：碰撞检测与穿透修正（HitMin=0, HitMax=1, HitRangeMin=10, HitRangeMax=250, HitFalloff=1）
        // 修复(2026-10-07): 原 CustomHLSL_0 在 #if 之前显式清零；迁移时丢掉后累加器初值未定义
        // （CPU 路径整段被 #if 编译掉时，等于每帧把未定义向量加到 Position 上）。
        float3 Out_PenetrationOffset = float3(0.0, 0.0, 0.0);
        float Out_Hit = In_Hit;

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

                            float NeighborRadius;
                            bool bNeighborRadiusValid;
                            AttributeReader.GetFloatByIndex<Attribute="Radius">(NeighborIndex, bNeighborRadiusValid, NeighborRadius);

                            if(!bNeighborRadiusValid) continue;

                            const float CollisionDistanceThreshold = In_ParticleCollisionRadius + NeighborRadius;
                            const float CollisionDistanceThresholdSqrd = CollisionDistanceThreshold * CollisionDistanceThreshold;

                            if(SqrdDistanceToNeighborParticle < CollisionDistanceThresholdSqrd)
                            {
                                const float DistanceToNeighborParticle = sqrt(SqrdDistanceToNeighborParticle);
                                const float3 DirectionToNeighborParticle = DeltaPosition / DistanceToNeighborParticle;

                                Out_PenetrationOffset += DirectionToNeighborParticle * -(CollisionDistanceThreshold - DistanceToNeighborParticle);
                                CollisionCount++;

                                float3 NeighborVelocity;
                                bool bNeighborVelocityValid;
                                AttributeReader.GetVectorByIndex<Attribute="Velocity">(NeighborIndex, bNeighborVelocityValid, NeighborVelocity);

                                float3 HitVelocity = NeighborVelocity - In_Velocity;
                                float Hit = abs(dot(HitVelocity, -DirectionToNeighborParticle));
                                Hit = saturate((Hit - 10.0) / (250.0 - 10.0));
                                Hit = lerp(0.0, 1.0, pow(Hit, 1.0));
                                Out_Hit += Hit;
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

        // ==== 写回 ====
        // 修复(2026-10-07): 恢复原图 CustomHLSL_2 的速度重建（PBD 的 velocity update）。
        // 把本帧实际位移（力 + 分离修正）折回速度，下一帧粒子已带着分离产生的动量往外走，
        // 密集堆叠才会撑开并溢出；只做位置投影（速度直通）会让积分每帧把球重新压回重叠，
        // 而修正又按 CollisionCount 取平均 → 平衡态就是很深的互相穿模。
        float3 CorrectedPosition = In_Position + Out_PenetrationOffset;
        Particles.Position = CorrectedPosition;
        float Particles.Hit = Out_Hit;

        // In_PreviousPosition 由调用点绑定 Particles.Previous.Position（引擎每帧开头把 Position 自拷贝给它）。
        // 原先绑定自定义 Particles.MyPrevPos，实测读不到值，于是被改成直通 —— 那正是本次修掉的差异。
        Particles.Velocity = In_Age < In_DeltaTime ? In_Velocity : (CorrectedPosition - In_PreviousPosition) * Engine.InverseDeltaTime;
    }
}
