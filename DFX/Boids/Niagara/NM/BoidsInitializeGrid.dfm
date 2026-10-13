// Boids 自包含模块 1/3 —— 网格分辨率设置。
// 本目录（DFX/Boids/Niagara/NM，模块源）不引用 NeighborGrid3D 工具箱的任何 .dfs/.dfm/NM 资产：
// 这三个 .dfm 与上一级的 NS_Boids.dfs 自成一套，可单独构建。
// 用途: SystemSpawn 里调用 NeighborGrid3D 的 SetNumCells 配置三轴单元数与每格邻居上限。
// 注意: Name= 指向 /Game/Boids/Niagara/NM/BoidsInitializeGrid，构建本文件会生成/接管该资产。
Module(Name="Boids/Niagara/NM/BoidsInitializeGrid", Root="Game")
{
    Settings = {
        Usage       = SystemSpawn;
        Category    = "Boids";
        Description = "配置邻域网格三轴单元数（NumCellsX/Y/Z）与每格最大邻居数（MaxNeighborsPerCell）。";
    }

    Inputs = {
        DI<NeighborGrid3D> GridIn;
        int MaxNeighborsPerCell = 20;
        int NumCellsX = 40;
        int NumCellsY = 40;
        int NumCellsZ = 40;
    }

    Body = {
        bool Success;
        GridIn.SetNumCells(NumCellsX, NumCellsY, NumCellsZ, MaxNeighborsPerCell, Success);
    }
}
