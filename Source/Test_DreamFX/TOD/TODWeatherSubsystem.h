// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODWeatherProfile.h"
#include "TODWeatherSubsystem.generated.h"

class UNiagaraSystem;
class UTODWeatherProfile;

/** 天气特效强度表中经求值后的一行。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherEffectState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	TSoftObjectPtr<UNiagaraSystem> System;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FName ParameterName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float Intensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float Scale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

/**
 * 混合后的当帧天气数值（使用者应当读取的内容）。
 *
 * 生成方式：按当前一天进度求值天气文件
 * （0 = 午夜，0.5 = 正午），并在过渡期间把上一个文件与目标文件
 * 混合。每个组都带有自己的 b*Enabled 启用开关：未启用的组
 * 必须被应用方原样跳过（“只换雾、不动天空”）。
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FName CurrentPreset = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bLightning = false;

	/* ---- 地表 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bSurfaceEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float Wetness = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SnowAmount = 0.0f;

	/* ---- 高度雾 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bFogEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float FogDensity = 0.0003f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor FogInscatteringColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasFogInscatteringColor = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float FogStartDistance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float FogEndDistance = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float FogHeightFalloff = 0.2f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SecondFogDensity = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SecondFogHeightOffset = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SecondFogHeightFalloff = 0.2f;

	/* ---- 方向光 (日月) ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bLightEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SunIntensityScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MoonIntensityScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor SunLightColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor MoonLightColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasSunColor = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasMoonColor = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SunTemperature = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SunVolumetricScattering = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MoonVolumetricScattering = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SunBloomScale = 1.0f;

	/* ---- 天光 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bSkyLightEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SkyLightScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor SkyLightColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasSkyLightColor = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SkyLightVolumetricScattering = 1.0f;

	/* ---- 体积雾 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bVolumetricFogEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor VolumetricFogAlbedo = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasVolumetricFogAlbedo = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float VolumetricFogScatteringDistribution = 0.5f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float VolumetricFogExtinctionScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float VolumetricFogViewDistance = 6000.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float VolumetricFogStartDistance = 0.0f;

	/* ---- 风 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bWindEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float WindStrength = 0.0f;

	/* ---- 大气 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bAtmosphereEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float RayleighScatteringScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MieScatteringScale = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MieAnisotropy = 0.8f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MieExponentialDistribution = 0.2f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MultiScatteringFactor = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float HeightFogContribution = 1.0f;

	/* ---- 云层 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bCloudEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float CloudCoverage = 0.2f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float CloudLayerBottomKm = 1.5f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float CloudLayerHeightKm = 4.0f;

	/* ---- 后处理 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bPostProcessEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPExposureBias = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPExposureMinBrightness = -1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPExposureMaxBrightness = 3.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPSaturation = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPContrast = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPVignetteIntensity = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPBloomIntensity = 0.675f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPFilmGrainIntensity = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float PPIndirectLightingIntensity = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FLinearColor PPIndirectLightingColor = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bHasPPIndirectLightingColor = false;

	/* ---- 天气特效强度表 ---- */
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bEffectsEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	TMap<FName, FTODWeatherEffectState> Effects;
};

/** 可序列化的天气状态；游戏将预设名随自身存档持久化。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherSnapshot
{
	GENERATED_BODY()

	/** 存档时世界所处的预设。 */
	UPROPERTY(BlueprintReadWrite, Category="TOD|Weather")
	FName PresetName = NAME_None;
};

/**
 * UTODWeatherSubsystem
 *
 * 天气状态机：具名天气文件、加权随机调度器和强制天气覆盖。
 * 每 Tick 按当前一天进度求值激活天气文件的 24 小时曲线，
 * 过渡期间从上一个文件混合过来，并发布混合后的
 * FTODWeatherState——外加 TOD_WeatherMPC 中面向材质的
 * 五个标量。
 *
 * 本子系统*只负责状态与混合*；ATODSkyManager 在其 Tick 中
 * 驱动 TickWeather()（唯一驱动者，与世界时钟一致）。
 */
