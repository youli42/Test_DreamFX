// Boids 自包含模块 3/3 —— Stage Boids（群体行为）。
// 27 邻格查询（与 BoidsFillGrid 共用同一套世界→单元索引公式）+ 分离/对齐/聚合三规则，
// 以 Reynolds steer 的形式写回 Particles.Velocity。
//
// 运行语义（重要）: Stage 在 ParticleUpdate 之后执行，SolveForcesAndVelocity 已经做完位置积分，
//   所以这里**只写 Velocity**，绝不写 Position（否则双重积分）。
// 只依赖引擎的 NeighborGrid3D / ParticleRead 接口，不引用 NeighborGrid3D 工具箱任何资产。
// 注意: Name= 指向 /Game/Boids/Niagara/NM/BoidsQueryGrid，构建本文件会生成/接管该资产。
Module(Name="Boids/Niagara/NM/BoidsQueryGrid", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "Boids";
        Description = "27 邻格查询 + 分离/对齐/聚合三规则，按 Reynolds steer 写回 Particles.Velocity。";
    }

    Inputs = {
        DI<NeighborGrid3D> GridIn;
        DI<ParticleRead>   AttributeReader;
        Vector  In_GridCenter = (0.0, 0.0, 0.0);
        Vector  In_GridSize   = (4000.0, 4000.0, 4000.0);
        float   In_PerceptionRadius = 90.0;
        float   In_SeparationRadius = 32.0;
        float   In_WeightSeparation = 1.6;
        float   In_WeightAlignment  = 1.0;
        float   In_WeightCohesion   = 0.8;
        float   In_MaxSpeed         = 350.0;
        float   In_MinSpeed         = 120.0;
        float   In_MaxForce         = 800.0;
        Vector  In_AnchorPosition   = (0.0, 0.0, 0.0);
        float   In_BoundaryRadius   = 1500.0;
        float   In_BoundaryForce    = 1200.0;
    }

    Body = {
        // ==== 捕获写回前的原始值 ====
        float3 In_Position       = Particles.Position;
        float3 In_Velocity       = Particles.Velocity;
        float  In_DeltaTime      = Engine.DeltaTime;
        int    In_ExecutionIndex = ExecIndex();

        // 累加器必须在 #if GPU_SIMULATION 之前清零：清零语句若只存在于 GPU 分支，
        // CPU 路径整段被编译掉，等于每帧把未定义值加到属性上（本项目记录过的历史 bug）
        float3 SeparationAccum = float3(0.0, 0.0, 0.0);
        float3 AlignmentAccum  = float3(0.0, 0.0, 0.0);
        float3 CohesionAccum   = float3(0.0, 0.0, 0.0);
        int    NeighborCount   = 0;

        const float PerceptionRadiusSq = In_PerceptionRadius * In_PerceptionRadius;
        const float SeparationRadiusSq = In_SeparationRadius * In_SeparationRadius;

#if GPU_SIMULATION

        int3 NumCells;
        GridIn.GetNumCells(NumCells.x, NumCells.y, NumCells.z);

        int MaxNeighborsCount;
        GridIn.MaxNeighborsPerCell(MaxNeighborsCount);

        // 与 BoidsFillGrid 逐字相同的索引公式（自洽即可，不需要网格的矩阵接口）
        const float3 CellSize = In_GridSize / float3(NumCells.x, NumCells.y, NumCells.z);
        const float3 GridMin  = In_GridCenter - In_GridSize * 0.5;
        const float3 UnitPos  = (In_Position - GridMin) / CellSize;
        const int3   CellIndex = int3(floor(UnitPos));

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
                            GridIn.NeighborGridIndexToLinear(CellIndexToCheck.x, CellIndexToCheck.y, CellIndexToCheck.z, i, NeighborLinearIndex);

                            int NeighborIndex;
                            GridIn.GetParticleNeighbor(NeighborLinearIndex, NeighborIndex);

                            if(NeighborIndex == -1 || NeighborIndex == In_ExecutionIndex) continue;

                            float3 NeighborPosition;
                            bool bNeighborPositionValid;
                            AttributeReader.GetPositionByIndex<Attribute="Position">(NeighborIndex, bNeighborPositionValid, NeighborPosition);

                            if(!bNeighborPositionValid) continue;

                            const float3 DeltaPosition = NeighborPosition - In_Position;
                            const float SqrdDistance = dot(DeltaPosition, DeltaPosition);

                            // 感知球（要求 CellSize ≥ PerceptionRadius，否则 27 格盖不满）
                            if(SqrdDistance >= PerceptionRadiusSq) continue;

                            NeighborCount++;
                            CohesionAccum += NeighborPosition;

                            float3 NeighborVelocity;
                            bool bNeighborVelocityValid;
                            AttributeReader.GetVectorByIndex<Attribute="Velocity">(NeighborIndex, bNeighborVelocityValid, NeighborVelocity);

                            if(bNeighborVelocityValid) AlignmentAccum += NeighborVelocity;

                            // 分离：近邻按线性衰减反向推开（只累加方向，最后再归一化）
                            if(SqrdDistance > 0.0001 && SqrdDistance < SeparationRadiusSq)
                            {
                                const float Distance = sqrt(SqrdDistance);
                                SeparationAccum += (-DeltaPosition / Distance) * (1.0 - Distance / In_SeparationRadius);
                            }
                        }
                    }
                }
            }
        }

