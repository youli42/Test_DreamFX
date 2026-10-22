// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TODWeatherCurve.h"
#include "TODWeatherProfile.generated.h"

class UNiagaraSystem;

/** 内置回退天气与旧版迁移使用的项目粒子系统。 */
namespace TODWeatherDefaults
{
	inline constexpr TCHAR RainSystemPath[] = TEXT("/Game/TOD/Niagara/NS_TOD_Rain.NS_TOD_Rain");
	inline constexpr TCHAR SnowSystemPath[] = TEXT("/Game/TOD/Niagara/NS_TOD_Snow.NS_TOD_Snow");
}

/**
 * 旧版扁平预设（issue #10/#12）。仅保留作为曲线重构之前制作的
 * WP_*.uasset 文件的迁移输入：旧资产仍带有 `Data.Core` 标签，
 * 并由 UTODWeatherProfile::PostLoad 将其转换为下方的曲线分组。
 * 任何新内容都不应再制作此结构体。
 */
USTRUCT()
struct TEST_DREAMFX_API FTODWeatherPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name = NAME_None;

	UPROPERTY()
	float ScheduleWeight = 1.0f;

	UPROPERTY()
	float CloudCoverage = 0.2f;

	UPROPERTY()
	float FogDensity = 0.0003f;

	UPROPERTY()
	float WindStrength = 0.3f;

	UPROPERTY()
	float Wetness = 0.0f;

	UPROPERTY()
	float SnowAmount = 0.0f;

	UPROPERTY()
	float SunIntensityScale = 1.0f;

	UPROPERTY()
	float MoonIntensityScale = 1.0f;

	UPROPERTY()
	float SkyLightScale = 1.0f;

	UPROPERTY()
	bool bLightning = false;

	UPROPERTY()
	float RainIntensity = 0.0f;

	UPROPERTY()
	float SnowIntensity = 0.0f;
};