UCLASS()
class TEST_DREAMFX_API UTODWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 推进混合与自动调度器。由驱动者 Actor 每帧调用一次。 */
	void TickWeather(float DeltaSeconds);

	/** 开始向指定预设混合。名称未知时返回 false。 */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	bool ForceWeather(FName PresetName, float InTransitionSeconds = -1.0f);

	/** 以加权随机挑选一个预设并过渡到它。 */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	void RandomizeWeather(float InTransitionSeconds = -1.0f);

	/** 目标预设名（正在混合向的目标；空闲时等于 CurrentPreset）。 */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	FName GetTargetPresetName() const { return TargetData.Identity.Name; }

	/** 混合后的当前天气——每帧读取此值。 */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	const FTODWeatherState& GetCurrentState() const { return Current; }

	/** 用于求值曲线的当前一天进度 [0,1]。 */
	float GetDayProgress() const;

	/** 从代码注册完整天气文件（等价于一个 UTODWeatherProfile 资产）。 */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	void RegisterProfileFromData(const FTODWeatherProfileData& NewData);

	/** 已注册天气的完整配置文件数据；名称未知时返回 false。 */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	bool GetProfileData(FName PresetName, FTODWeatherProfileData& OutData) const;

	/** 所有已注册的天气名（资产发现 + 代码注册）。 */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	TArray<FName> GetRegisteredPresetNames() const;

	/** 为持久化而对混合目标做快照（issue #10 存档接口）。 */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	FTODWeatherSnapshot GetWeatherSnapshot() const;

	/** 恢复快照；bInstant 表示跳过过渡（读档时应如此）。 */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	bool ApplyWeatherSnapshot(const FTODWeatherSnapshot& Snapshot, bool bInstant = true);

	/** 在给定一天进度下求值某个天气文件。纯函数；编辑器预览亦使用。 */
	static FTODWeatherState EvaluateProfileAt(const FTODWeatherProfileData& Data, float DayProgress);

	/** 混合两个已求值的状态（天空 alpha，以及地表状态单独的较慢 alpha）。 */
	static FTODWeatherState BlendStates(const FTODWeatherState& From, const FTODWeatherState& To, float Alpha, float SurfaceAlpha);

	/** 自动切换天气的间隔秒数；<= 0 时禁用自动调度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="天气自动切换间隔(秒)"))
	float AutoChangeIntervalSeconds = 0.0f;

	/** ForceWeather 不覆盖时的默认过渡时长。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="默认过渡时长(秒)"))
	float TransitionSeconds = 3.0f;

	/** 启动时以及缺少快照时所使用的天气。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="默认天气"))
	FName DefaultWeatherName = TEXT("ClearSky");

	static void HandleConsoleForceWeather(const TArray<FString>& Args);
	static void HandleConsoleWeatherInfo();

private:
	void RegisterFallbackProfiles();
	/** 从 /Game/TOD/Profiles 下的 UTODWeatherProfile 资产填充。 */
	bool DiscoverProfileAssets();
	void ApplyStateToMPC();
	UTODWeatherSubsystem* AsInstance(const UWorld* World) const { return World ? World->GetSubsystem<UTODWeatherSubsystem>() : nullptr; }

	/** 控制台辅助：首个含本子系统的游戏/PIE 世界。 */
	static UTODWeatherSubsystem* GetForConsole();

	/** 已注册的天气文件（资产发现与代码注册）。 */
	TMap<FName, FTODWeatherProfileData> Profiles;

	/** 由资产支撑的配置文件在此固定。 */
	UPROPERTY()
	TMap<FName, TObjectPtr<UTODWeatherProfile>> ProfileAssets;

	FTODWeatherState Current;
	FTODWeatherProfileData TargetData;
	FTODWeatherProfileData FromData;

	bool bTransitioning = false;
	float TransitionElapsed = 0.0f;
	float ActiveTransitionSeconds = 3.0f;
	float ActiveSurfaceSeconds = 20.0f;
	float AutoChangeCountdown = 0.0f;

	/** 天气 MPC 的软引用；延迟解析。 */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> WeatherCollection;
};
