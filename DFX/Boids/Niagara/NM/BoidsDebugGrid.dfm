// Boids 自包含模块 4/4 —— 调试用网格可视化（跑在 debug 发射器的 ParticleUpdate）。
// 每个网格单元一个粒子：按单元中心摆放，缩放大 = 占用；颜色 绿 = 正常 / 红 = 超过每格登记上限 / 暗蓝 = 空格，
// 并把**该格内的粒子数量**（尝试登记数）写进 Particles.DynamicMaterialParameter，供文字材质打印成数字。
//
// 数量显示链路（材质侧由 DShader/Boids/Material/M_BoidsCellCount.dss 提供）：
//   本模块 → Particles.DynamicMaterialParameter.x = 是否显示（数量 > 0）
//           Particles.DynamicMaterialParameter.y = 数量本身（材质打印成数字）
//           Particles.Color                       = 状态色（绿 = 正常 / 红 = 超过每格登记上限 / 暗 = 空格）
//   M_BoidsCellCount 于是把**数字也画成状态色**：字形来自引擎材质函数 DebugScalarValues
//   （Number = .y，最多 2 位），颜色取 Particles.Color，透明度 = 字形遮罩 × .x。
//   （NeighborGrid3D 演示的 M_Debug_CellText 把数字颜色写死成红色，所以那边数字不随状态变。）
//
// 颜色（写 Particles.Color，格子材质 M_Debug_Cell 的 Emissive 直接取它）：
//   绿 = 该格正常占用；红 = 该格**超过每格登记上限**（有粒子没进网格，正在丢邻居）；暗蓝 = 空格。
//   注意"超过上限"只有用引擎的登记计数才判得出来：AddParticle 是先 InterlockedAdd 计数、
//   再判还有没有空槽，所以计数可以大于 MaxNeighborsPerCell —— 大于就是溢出，不是"刚好写满"。
//
// 计数方式：读引擎 DI 的每格登记计数 GetParticleNeighborCount(cellLinearIndex)
//   （cellLinearIndex 由基类函数 IndexToLinear 给出，与 AddParticle 写计数缓冲用的是同一个公式）。
//   它统计的是"尝试登记数"（可超过上限），与 NeighborGrid3D 演示里的数字语义一致；
//   网格每帧 ClearBeforeNonIterationStage（槽清 -1、计数清 0），所以这是该格本帧的值。
//
// 为什么不用现成的 /Game/NeighborGrid3D/NM/NM_InitializeDebugCells + NE_DebugGrid.dfe：
// 那套依赖 Grid3D_CreateUnitToWorldTransform 提供的 UnitToWorld 矩阵，而 Boids 的两个模块
// 是**自算**世界→单元索引的（GridMin = Center − Size/2），引入矩阵等于多一套口径，
// Owner 的旋转/缩放一旦不是单位值就会与实际查询错位。这里直接用同一套公式，天然一致。
// 注意: Name= 指向 /Game/Boids/Niagara/NM/BoidsDebugGrid，构建本文件会生成/接管该资产。
Module(Name="Boids/Niagara/NM/BoidsDebugGrid", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "Boids";
        Description = "调试网格：每格一个粒子摆到单元中心，写该格粒子数（文字）与占用状态（Scale / Color）。";
    }

    Inputs = {
        DI<NeighborGrid3D> GridIn;
        Vector  In_GridCenter    = (0.0, 0.0, 0.0);
        Vector  In_GridSize      = (4000.0, 4000.0, 4000.0);
        int     In_NumCellsX     = 20;
        int     In_NumCellsY     = 20;
        int     In_NumCellsZ     = 20;
        // 立方体边长 = 单元尺寸 × 该系数。占用格接近满格（看清单元大小），空格缩成小点（不挡视线）
        float   In_OccupiedScale = 0.95;
        float   In_EmptyScale    = 0.12;
        // 数字精灵尺寸 = 单元最大边 × 该系数（0 = 不显示数字）
        float   In_SpriteScale   = 0.6;
    }

    Body = {
        // ==== 每格一个粒子：把 ExecIndex 展开成 (i, j, k) ====
        // 用浮点 floor 而不是整数取模：GPU / CPU 两条编译路径都稳
        const float CellsX  = float(max(In_NumCellsX, 1));
        const float CellsY  = float(max(In_NumCellsY, 1));
        const float CellsZ  = float(max(In_NumCellsZ, 1));
        const float CellsXY = CellsX * CellsY;

        const int   FlatIndex = ExecIndex();
        const float Flat      = float(FlatIndex);
        const float CellZ     = floor(Flat / CellsXY);
        const float CellY     = floor((Flat - CellZ * CellsXY) / CellsX);
        const float CellX     = Flat - CellZ * CellsXY - CellY * CellsX;

        const int3 CellIndex = int3(floor(float3(CellX, CellY, CellZ)));

        // 计数结果必须在 #if GPU_SIMULATION 之前初始化：
        // 查询语句若只存在于 GPU 分支，CPU 路径整段被编译掉，会留下未定义值
        int  Count        = 0;
        int  MaxNeighbors = 0;

        // 单元尺寸与网格最小角，与 BoidsFillGrid / BoidsQueryGrid 必须逐字一致
        const float3 CellSize = In_GridSize / float3(CellsX, CellsY, CellsZ);

#if GPU_SIMULATION

        GridIn.MaxNeighborsPerCell(MaxNeighbors);

        if(CellIndex.x >= 0 && CellIndex.x < In_NumCellsX &&
           CellIndex.y >= 0 && CellIndex.y < In_NumCellsY &&
           CellIndex.z >= 0 && CellIndex.z < In_NumCellsZ)
        {
            // 该格"尝试登记"的粒子总数：AddParticle 先自增计数、再判还有没有空槽，
            // 所以返回值可以大于 MaxNeighborsPerCell —— 大于就说明有粒子没进网格（正在丢邻居）。
            int CellLinearIndex;
            GridIn.IndexToLinear(CellIndex.x, CellIndex.y, CellIndex.z, CellLinearIndex);

            GridIn.GetParticleNeighborCount(CellLinearIndex, Count);
        }

#endif

        const bool  bOccupied     = Count > 0;
        const bool  bOverCapacity = Count > MaxNeighbors;   // 超过每格登记上限 = 有粒子没进网格
        const float CountF        = float(Count);
        // 归一化数量：材质只拿它当可见性/亮度（>0 就显示文字），空格为 0 即不显示
        const float NormalizedCount = clamp(CountF / float(max(MaxNeighbors, 1)), 0.0, 1.0);

        // ==== 摆到单元中心：GridMin = Center − Size/2，中心 = GridMin + (ijk + 0.5)·CellSize ====
        const float3 GridMin    = In_GridCenter - In_GridSize * 0.5;
        const float3 CellCenter = GridMin + (float3(CellX, CellY, CellZ) + 0.5) * CellSize;

        // 几何尺寸与颜色/透明度都带占用信息：材质只要吃粒子色就能看出来；
        // 就算材质不吃粒子色，满格 vs 小点也已经把占用区分开了
        const float CellScale = bOccupied ? In_OccupiedScale : In_EmptyScale;
        const float CellSpan  = max(CellSize.x, max(CellSize.y, CellSize.z));

        Particles.Position   = CellCenter;
        Particles.Scale      = CellSize * CellScale;
        Particles.SpriteSize = float2(CellSpan * In_SpriteScale, CellSpan * In_SpriteScale);
        // 格子材质 M_Debug_Cell 是 Additive + Unlit，Emissive 直接取粒子色（还过一层
        // EyeAdaptationInverse，会把各通道按曝光放大到裁剪），所以三档都用**纯色相**：
        // 非本色的通道留 0，放大后只会饱和不会褪成白。
        //   绿 = 正常占用  红 = 超过每格登记上限（有粒子被丢）  暗蓝（低 alpha）= 空格
        Particles.Color      = bOverCapacity
            ? float4(1.00, 0.00, 0.00, 1.00)
            : (bOccupied ? float4(0.00, 1.00, 0.00, 1.00) : float4(0.00, 0.05, 0.15, 0.05));

        // 数量：.x = 归一化（材质当亮度/可见性），.y = 实际数量（材质打印的数字）
        Particles.DynamicMaterialParameter = float4(NormalizedCount, CountF, 0.0, 0.0);
    }
}
