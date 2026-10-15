// Boids 自包含模块 3/3 —— Stage Boids（群体行为）。
// 27 邻格查询（与 BoidsFillGrid 共用同一套世界→单元索引公式）+ 分离/对齐/聚合三规则，
// 以 Reynolds steer 的形式写回 Particles.Velocity；边界回收是第 4 条 steer 规则（软阈值，
// 见 Body 里的边界压力段），与三规则共用同一个 MaxForce 预算。
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
        Description = "27 邻格查询 + 分离/对齐/聚合三规则 + 软边界回收（第 4 条 steer 规则），按 Reynolds steer 写回 Particles.Velocity。";
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
        float   In_ShellStart        = 0.75;
        float   In_ShellSpeedDamping = 0.6;
        float   In_InnerBias         = 0.08;
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

        // ==== 边界压力：ShellStart×BoundaryRadius 起连续上升，到 BoundaryRadius 处满格 ====
        // 软阈值：旧版是硬阈值 + 截断之后的事后冲量，于是 r = BoundaryRadius 成为一个零力面
        //（外面有回拉力、里面什么都不拉），群体贴上去沿切向环流就不再回到中间空间。
        // 这里把压力做成 C1 连续（smoothstep 形式），并把它并进下面的 steer 合成。
        const float3 ToAnchor = In_AnchorPosition - In_Position;
        const float DistanceToAnchor = max(length(ToAnchor), 0.0001);
        const float3 RadialDir = ToAnchor / DistanceToAnchor;

        const float ShellRadius = In_BoundaryRadius * clamp(In_ShellStart, 0.05, 0.95);
        const float ShellSpan = max(In_BoundaryRadius - ShellRadius, 0.0001);
        const float ShellT = clamp((DistanceToAnchor - ShellRadius) / ShellSpan, 0.0, 1.0);
        const float BoundaryPressure = ShellT * ShellT * (3.0 - 2.0 * ShellT);

        // ==== 期望方向：三规则方向（+ 内部弱向心偏置）与径向按压力混合 ====
        const float SteerLength = length(SteerDirection);
        const float3 FlockDir = (SteerLength > 0.0001) ? SteerDirection / SteerLength : float3(0.0, 0.0, 0.0);

        // 偏置随半径平方增长、在锚点处恒为 0：只把群体整体往内偏，不会把群拉成一个点。
        // 只在有三规则意图时叠加：否则落单个体（NeighborCount = 0）会把这份很小的偏置
        // 当成满速期望速度，被直接吸向锚点。
        const float InnerT = clamp(DistanceToAnchor / max(ShellRadius, 0.0001), 0.0, 1.0);
        const float3 InnerDir = (SteerLength > 0.0001) ? RadialDir * (In_InnerBias * InnerT * InnerT) : float3(0.0, 0.0, 0.0);

        const float3 BlendedDir = FlockDir * (1.0 - BoundaryPressure) + RadialDir * BoundaryPressure + InnerDir;
        const float BlendedLength = length(BlendedDir);
        const float CurrentSpeed = length(In_Velocity);

        // 兜底取当前航向而不是一律取径向：内部且无邻居时保持滑行，而不是掉头冲向锚点
        const float3 DesiredDir = (BlendedLength > 0.0001)
            ? BlendedDir / BlendedLength
            : ((CurrentSpeed > 0.0001) ? In_Velocity / CurrentSpeed : RadialDir);

        // ==== 速度包络：壳上降速（打破切向环流最直接的手段），并放开下限让个体能减速掉头 ====
        const float SpeedCap = In_MaxSpeed * (1.0 - clamp(In_ShellSpeedDamping, 0.0, 0.9) * BoundaryPressure);
        const float SpeedFloor = min(In_MinSpeed * (1.0 - BoundaryPressure), SpeedCap);

        // ==== 边界作为第 4 条 steer 规则：与三规则共用同一个 MaxForce 预算，而不是事后加冲量 ====
        // 压力→1 时 SpeedCap 收窄到 MaxSpeed 的一部分，方向被径向接管；BoundaryForce 大于 MaxForce
        // 时边界还能独自吃满预算，因此角速度不再被"事后归一化"稀释成转向权限的百分之几。
        float3 SteeringForce = DesiredDir * SpeedCap - In_Velocity + RadialDir * (In_BoundaryForce * BoundaryPressure);
        const float SteerMagnitude = length(SteeringForce);

        if(SteerMagnitude > In_MaxForce)
        {
            SteeringForce = SteeringForce / SteerMagnitude * In_MaxForce;
        }

        float3 NewVelocity = In_Velocity + SteeringForce * In_DeltaTime;

        // 速度带：与转向共用同一帧的包络；引擎 SolveForcesAndVelocity 的 SpeedLimit(500) 只是最后一道闸
        const float Speed = length(NewVelocity);

        if(Speed > 0.0001)
        {
            const float ClampedSpeed = min(max(Speed, SpeedFloor), SpeedCap);
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
