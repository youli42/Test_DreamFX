// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TODWeatherProfile.generated.h"

class UNiagaraSystem;

/**
 * One named weather preset - the core simulation values (issue #10).
 * Absolute targets; UTODWeatherSubsystem blends from the live state to the
 * target over the transition duration. Lives inside a UTODWeatherProfile
 * asset (or registered from code); the blend state machine consumes this.
 *
 * Display labels are Chinese (issue #12 polish); the property identifiers
 * below stay English - they are the serialization names, the reflection
 * paths (set_property / blueprints) and the doc/console vocabulary.
 */
USTRUCT(BlueprintType)
struct FTODWeatherPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="天气名称"))
	FName Name = NAME_None;

	/** Relative chance of being picked by the random scheduler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="调度权重", ClampMin="0.0"))
	float ScheduleWeight = 1.0f;

	/** 0 = clear, 1 = fully overcast. Drives cloud visibility today, cloud material in M3+. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="云量", ClampMin="0.0", ClampMax="1.0"))
	float CloudCoverage = 0.2f;

	/** Absolute ExponentialHeightFog fog density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="雾密度"))
	float FogDensity = 0.0003f;

	/** Global wind strength (Niagara / vegetation consumers read this via MPC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="风速", ClampMin="0.0"))
	float WindStrength = 0.3f;

	/** Ground wetness 0-1 (drives the Wetness MPC parameter; ground materials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="地表湿度", ClampMin="0.0", ClampMax="1.0"))
	float Wetness = 0.0f;

	/** Ground snow amount 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="地表积雪", ClampMin="0.0", ClampMax="1.0"))
	float SnowAmount = 0.0f;

	/** Multiplier on the sun's computed intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="日照强度缩放", ClampMin="0.0"))
	float SunIntensityScale = 1.0f;

	/** Multiplier on the moon's computed intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="月光强度缩放", ClampMin="0.0"))
	float MoonIntensityScale = 1.0f;

	/** Multiplier on the SkyLight intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="天光强度缩放", ClampMin="0.0"))
	float SkyLightScale = 1.0f;

	/** Marks lightning-capable weather (thunderstorm). Consumed by M4. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="伴有雷电"))
	bool bLightning = false;

	/** Rain particle intensity 0-1; drives NS_TOD_Rain User.SpawnRateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="雨强度", ClampMin="0.0", ClampMax="1.0"))
	float RainIntensity = 0.0f;

	/** Snow particle intensity 0-1; drives NS_TOD_Snow User.SpawnRateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="雪强度", ClampMin="0.0", ClampMax="1.0"))
	float SnowIntensity = 0.0f;
};

/** Which blended weather value feeds a Niagara slot's parameter (M4 applies this). */
UENUM(BlueprintType)
enum class ETODWeatherParamDriver : uint8
{
	RainIntensity UMETA(DisplayName="雨量"),
	SnowIntensity UMETA(DisplayName="雪量"),
	WindStrength  UMETA(DisplayName="风力"),
	FixedValue    UMETA(DisplayName="固定值"),
};

/**
 * One weather-driven particle effect. The sky rig spawns and owns the
 * component at runtime (manager-managed, no level placement needed), writes
 * driver*Scale into ParameterName every frame, and destroys it when the
 * weather no longer references the slot.
 * NOTE(M1): fields are authored now; spawning lands in M4.
 */
USTRUCT(BlueprintType)
struct FTODNiagaraSlot
{
	GENERATED_BODY()

	/** Particle system to spawn (e.g. NS_TOD_Rain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="粒子系统"))
	TSoftObjectPtr<UNiagaraSystem> System;

	/** User parameter written every frame. DreamFX rain/snow use User.SpawnRateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="参数名"))
	FName ParameterName = TEXT("User.SpawnRateScale");

	/** Blended weather value feeding the parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="驱动来源"))
	ETODWeatherParamDriver Driver = ETODWeatherParamDriver::RainIntensity;

	/** Final value = Driver value * Scale (lets one intensity drive several emitters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="缩放系数", ClampMin="0.0"))
	float Scale = 1.0f;

	/** Spawn location relative to the sky rig. Rain/snow systems expect a box overhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="相对位置"))
	FVector RelativeLocation = FVector::ZeroVector;

	/** Spawn rotation relative to the sky rig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="相对旋转"))
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

/**
 * SkyAtmosphere overrides (whitelist: one flag + one value per field).
 * Unchecked fields leave the component untouched, so switching weather never
 * leaves stale atmosphere values behind. NOTE(M1): authored now; applied in M2.
 */
USTRUCT(BlueprintType)
struct FTODAtmosphereConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="覆盖空气密度"))
	bool bOverrideAirDensityScale = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="空气密度缩放", EditCondition="bOverrideAirDensityScale", EditConditionHides, ClampMin="0.05", ClampMax="5.0"))
	float AirDensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="覆盖瑞利散射"))
	bool bOverrideRayleighScale = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="瑞利散射缩放", EditCondition="bOverrideRayleighScale", EditConditionHides, ClampMin="0.0", ClampMax="5.0"))
	float RayleighScatteringScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="覆盖米氏散射"))
	bool bOverrideMieScale = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="米氏散射缩放", EditCondition="bOverrideMieScale", EditConditionHides, ClampMin="0.0", ClampMax="10.0"))
	float MieScatteringScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="覆盖尘埃散射"))
	bool bOverrideDustScale = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="尘埃散射缩放", EditCondition="bOverrideDustScale", EditConditionHides, ClampMin="0.0", ClampMax="10.0"))
	float DustScatteringScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="覆盖臭氧密度"))
	bool bOverrideOzoneScale = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="臭氧密度缩放", EditCondition="bOverrideOzoneScale", EditConditionHides, ClampMin="0.0", ClampMax="10.0"))
	float OzoneDensityScale = 3.0f;
};

