// 整理自 /Game/NeighborGrid3D/NS/Scripts/NS_WhatsNeighborGrid_IntByTIme.NS_WhatsNeighborGrid_IntByTIme 的图（CustomHLSL）。
// 原 HLSL: float continuousValue = IN_Age * IN_Speed; int currentStep = (int)floor(continuousValue);
//          OUT_result = currentStep % (IN_MaxNum + 1);
// 用途: 随时间产生阶梯递增的整数索引（对 MaxNum+1 取模循环），.dfs 用它轮换 Leader 粒子。
// 注意: 原脚本只暴露 Speed/MaxNum 两个输入，Age 由图内参数映射节点内部读取（不暴露）；
//       此处用 Engine.Time 等价替代 —— 与原实现仅差绝对相位，循环行为一致（构建后需模拟验证）。
// 注意: Name= 与既有脚本资产路径一致，构建本文件会原位接管该资产。
DynamicInput(Name="NeighborGrid3D/NS/Scripts/NS_WhatsNeighborGrid_IntByTIme", Root="Game")
{
    Settings = {
        Usage       = DynamicInput;
        Output      = int;
        Category    = "NeighborGrid3D";
        Description = "随时间阶梯递增并循环的整数索引：floor(Time * Speed) 对 (MaxNum + 1) 取模。";
    }

    Inputs = {
        float Speed  = 1.0;
        int   MaxNum = 10;
    }

    Body = {
        return (int)fmod(floor(Engine.Time * Speed), (float)(MaxNum + 1));
    }
}
