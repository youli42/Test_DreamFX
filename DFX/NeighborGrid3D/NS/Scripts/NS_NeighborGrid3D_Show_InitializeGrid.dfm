// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Show_InitializeGrid.NS_NeighborGrid3D_Show_InitializeGrid 的图。
// 原图是薄封装：Module 输入 → NeighborGrid3D.SetNumCells（DI 成员函数，CPU-only）→ Success 输出。
// 用途: 配置 NeighborGrid3D 的三轴单元数与每格邻居上限（.dfs 在 SystemSpawn 调用）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Show_InitializeGrid", Root="Game")
{
    Settings = {
        Usage       = [SystemSpawn, ParticleUpdate];
        Category    = "NeighborGrid3D";
        Description = "设置网格三轴单元数（NumCellsX/Y/Z）与每格最大邻居数（MaxNeighborsPerCell）。";
    }

    Inputs = {
        DI<NeighborGrid3D> MyNeighborGrid3DIn;
        int MaxNeighborsPerCell = 24;
        int NumCellsX = 10;
        int NumCellsY = 10;
        int NumCellsZ = 10;
    }

    Body = {
        bool Success;
        MyNeighborGrid3DIn.SetNumCells(NumCellsX, NumCellsY, NumCellsZ, MaxNeighborsPerCell, Success);
    }
}