/**
 * Volumetric cloud layer overrides. Coverage itself comes from Core.
 * NOTE(M1): authored now; LayerBottom/Height apply in M2 via the component,
 * density/albedo through a dynamic material instance once the parameter
 * names of the cloud material are pinned down.
 */
USTRUCT(BlueprintType)
struct FTODCloudConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="覆盖云层底高"))
	bool bOverrideLayerBottom = false;
	/** Kilometres, engine cloud layer convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层底高(千米)", EditCondition="bOverrideLayerBottom", EditConditionHides, ClampMin="0.1", ClampMax="10.0"))
	float LayerBottomAltitudeKm = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="覆盖云层厚度"))
	bool bOverrideLayerHeight = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层厚度(千米)", EditCondition="bOverrideLayerHeight", EditConditionHides, ClampMin="0.1", ClampMax="20.0"))
	float LayerHeightKm = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="覆盖云密度"))
	bool bOverrideCloudDensity = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云密度", EditCondition="bOverrideCloudDensity", EditConditionHides, ClampMin="0.0", ClampMax="1.0"))
	float CloudDensity = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="覆盖云反照率"))
	bool bOverrideCloudAlbedo = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云反照率", EditCondition="bOverrideCloudAlbedo", EditConditionHides, ClampMin="0.0", ClampMax="5.0"))
	float CloudAlbedo = 0.5f;
};

/**
 * Post-process overrides written into a global APostProcessVolume owned by
 * the sky rig. Whitelist of eight knobs; each interpolates with the weather
 * transition. NOTE(M1): authored now; applied in M3.
 */
USTRUCT(BlueprintType)
struct FTODPostProcessConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖曝光偏移"))
	bool bOverrideExposureBias = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光偏移", EditCondition="bOverrideExposureBias", EditConditionHides, ClampMin="-10.0", ClampMax="10.0"))
	float AutoExposureBias = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖曝光范围"))
	bool bOverrideExposureRange = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光下限", EditCondition="bOverrideExposureRange", EditConditionHides, ClampMin="-10.0", ClampMax="0.0"))
	float AutoExposureMinBrightness = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="曝光上限", EditCondition="bOverrideExposureRange", EditConditionHides, ClampMin="0.0", ClampMax="20.0"))
	float AutoExposureMaxBrightness = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖饱和度"))
	bool bOverrideSaturation = false;
	/** 1 = neutral. Written to all three ColorSaturation bands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="色彩饱和度", EditCondition="bOverrideSaturation", EditConditionHides, ClampMin="0.0", ClampMax="2.0"))
	float ColorSaturation = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖对比度"))
	bool bOverrideContrast = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="色彩对比度", EditCondition="bOverrideContrast", EditConditionHides, ClampMin="0.25", ClampMax="4.0"))
	float ColorContrast = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖暗角"))
	bool bOverrideVignette = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="暗角强度", EditCondition="bOverrideVignette", EditConditionHides, ClampMin="0.0", ClampMax="1.2"))
	float VignetteIntensity = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖泛光"))
	bool bOverrideBloom = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="泛光强度", EditCondition="bOverrideBloom", EditConditionHides, ClampMin="0.0", ClampMax="5.0"))
	float BloomIntensity = 0.675f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="覆盖胶片颗粒"))
	bool bOverrideFilmGrain = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="胶片颗粒", EditCondition="bOverrideFilmGrain", EditConditionHides, ClampMin="0.0", ClampMax="1.0"))
	float FilmGrainIntensity = 0.0f;
};

/**
 * Everything one weather owns, authored in one UTODWeatherProfile asset:
 * core simulation values plus the atmosphere/cloud/post-process/particle
 * panels. M1 consumes Core; the other segments activate in M2-M4.
 */
USTRUCT(BlueprintType)
struct FTODWeatherProfileData
{
	GENERATED_BODY()

	/** Core weather values driving lighting/fog/cloud visibility right now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|核心", meta=(DisplayName="核心参数"))
	FTODWeatherPreset Core;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|大气", meta=(DisplayName="大气设置"))
	FTODAtmosphereConfig Atmosphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|云层", meta=(DisplayName="云层设置"))
	FTODCloudConfig Clouds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|后处理", meta=(DisplayName="后处理设置"))
	FTODPostProcessConfig PostProcess;

	/** Particle effects this weather spawns and drives (manager-owned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气|粒子", meta=(DisplayName="粒子槽位"))
	TArray<FTODNiagaraSlot> NiagaraSlots;
};

/**
 * UTODWeatherProfile
 *
 * One asset == one weather (issue #12). Author the whole panel in the editor,
 * or define the same struct from code with RegisterProfileFromData. The
 * weather subsystem discovers these under /Game/TOD/Profiles at startup
 * (cooked builds read the asset registry); names come from Data.Core.Name and
 * are what `tod.ForceWeather <name>` / ForceWeather(FName) accept.
 */
UCLASS(BlueprintType, hidecategories=Object, meta=(DisplayName="TOD Weather Profile"))
class TEST_DREAMFX_API UTODWeatherProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="天气", meta=(DisplayName="天气配置"))
	FTODWeatherProfileData Data;

	/** Preset name used to select this weather (Data.Core.Name). */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	FName GetProfileName() const { return Data.Core.Name; }
};
