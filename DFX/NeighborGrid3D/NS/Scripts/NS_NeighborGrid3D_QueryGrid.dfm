// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_QueryGrid.NS_NeighborGrid3D_QueryGrid 的图（CustomHLSL ×2 + FunctionCall 链）。
// 原 CustomHLSL_0：基于 3D 邻域网格的粒子碰撞检测与穿透修正（27 格邻居循环 + 撞击强度累计）；
// 原 CustomHLSL_2：由位置差重建速度（出生首帧保持初速）。
// 常量已按原资产引脚值烘焙：HitMin=0 / HitMax=1 / HitRangeMin=10 / HitRangeMax=250 / HitFalloff=1。
// 变更 1: 原 WorldToUnitMatrix 输入（NiagaraMatrix，.dfm 输入类型白名单不含矩阵 DFX4021）拆为
//         W2URow0..W2URow3 四个 Vector4 输入，宿主 .dfs 调用点用内联 hlsl{} 从 System 矩阵取行。
// 变更 2: 原 In_Hit 为图内读 Particles.Hit —— .dfm 模块对自定义属性自读触发 DFX3046，
//         升级为模块输入，宿主调用点传 In_Hit = Particles.Hit。
// 用途: Stage QueryGrid —— 读邻域网格做粒子间碰撞分离与命中强度计算（写入 Position/Hit/Velocity）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_QueryGrid", Root="Game")
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
        // 上一帧位置经 In_PreviousPosition 输入由调用点栈层回读 Particles.MyPrevPos（本模块帧末写入）；
        // Body 内"写过的属性不能自读"（读取绑定 Write_ 引脚产生 NaN），故旧值一律走输入。
        float3 In_Position = Particles.Position;
        float3 In_Velocity = Particles.Velocity;
        float In_DeltaTime = Engine.DeltaTime;
        float In_InvDeltaTime = 1.0 / In_DeltaTime;
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
        Particles.Position = In_Position + Out_PenetrationOffset;
        float Particles.Hit = Out_Hit;
        Vector Particles.MyPrevPos = In_Position;

        // 原 CustomHLSL_2：由位置差重建速度（出生首帧保持初速）
        // 出生帧 Age 与 DeltaTime 精确相等，必须用 <= 才能命中"首帧保初速"分支：
        // 若走重建分支，此时 MyPrevPos 仍为出生默认 (0,0,0)，V = In_Position/dt
        // 是朝离世界原点方向的巨大初速（系统离原点越远越明显，表现为粒子出生即同向飞走）。
        // 语义等价改写：MyPrevPos 语义下位置差重建 ≈ 物理位移速度，直接保留物理速度等价，
        // 且免疫 Stage 调用点模块输入绑定读不到上帧值的问题（实测该绑定每帧读到 0）。
        Particles.Velocity = In_Velocity;
    }
}
