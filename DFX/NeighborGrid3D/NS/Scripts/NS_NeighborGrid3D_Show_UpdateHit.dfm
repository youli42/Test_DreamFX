// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Show_UpdateHit.NS_NeighborGrid3D_Show_UpdateHit 的图（与 NS 版同构）。
// 重建公式（算子指纹唯一自洽解）: Hit = saturate( remap(CollisionValue, RangeMin, RangeMax, CollisionMin, CollisionMax) + Hit * (1 - FadeSpeed * DeltaTime) )
// 用途: 碰撞闪光 —— 新命中按碰撞强度抬升 Particles.Hit，旧值按 FadeSpeed 线性衰减。
// 注意: 构建本文件会原位接管脚本资产；公式为算子指纹重建，需 PIE 目视确认闪光/衰减节奏。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_Show_UpdateHit", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "NeighborGrid3D";
        Description = "碰撞闪光：新命中按强度抬升 Hit，旧值按 FadeSpeed 衰减（写入 Particles.Hit）。";
    }

    Inputs = {
        float CollisionValue    = 0.0;
        float CollisionRangeMin = 10.0;
        float CollisionRangeMax = 100.0;
        float CollisionMin      = 0.0;
        float CollisionMax      = 1.0;
        float FadeSpeed         = 2.0;
    }

    Body = {
        float NewHit = CollisionMin + saturate((CollisionValue - CollisionRangeMin) / (CollisionRangeMax - CollisionRangeMin)) * (CollisionMax - CollisionMin);
        float Particles.Hit = saturate(NewHit + Particles.Hit * (1.0 - FadeSpeed * Engine.DeltaTime));
    }
}