/** 天气身份 + 调度元数据（不随时间混合）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="天气名称"))
	FName Name = NAME_None;

	/** 被随机调度器选中的相对概率。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="调度权重", ClampMin="0.0"))
	float ScheduleWeight = 1.0f;

	/** 标记伴有雷电的天气（雷暴）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="伴有雷电"))
	bool bLightning = false;

	/** 地表累积（湿度/积雪）的混合窗口比天空的更长。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="地表过渡时长(秒)", ClampMin="0.0"))
	float SurfaceTransitionSeconds = 20.0f;
};

/** 材质通过天气 MPC 读取的地表状态（不含粒子）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherSurfaceGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|地表", meta=(DisplayName="启用地表参数"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|地表", meta=(DisplayName="地表湿度"))
	FTODWeatherCurve Wetness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|地表", meta=(DisplayName="地表积雪"))
	FTODWeatherCurve SnowAmount;
};

/** 高度雾：密度、内散射颜色、起终点距离、衰减以及第二层雾。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherFogGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="启用高度雾"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="雾密度"))
	FTODWeatherCurve Density;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="雾散射颜色"))
	FTODWeatherColorCurve InscatteringColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="起雾距离"))
	FTODWeatherCurve StartDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="远处截止距离"))
	FTODWeatherCurve EndDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="高度衰减"))
	FTODWeatherCurve HeightFalloff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="第二层雾密度"))
	FTODWeatherCurve SecondFogDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="第二层雾高度偏移"))
	FTODWeatherCurve SecondFogHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|高度雾", meta=(DisplayName="第二层雾高度衰减"))
	FTODWeatherCurve SecondFogHeightFalloff;
};

/** 太阳与月亮的调制（本天空装置有两盏明确的天体光源，不同于单盏定向光）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherDirectionalLightGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="启用方向光调节"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光强度缩放"))
	FTODWeatherCurve SunIntensityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="月光强度缩放"))
	FTODWeatherCurve MoonIntensityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光颜色"))
	FTODWeatherColorCurve SunColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="月光颜色"))
	FTODWeatherColorCurve MoonColor;

	/** Kelvin。0 = 保留昼夜颜色；> 0 则强制指定黑体色温。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光色温(K, 0=不启用)"))
	FTODWeatherCurve SunTemperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光体积散射强度"))
	FTODWeatherCurve SunVolumetricScattering;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="月光体积散射强度"))
	FTODWeatherCurve MoonVolumetricScattering;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光轴泛光强度"))
	FTODWeatherCurve SunBloomScale;
};

/** 天光的强度 / 颜色 / 体积贡献。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherSkyLightGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|天光", meta=(DisplayName="启用天光调节"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|天光", meta=(DisplayName="强度缩放"))
	FTODWeatherCurve IntensityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|天光", meta=(DisplayName="颜色"))
	FTODWeatherColorCurve LightColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|天光", meta=(DisplayName="体积散射强度"))
	FTODWeatherCurve VolumetricScattering;
};

/** 体积雾参数组（反照率 / 分布 / 消光 / 可视距离）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherVolumetricFogGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="启用体积雾"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="反照率"))
	FTODWeatherColorCurve Albedo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="散射分布"))
	FTODWeatherCurve ScatteringDistribution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="消光缩放"))
	FTODWeatherCurve ExtinctionScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="可视距离"))
	FTODWeatherCurve ViewDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|体积雾", meta=(DisplayName="起始距离"))
	FTODWeatherCurve StartDistance;
};

/** 全局风力强度（Niagara / 植被通过 MPC 与风组件读取）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherWindGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|风", meta=(DisplayName="启用风"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|风", meta=(DisplayName="风速"))
	FTODWeatherCurve Strength;
};

/** 天空大气的覆盖参数（物理散射调节项）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherAtmosphereGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="启用大气调节"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="瑞利散射缩放"))
	FTODWeatherCurve RayleighScatteringScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="米氏散射缩放"))
	FTODWeatherCurve MieScatteringScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="米氏各向异性"))
	FTODWeatherCurve MieAnisotropy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="米氏指数分布"))
	FTODWeatherCurve MieExponentialDistribution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="多散射系数"))
	FTODWeatherCurve MultiScatteringFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="高度雾贡献"))
	FTODWeatherCurve HeightFogContribution;
};

/** 体积云层：云量与高度。材质参数仍留在材质上。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherCloudGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="启用云层调节"))
	bool bEnabled = false;

	/** 0 = 晴朗，1 = 完全阴天。目前驱动云层可见性，后续驱动材质云量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云量"))
	FTODWeatherCurve Coverage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层底高(千米)"))
	FTODWeatherCurve LayerBottomKm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层厚度(千米)"))
	FTODWeatherCurve LayerHeightKm;
};

/** 后处理覆盖参数（作用于天空装置持有的全局非绑定后处理体积）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherPostProcessGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="启用后处理调节"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光偏移"))
	FTODWeatherCurve ExposureBias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光下限"))
	FTODWeatherCurve ExposureMinBrightness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光上限"))
	FTODWeatherCurve ExposureMaxBrightness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="色彩饱和度"))
	FTODWeatherCurve Saturation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="色彩对比度"))
	FTODWeatherCurve Contrast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="暗角强度"))
	FTODWeatherCurve VignetteIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="泛光强度"))
	FTODWeatherCurve BloomIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="胶片颗粒"))
	FTODWeatherCurve FilmGrainIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="间接光强度"))
	FTODWeatherCurve IndirectLightingIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="间接光颜色"))
	FTODWeatherColorCurve IndirectLightingColor;
};

/**
 * 天气特效强度表中的一行。
 * 天空装置生成并持有该系统，每帧将 Intensity * Scale 写入
 * ParameterName，并在强度淡出之后销毁组件
 * （天气切换，或昼夜曲线归零）。
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="是否启用"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="粒子系统"))
	TSoftObjectPtr<UNiagaraSystem> System;

	/** 每帧写入的用户参数（DreamFX 雨/雪暴露 User.SpawnRateScale）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="参数名"))
	FName ParameterName = TEXT("User.SpawnRateScale");

	/** 该效果在天气域内的强度；空曲线 = 特效不生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="强度曲线"))
	FTODWeatherCurve Intensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="缩放系数", ClampMin="0.0"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="相对位置"))
	FVector RelativeLocation = FVector(0.0f, 0.0f, 1000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="相对旋转"))
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

/** 天气特效表；分组开关让"只换雾、不动特效"这类变体成为可能。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherEffectsGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="启用天气特效"))
	bool bEnabled = false;

	/** 特效名 -> 强度。键是稳定 id（Rain/Snow/...），不是显示名。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="特效强度表"))
	TMap<FName, FTODWeatherEffect> Effects;
};

/**
 * 一个天气文件：按分组组织的曲线驱动参数。
 *
 * 每个数值字段都是 24 小时曲线（一天进度 [0,1]）。每个分组都有
 * 自己的启用开关：一个天气文件可以只驱动它关心的分组
 * （"只换雾、不动天空"），关闭的分组保持原样、不受改动。
 *
 * 运行时 = UTODWeatherSubsystem 在当前一天进度处求值这些数据，并在
 * 上一个与目标天气文件之间做混合；天空装置
 * （ATODSkyManager）应用混合后的结果。
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherProfileData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="身份"))
	FTODWeatherIdentity Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="地表"))
	FTODWeatherSurfaceGroup Surface;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="高度雾"))
	FTODWeatherFogGroup Fog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="方向光"))
	FTODWeatherDirectionalLightGroup DirectionalLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="天光"))
	FTODWeatherSkyLightGroup SkyLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="体积雾"))
	FTODWeatherVolumetricFogGroup VolumetricFog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="风"))
	FTODWeatherWindGroup Wind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="大气"))
	FTODWeatherAtmosphereGroup Atmosphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="云层"))
	FTODWeatherCloudGroup CloudLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="后处理"))
	FTODWeatherPostProcessGroup PostProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="天气特效"))
	FTODWeatherEffectsGroup WeatherEffects;

	/** 0 = 旧版扁平预设，1 = 曲线组合。迁移会写入 1。 */
	UPROPERTY(VisibleAnywhere, Category="天气", meta=(DisplayName="数据版本"))
	int32 DataVersion = 0;

	/** 旧版迁移来源（旧 WP_* 资产带有该标签）。不应再被制作。 */
	UPROPERTY()
	FTODWeatherPreset Core;

	/**
	 * 把旧版扁平的 Core 预设转换为曲线分组。结果是确定的：
	 * 同一个旧资产总是生成同样的曲线，重复调用两次也安全。
	 */
	void MigrateLegacyCore();
};

/**
 * UTODWeatherProfile
 *
 * 一个资产 == 一个天气文件（issue #12）。在编辑器中制作分组与曲线；
 * 天气子系统在启动时于 /Game/TOD/Profiles 下发现这些资产
 * （打包构建读取资产注册表）。天气名称取自 Data.Identity.Name，
 * 也就是 `tod.ForceWeather <name>` 接受的名称。
 */
UCLASS(BlueprintType, hidecategories=Object, meta=(DisplayName="TOD Weather Profile"))
class TEST_DREAMFX_API UTODWeatherProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="天气配置"))
	FTODWeatherProfileData Data;

	/** 用于选择该天气文件的天气名称（Data.Identity.Name）。 */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	FName GetProfileName() const { return Data.Identity.Name; }
};