#endif

        // ==== 三规则合成转向方向 ====
        // 三个方向都先归一化再加权，否则"最近的邻居"会独占权重，群体会碎成小团
        float3 SteerDirection = float3(0.0, 0.0, 0.0);

        if(NeighborCount > 0)
        {
            const float InvCount = 1.0 / NeighborCount;
            const float3 FlockCenter = CohesionAccum * InvCount;
            const float3 AverageVelocity = AlignmentAccum * InvCount;

            const float3 ToCenter = FlockCenter - In_Position;

            const float3 SeparationDir = SeparationAccum / max(length(SeparationAccum), 0.0001);
            const float3 AlignmentDir  = AverageVelocity / max(length(AverageVelocity), 0.0001);
            const float3 CohesionDir   = ToCenter / max(length(ToCenter), 0.0001);

            SteerDirection = SeparationDir * In_WeightSeparation
                           + AlignmentDir  * In_WeightAlignment
                           + CohesionDir   * In_WeightCohesion;
        }

        // Reynolds steer：期望速度 − 当前速度，按最大转向力截断
        const float3 DesiredVelocity = SteerDirection / max(length(SteerDirection), 0.0001) * In_MaxSpeed;
        float3 SteeringForce = DesiredVelocity - In_Velocity;
        const float SteerMagnitude = length(SteeringForce);

        if(SteerMagnitude > In_MaxForce)
        {
            SteeringForce = SteeringForce / SteerMagnitude * In_MaxForce;
        }

        float3 NewVelocity = In_Velocity + SteeringForce * In_DeltaTime;

        // 边界回收：越过活动半径就往回拉，避免飞出网格后失去邻居（出格即失联）
        const float3 ToAnchor = In_AnchorPosition - In_Position;
        const float DistanceToAnchor = length(ToAnchor);

        if(DistanceToAnchor > In_BoundaryRadius)
        {
            const float BoundaryScale = (DistanceToAnchor - In_BoundaryRadius) / max(In_BoundaryRadius, 0.0001);
            NewVelocity += (ToAnchor / DistanceToAnchor) * In_BoundaryForce * BoundaryScale * In_DeltaTime;
        }

        // 速度带：引擎 SolveForcesAndVelocity 的 SpeedLimit(500) 只是最后一道闸
        const float Speed = length(NewVelocity);

        if(Speed > 0.0001)
        {
            const float ClampedSpeed = min(max(Speed, In_MinSpeed), In_MaxSpeed);
            NewVelocity = NewVelocity / Speed * ClampedSpeed;
        }

        // ==== 写回 ====
        Particles.Velocity = NewVelocity;

        // 自检用着色（debug 专用，直接用 RGB 纯色）：在群里 = 绿 G，边缘 = 蓝 B，失联 = 红 R
        //（通常是出格或网格覆盖不足）。阈值只写在下面的代码里，注释不写具体数字，避免与代码漂移。
        Particles.Color = NeighborCount >= 4
            ? float4(0.0, 1.0, 0.0, 1.0)
            : (NeighborCount > 0 ? float4(0.0, 0.0, 1.0, 1.0) : float4(1.0, 0.0, 0.0, 1.0));
    }
}
