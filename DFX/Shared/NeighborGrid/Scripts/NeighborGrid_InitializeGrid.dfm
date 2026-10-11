// 共享网格基础设施（2026-10-10 自 NeighborGrid3D/NS/Scripts/ 迁入 Shared/NeighborGrid/）：
//   Boids 等外部效果按路径复用网格设施，不再依赖 NeighborGrid3D 演示目录；
//   原资产 NS_NeighborGrid3D_InitializeGrid 随之退役（旧 .dfm 源已删除，旧构建产物为孤儿，可在编辑器中清理）。
// 规范共享模块（2026-10 合并）：基础版 / Out / Show / FX_Syst 四个系统共用本资产。
//   四份 *_InitializeGrid 原本 Body 逐字节相同，仅 Name 不同，故合并为这一份；
//   Out/Show/FX_Syst 的同名脚本资产已无源文件、成为孤儿（见 NeighborGrid3D/README.md）。
// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_InitializeGrid.NS_NeighborGrid3D_InitializeGrid 的图。
// 原图是薄封装：Module 输入 → NeighborGrid3D.SetNumCells（DI 成员函数，CPU-only）→ Success 输出。
// 用途: 配置 NeighborGrid3D 的三轴单元数与每格邻居上限（.dfs 在 SystemSpawn 调用）。
// 注意: Name= 与资产路径一致，构建本文件原位接管 /Game/Shared/NeighborGrid/Scripts/NeighborGrid_InitializeGrid。
Module(Name="Shared/NeighborGrid/Scripts/NeighborGrid_InitializeGrid", Root="Game")
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
