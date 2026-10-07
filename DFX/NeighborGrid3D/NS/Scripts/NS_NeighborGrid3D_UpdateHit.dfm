// 规范共享模块（2026-10 合并）：基础版 / Out / Show 三个系统共用本资产，内容以 Out 版为准。
// 原图公式（2026-10-07 按原资产反编译逐句核对）：
//   NewHit = remap+clamp(CollisionValue, RangeMin..RangeMax -> CollisionMin..CollisionMax)
//   Hit    = (Hit + NewHit) * (1 - clamp(FadeSpeed * DeltaTime, 0, 1))
//   原图对累加值**不做** saturate（连续命中可累积 >1），只 clamp 衰减因子。
// 用途: 碰撞闪光 —— 抬升/衰减 Particles.Hit，并把它写入 Particles.DynamicMaterialParameter 供材质发光。
// 合并说明: 基础版原以 In_Hit 输入携带旧 Hit（调用点传 In_Hit = Particles.Hit），与 Out/Show 的
//   "带类型声明自读"（DFX3046 规则）数学等价；统一后采用自读形态，调用点不再传 In_Hit。
//   FadeSpeed 由各调用点显式传参：Out / Show = 20，基础版 = 2（原资产调参，保留）。
// 注意: 构建本文件会原位接管脚本资产。
// 注意: `Type Particles.X = ...;` 声明**正上方不能是注释行** —— DFX 的类型剥离只在语句起始生效
//       （`;`/`{`/`}` 之后），注释会把该状态清掉，类型名会原样漏进 HLSL（`Vector4 ...` → 未知类型报错）。
//
// 修复(2026-10-07): 迁移时漏掉了发光载体 —— 原图写完 Hit 之后还有
//   Particles.DynamicMaterialParameter = float4(Hit, Hit, Hit, Hit);
// 材质 M_FX_Spheres 的 Dynamic Parameter（"传递撞击事件"）× Glow 驱动 EmissiveColor，
// 少了这一句，Hit 算得再对材质端也恒为 0 → 完全没有碰撞发光。
// 同时把衰减公式对齐原图：只 clamp 衰减因子，不再对累加值 saturate。
Module(Name="NeighborGrid3D/NS/Scripts/NS_NeighborGrid3D_UpdateHit", Root="Game")
{
    Settings = {
        Usage       = ParticleUpdate;
        Category    = "NeighborGrid3D";
        Description = "碰撞闪光：新命中按强度抬升 Hit、按 FadeSpeed 衰减，并写入 DynamicMaterialParameter 供材质自发光。";
    }

    Inputs = {
        float CollisionValue    = 0.0;
        float CollisionRangeMin = 10.0;
        float CollisionRangeMax = 100.0;
        float CollisionMin      = 0.0;
        float CollisionMax      = 1.0;
        float FadeSpeed         = 5.0;
    }

    Body = {
        float NewHit = CollisionMin + saturate((CollisionValue - CollisionRangeMin) / (CollisionRangeMax - CollisionRangeMin)) * (CollisionMax - CollisionMin);
        float Fade   = 1.0 - saturate(FadeSpeed * Engine.DeltaTime);
        float Particles.Hit = (Particles.Hit + NewHit) * Fade;
        Vector4 Particles.DynamicMaterialParameter = (Particles.Hit, Particles.Hit, Particles.Hit, Particles.Hit);
        // 上一句是发光载体（原图 UpdateHit 收尾的第二句）：材质 M_FX_Spheres 的
        // Dynamic Parameter（"传递撞击事件"）× Glow 驱动 EmissiveColor —— 迁移时整条丢失，故完全没有闪光。
        // 注意这两句之间不能插注释（见文件头），否则 `Vector4` 会漏进 HLSL。
    }
}
