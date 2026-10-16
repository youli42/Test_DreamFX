// Boids 自包含模块 3/3 —— Stage Boids（群体行为）。
// 27 邻格查询（与 BoidsFillGrid 共用同一套世界→单元索引公式）+ 分离/对齐/聚合三规则，
// 以 Reynolds steer 的形式写回 Particles.Velocity。
//
// 运行语义（重要）: Stage 在 ParticleUpdate 之后执行，SolveForcesAndVelocity 已经做完位置积分，
//   所以这里**只写 Velocity**，绝不写 Position（否则双重积分）。
// 边界形状: In_BoundaryMode 选 Sphere（In_AnchorPosition + In_BoundaryRadius）或
//   Box（In_GridCenter + In_GridSize，逐轴半边长 × In_BoundaryMargin）。两种形状共用同一套逐轴压力公式，
//   不生效的参数已在各自的 Description 里写明；换成 Box 后 p = 1 的面向网格面靠近，`In_BoundaryMargin`
//   是唯一的越冲余量，别把它取到 1。
// 转向卫生（issue #7）: 速度方向每帧最多转 In_MaxTurnRate × dt（角速度上限，边界回收力不受限）；
//   没有邻居时保持当前速度直飞（不再刹车到 In_MinSpeed）；分离累加量趋近 0 时按死区淡出（不让噪声
//   变成单位方向）；感知球边界用 smoothstep 连续渐隐而不是硬切；dt 夹到 1/30 避免掉帧时朝向抽动。
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
        float   In_MaxTurnRate      = 1.2;
        Vector  In_AnchorPosition   = (0.0, 0.0, 0.0);
        // 边界形状: 0 = Sphere（In_BoundaryRadius 生效）, 1 = Box（In_GridCenter/In_GridSize 生效）
        int     In_BoundaryMode     = 0;
        float   In_BoundaryRadius   = 1500.0;
        float   In_BoundaryMargin   = 0.9;
        float   In_BoxShellStart    = 0.75;
        float   In_BoundaryForce    = 1200.0;
    }

    Body = {
        // ==== 捕获写回前的原始值 ====
        float3 In_Position       = Particles.Position;
        float3 In_Velocity       = Particles.Velocity;
        // dt 夹到 1/30: 掉帧（一次卡顿 100ms）时朝向只按 1/30 转，不会被一次性抽十几度。
        // 位置积分仍在引擎侧用未夹的 dt，所以卡顿时位置会跳一下、朝向不会抽——这是有意的取舍。
        float  In_DeltaTime      = min(Engine.DeltaTime, 0.0333);
        int    In_ExecutionIndex = ExecIndex();

        // 累加器必须在 #if GPU_SIMULATION 之前清零：清零语句若只存在于 GPU 分支，
        // CPU 路径整段被编译掉，等于每帧把未定义值加到属性上（本项目记录过的历史 bug）
        float3 SeparationAccum = float3(0.0, 0.0, 0.0);
        float3 AlignmentAccum  = float3(0.0, 0.0, 0.0);
        float3 CohesionAccum   = float3(0.0, 0.0, 0.0);
        int    NeighborCount   = 0;     // 整数计数: 只用于自检着色的图例（>= 4 判绿）
        float  NeighborWeight  = 0.0;   // 感知权重和: 三规则求平均用（软感知边界的权重）

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

                            // 感知球（要求 CellSize ≥ PerceptionRadius，否则 27 格盖不满）。
                            // 边界不再硬切: 邻居穿过感知面时权重从 0 连续长起来，不会在那一帧产生
                            // "贡献从 0 突变成 1/N" 的方向跳变（issue #7 分析 1 第一条）。
                            const float Distance = sqrt(SqrdDistance);
                            const float PerceptionWeight =
                                1.0 - smoothstep(In_PerceptionRadius * 0.75, In_PerceptionRadius, Distance);

                            if(PerceptionWeight <= 0.0) continue;

                            NeighborCount++;
                            NeighborWeight += PerceptionWeight;
                            CohesionAccum += NeighborPosition * PerceptionWeight;

                            float3 NeighborVelocity;
                            bool bNeighborVelocityValid;
                            AttributeReader.GetVectorByIndex<Attribute="Velocity">(NeighborIndex, bNeighborVelocityValid, NeighborVelocity);

                            if(bNeighborVelocityValid) AlignmentAccum += NeighborVelocity * PerceptionWeight;

                            // 分离：近邻按线性衰减反向推开（只累加方向，最后再归一化）
                            if(SqrdDistance > 0.0001 && SqrdDistance < SeparationRadiusSq)
                            {
                                SeparationAccum += (-DeltaPosition / Distance)
                                                 * (1.0 - Distance / In_SeparationRadius)
                                                 * PerceptionWeight;
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

        // 没有邻居 = 保持当前速度直飞（issue #7）: 旧实现把期望速度算成 0，等于全速刹车到
        // In_MinSpeed，速度在 80↔300 之间脉动，而箭头长度正比于速度 → 屏幕上就是箭头忽长忽短
        float3 NewVelocity = In_Velocity;

        if(NeighborWeight > 0.0001)
        {
            const float InvCount = 1.0 / NeighborWeight;
            const float3 FlockCenter = CohesionAccum * InvCount;
            const float3 AverageVelocity = AlignmentAccum * InvCount;

            const float3 ToCenter = FlockCenter - In_Position;

            // 分离累加量趋近 0 时（近邻很少、或近邻近似对称相消），它的方向由浮点噪声与邻居
            // 顺序决定，而它的权重恰是三项里最大的 → 用"一个邻居满推力的 5%"做死区、0.05→0.5
            // 线性淡入，让它不可信时闭嘴；淡入本身连续，死区边界不会变成新的跳变
            const float SepLen = length(SeparationAccum);
            const float3 SeparationDir = SeparationAccum * (saturate((SepLen - 0.05) / 0.45) / max(SepLen, 0.0001));
            const float3 AlignmentDir  = AverageVelocity / max(length(AverageVelocity), 0.0001);
            const float3 CohesionDir   = ToCenter / max(length(ToCenter), 0.0001);

            SteerDirection = SeparationDir * In_WeightSeparation
                           + AlignmentDir  * In_WeightAlignment
                           + CohesionDir   * In_WeightCohesion;

            // Reynolds steer：期望速度 − 当前速度
            const float3 DesiredVelocity = SteerDirection / max(length(SteerDirection), 0.0001) * In_MaxSpeed;
            float3 SteeringForce = DesiredVelocity - In_Velocity;
            const float SteerMagnitude = length(SteeringForce);

            // 注意: 默认参数下这段截断不会触发（|期望−当前| ≤ 2×MaxSpeed = 600 < MaxForce = 800），
            // 真正限制转向的是下面的角速度上限。保留它是为了不动调用点，别再指望它治抖动。
            if(SteerMagnitude > In_MaxForce)
            {
                SteeringForce = SteeringForce / SteerMagnitude * In_MaxForce;
            }

            NewVelocity = In_Velocity + SteeringForce * In_DeltaTime;
        }

        // ==== 角速度上限（issue #7 核心）：这一帧最多把速度方向朝目标方向转 In_MaxTurnRate × dt ====
        // 抖动噪声的特点是方向逐帧左右横跳；把"直接跳到新方向"换成"朝新方向挪一小步"之后，
        // 横跳的 ± 会互相抵消 —— 这个闸门本身就是积分器/低通，既限幅又把随机性平均掉。
        // 只限制三规则转向这一步: 边界回收力不受限，否则 #2 的回中会被拖慢。
        const float SpeedFrom = length(In_Velocity);
        const float SpeedTo   = length(NewVelocity);

        if(SpeedFrom > 0.0001 && SpeedTo > 0.0001)
        {
            const float3 DirFrom = In_Velocity / SpeedFrom;
            const float3 DirTo   = NewVelocity / SpeedTo;
            const float  TurnAngle = acos(clamp(dot(DirFrom, DirTo), -1.0, 1.0));
            const float  MaxStep   = min(In_MaxTurnRate * In_DeltaTime, 3.141592);

            if(TurnAngle > MaxStep)
            {
                // 绕 DirFrom × DirTo 转 MaxStep = 朝目标方向转、但只转这么多。
                // 反平行（正好 180°）时叉乘为 0，用确定轴兜底，避免 0/0 出 NaN 让鸟乱飞。
                float3 TurnAxis = cross(DirFrom, float3(0.0, 0.0, 1.0));
                if(length(TurnAxis) < 0.0001) { TurnAxis = cross(DirFrom, float3(0.0, 1.0, 0.0)); }

                TurnAxis = normalize(TurnAxis);
                NewVelocity = (DirFrom * cos(MaxStep) + TurnAxis * sin(MaxStep)) * SpeedTo;
            }
        }

        // ==== 边界回收：越过活动边界就往回拉，避免飞出网格后失去邻居（出格即失联）====
        // 两种形状走同一条逐轴公式：把位置换算成轴对齐单元坐标 u_i（|u_i| = 1 就是边界，
        // 边界中心 = 锚点/网格中心，朝向 = 世界轴），再按 |u_i| 越过 In_BoxShellStart（边界半径的比例）的多少取压力 p_i。
        //   Box : u_i = (Position_i - GridCenter_i) / (GridSize_i * 0.5 * BoundaryMargin)
        //         → 逐轴半边长取自 In_GridSize，天然与 NeighborGrid 同尺寸、同中心、同朝向；
        //           BoundaryMargin < 1 让边界面内缩于网格面，越冲余量留在这里。
        //   Sphere: u_i = 各轴相同的 (Position - Anchor) / BoundaryRadius
        //         → 三轴 p_i 相同，回正力自然退化成纯径向（逐轴公式不必分支）。
        //         注意与"硬阈值"旧实现的差别: 旧实现只在 r > BoundaryRadius 时给力，而 r = BoundaryRadius
        //         恰好是零力面（切向速度不受阻，是贴壳环流的成因）；这里从 In_BoxShellStart × 半径
        //         起就连续加压，使力在边界内侧、边界处满格且向外单调 —— 这是有意的行为改动。
        // 方向: 取最近的那个面（|u_i| 最大的轴），回正力就沿该轴的面法线 —— 长方面上方向恒定，
        //       不会在墙面内部产生切向分量。不要把各轴 -sign(u_i) * p_i 归一化后当方向用：
        //       在靠近某个面、但另一轴的压力更大时，那个"混合方向"会几乎完全垂直于该面
        //       （即沿墙面切向），正是要避免的贴壁环流。
        // 压力取 max(p_i) 而不是 length: 角上压力与面上相同，避免角落受力叠加造成堆积。
        // 生效语义: BoundaryMode = 0 时 In_BoundaryRadius 定义球半径（In_BoundaryMargin 不生效）；
        //           BoundaryMode = 1 时 In_GridSize × In_BoundaryMargin 定义长方体半边长（In_BoundaryRadius 不生效）。
        const float3 BoundaryCenter = (In_BoundaryMode == 0) ? In_AnchorPosition : In_GridCenter;
        const float3 BoundaryExtent = (In_BoundaryMode == 0)
            ? float3(In_BoundaryRadius, In_BoundaryRadius, In_BoundaryRadius)
            : In_GridSize * 0.5 * clamp(In_BoundaryMargin, 0.1, 1.0);

        const float3 BoundaryOffset = In_Position - BoundaryCenter;
        const float3 UnitBoundary   = BoundaryOffset / max(BoundaryExtent, float3(0.0001, 0.0001, 0.0001));
        const float3 UnitAbs        = abs(UnitBoundary);

        const float ShellStart = clamp(In_BoxShellStart, 0.05, 0.95);
        const float ShellSpan  = max(1.0 - ShellStart, 0.0001);
        const float3 AxisT = clamp((UnitAbs - float3(ShellStart, ShellStart, ShellStart)) / ShellSpan,
                                   float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
        // 逐轴压力：软阈值从 ShellStart 升到 1 时 0 → 1，平方是为了让吸力更晚、更硬
        const float3 AxisPressure = AxisT * AxisT;

        const float BoundaryPressure = max(AxisPressure.x, max(AxisPressure.y, AxisPressure.z));

        // 越冲量沿各轴相加，而不是只沿一个方向: Box 的角上是两个方向同时压
        const float3 Overshoot = max(UnitAbs - float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0));

        if(BoundaryPressure > 0.0001 && dot(BoundaryOffset, BoundaryOffset) > 0.0001)
        {
            const float3 DominantAxis = (UnitAbs.x >= UnitAbs.y && UnitAbs.x >= UnitAbs.z)
                ? float3(1.0, 0.0, 0.0)
                : ((UnitAbs.y >= UnitAbs.z) ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0));

            // -sign(BoundaryOffset) 是"从最近的那面往回指"的符号: 在面内侧指向内，在面外侧也指向内。
            // Perpendicular 分量（轴之外的另外两轴）由 DominantAxis 置 0，所以面上方向恒为面法线。
            const float3 BoundaryDir = -sign(BoundaryOffset) * DominantAxis;

            NewVelocity += (BoundaryDir * BoundaryPressure + (-sign(BoundaryOffset)) * Overshoot)
                         * In_BoundaryForce * In_DeltaTime;
        }

        // 速度带：引擎 SolveForcesAndVelocity 的 SpeedLimit(500) 只是最后一道闸。
        // 这里是硬夹而非软弹簧（issue #7 分析 4 那条），但配合"没有邻居不刹车"之后，有邻居时
        // 期望速度恒为 In_MaxSpeed、稳态速度被夹在带上沿，箭头长度不再脉动，所以本单不动它。
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
