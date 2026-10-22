// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TODWeatherCurve.h"
#include "TODWeatherProfile.generated.h"

class UNiagaraSystem;

/** Project particle systems used by the built-in fallback weathers and legacy migration. */
namespace TODWeatherDefaults
{
	inline constexpr TCHAR RainSystemPath[] = TEXT("/Game/TOD/Niagara/NS_TOD_Rain.NS_TOD_Rain");
	inline constexpr TCHAR SnowSystemPath[] = TEXT("/Game/TOD/Niagara/NS_TOD_Snow.NS_TOD_Snow");
}

/**
 * Legacy flat preset (issue #10/#12). Kept ONLY as the migration input for
 * WP_*.uasset files authored before the curve refactor: the old assets still
 * carry a `Data.Core` tag, and UTODWeatherProfile::PostLoad converts it into
 * the curve groups below. Nothing new should author this struct.
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

/** Weather identity + scheduling metadata (not blended over time). */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="天气名称"))
	FName Name = NAME_None;

	/** Relative chance of being picked by the random scheduler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="调度权重", ClampMin="0.0"))
	float ScheduleWeight = 1.0f;

	/** Marks lightning-capable weather (thunderstorm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="伴有雷电"))
	bool bLightning = false;

	/** Longer blend window for ground accumulation (wetness/snow) than the sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|身份", meta=(DisplayName="地表过渡时长(秒)", ClampMin="0.0"))
	float SurfaceTransitionSeconds = 20.0f;
};

/** Ground state consumed by materials through the weather MPC (no particles). */
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

/** Height fog: density, inscattering colour, start/end distance, falloff and the second fog layer. */
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

/** Sun and moon modulation (our rig has two explicit celestial lights, unlike a single direct light). */
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

	/** Kelvin. 0 = keep the time-of-day colour; > 0 forces a blackbody temperature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光色温(K, 0=不启用)"))
	FTODWeatherCurve SunTemperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光体积散射强度"))
	FTODWeatherCurve SunVolumetricScattering;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="月光体积散射强度"))
	FTODWeatherCurve MoonVolumetricScattering;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|方向光", meta=(DisplayName="阳光轴泛光强度"))
	FTODWeatherCurve SunBloomScale;
};

/** SkyLight intensity / colour / volumetric contribution. */
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

/** Volumetric fog knob set (albedo / distribution / extinction / view distance). */
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

/** Global wind strength (Niagara / vegetation read it via MPC and the wind component). */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherWindGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|风", meta=(DisplayName="启用风"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|风", meta=(DisplayName="风速"))
	FTODWeatherCurve Strength;
};

/** SkyAtmosphere overrides (the physical scattering knobs). */
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

/** Volumetric cloud layer: coverage and altitude. Material parameters stay on the material. */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherCloudGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="启用云层调节"))
	bool bEnabled = false;

	/** 0 = clear, 1 = fully overcast. Drives layer visibility today, material coverage later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云量"))
	FTODWeatherCurve Coverage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层底高(千米)"))
	FTODWeatherCurve LayerBottomKm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层厚度(千米)"))
	FTODWeatherCurve LayerHeightKm;
};

/** Post-process overrides (applied to a global unbound post-process volume owned by the sky rig). */
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
 * One row of the weather-effect intensity table.
 * The sky rig spawns and owns the system, writes Intensity * Scale into
 * ParameterName every frame and destroys the component once the intensity
 * fades out (weather change or time-of-day curve reaching zero).
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="是否启用"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="粒子系统"))
	TSoftObjectPtr<UNiagaraSystem> System;

	/** User parameter written every frame (DreamFX rain/snow expose User.SpawnRateScale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="参数名"))
	FName ParameterName = TEXT("User.SpawnRateScale");

	/** Weather-domain intensity for this effect; empty curve = effect dormant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="强度曲线"))
	FTODWeatherCurve Intensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="缩放系数", ClampMin="0.0"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="相对位置"))
	FVector RelativeLocation = FVector(0.0f, 0.0f, 1000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="相对旋转"))
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

/** The weather-effect table; the group switch keeps "only fog, don't touch effects" variants possible. */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherEffectsGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="启用天气特效"))
	bool bEnabled = false;

	/** 特效名 -> 强度. Keys are stable ids (Rain/Snow/...), not display names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|特效", meta=(DisplayName="特效强度表"))
	TMap<FName, FTODWeatherEffect> Effects;
};

/**
 * One weather file: grouped, curve-driven parameters.
 *
 * Every numeric field is a 24-hour curve (day progress [0,1]). Each group has
 * its own Enable switch: a weather file may drive only the groups it cares
 * about ("只换雾、不动天空"), and groups that are off are left untouched.
 *
 * Runtime = UTODWeatherSubsystem evaluates this data at the current day
 * progress and blends between the previous and target weather files; the sky
 * rig (ATODSkyManager) applies the blended result.
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

	/** 0 = legacy flat preset, 1 = curve composition. Migration writes 1. */
	UPROPERTY(VisibleAnywhere, Category="天气", meta=(DisplayName="数据版本"))
	int32 DataVersion = 0;

	/** Legacy migration source (old WP_* assets carry this tag). Never authored. */
	UPROPERTY()
	FTODWeatherPreset Core;

	/**
	 * Convert the legacy flat Core preset into curve groups. Deterministic:
	 * the same old asset always produces the same curves. Safe to call twice.
	 */
	void MigrateLegacyCore();
};

/**
 * UTODWeatherProfile
 *
 * One asset == one weather file (issue #12). Author the groups/curves in the
 * editor; the weather subsystem discovers these under /Game/TOD/Profiles at
 * startup (cooked builds read the asset registry). Weather names come from
 * Data.Identity.Name and are what `tod.ForceWeather <name>` accepts.
 */
UCLASS(BlueprintType, hidecategories=Object, meta=(DisplayName="TOD Weather Profile"))
class TEST_DREAMFX_API UTODWeatherProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="天气配置"))
	FTODWeatherProfileData Data;

	/** Weather name used to select this file (Data.Identity.Name). */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	FName GetProfileName() const { return Data.Identity.Name; }
};
