// Boids 自包含模块 2/3 —— Stage FillGrid。
// 把每个粒子的世界坐标换算成单元索引并登记进 NeighborGrid3D。
// 关键: 不使用网格自带的矩阵接口（NeighborGrid3D.SimulationToUnit / UnitToIndex），
//       也不引用 NeighborGrid3D 工具箱的 Grid3D_CreateUnitToWorldTransform 图模块；
//       单元索引由本模块自行计算，查询侧（BoidsQueryGrid）用**同一套公式**，两者自洽即可
//       （±半格的整体偏移不影响 27 邻格覆盖：|Δworld| ≤ CellSize ⇒ |Δindex| ≤ 1）。
// 网格约定: 网格中心 = In_GridCenter，世界尺寸 = In_GridSize，CellSize = In_GridSize / NumCells。
// 注意: Name= 指向 /Game/Boids/Niagara/BoidsFillGrid，构建本文件会生成/接管该资产。
Module(Name="Boids/Niagara/BoidsFillGrid", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "Boids";
        Description = "按世界坐标把粒子登记进邻域网格单元（自行换算索引，不依赖网格矩阵接口）。";
    }

    Inputs = {
        DI<NeighborGrid3D> GridIn;
        Vector In_GridCenter = (0.0, 0.0, 0.0);
        Vector In_GridSize   = (4000.0, 4000.0, 4000.0);
    }

    Body = {
#if GPU_SIMULATION

        int3 NumCells;
        GridIn.GetNumCells(NumCells.x, NumCells.y, NumCells.z);

        // 单元尺寸与网格最小角（世界空间），与 BoidsQueryGrid 必须逐字一致
        const float3 CellSize = In_GridSize / float3(NumCells.x, NumCells.y, NumCells.z);
        const float3 GridMin  = In_GridCenter - In_GridSize * 0.5;

        const float3 UnitPos   = (Particles.Position - GridMin) / CellSize;
        const int3   CellIndex = int3(floor(UnitPos));

        // 出格粒子不登记：它在查询侧同样查不到邻居，靠边界回收力拉回来
        if(CellIndex.x >= 0 && CellIndex.x < NumCells.x &&
           CellIndex.y >= 0 && CellIndex.y < NumCells.y &&
           CellIndex.z >= 0 && CellIndex.z < NumCells.z)
        {
            bool bSuccess;
            GridIn.AddParticle(CellIndex.x, CellIndex.y, CellIndex.z, ExecIndex(), bSuccess);
        }

#endif
    }
}
