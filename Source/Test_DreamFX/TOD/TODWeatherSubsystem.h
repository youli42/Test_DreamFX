// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODWeatherProfile.h"
#include "TODWeatherSubsystem.generated.h"

class UNiagaraSystem;
class UTODWeatherProfile;

/** One evaluated row of the weather-effect intensity table. */
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
 * Blended, frame-current weather values (what consumers should read).
 *
 * Produced by evaluating the weather files at the current day progress
 * (0 = midnight, 0.5 = noon) and blending the previous file with the target
 * during a transition. Every group carries its own b*Enabled switch: disabled
 * groups must be left untouched by appliers ("只换雾、不动天空").
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

/** Serializable weather state; games persist the preset name with their save. */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherSnapshot
{
	GENERATED_BODY()

	/** Preset the world was in at save time. */
	UPROPERTY(BlueprintReadWrite, Category="TOD|Weather")
	FName PresetName = NAME_None;
};

/**
 * UTODWeatherSubsystem
 *
 * Weather state machine: named weather files, weighted random scheduler, and
 * a forced-weather override. Each tick it evaluates the active weather file's
 * 24-hour curves at the current day progress, blends from the previous file
 * during a transition, and publishes the blended FTODWeatherState - plus the
 * five material-facing scalars in TOD_WeatherMPC.
 *
 * The subsystem owns *state and blending only*; ATODSkyManager drives
 * TickWeather() from its Tick (single driver source, same as the clock).
 */
UCLASS()
class TEST_DREAMFX_API UTODWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Advance blending and the auto-scheduler. Call once per frame from a driver actor. */
	void TickWeather(float DeltaSeconds);

	/** Start blending to a named preset. Returns false if the name is unknown. */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	bool ForceWeather(FName PresetName, float InTransitionSeconds = -1.0f);

	/** Pick a preset by weighted random and transition to it. */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	void RandomizeWeather(float InTransitionSeconds = -1.0f);

	/** Target preset name (what we are blending towards; equals CurrentPreset when idle). */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	FName GetTargetPresetName() const { return TargetData.Identity.Name; }

	/** Blended current weather — read this every frame. */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	const FTODWeatherState& GetCurrentState() const { return Current; }

	/** Current day progress [0,1] used to evaluate the curves. */
	float GetDayProgress() const;

	/** Register a full weather file from code (equivalent to a UTODWeatherProfile asset). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	void RegisterProfileFromData(const FTODWeatherProfileData& NewData);

	/** Full profile data of a registered weather; false if the name is unknown. */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	bool GetProfileData(FName PresetName, FTODWeatherProfileData& OutData) const;

	/** Every registered weather name (asset-discovered + code-registered). */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	TArray<FName> GetRegisteredPresetNames() const;

	/** Snapshot the blend target for persistence (issue #10 save interface). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	FTODWeatherSnapshot GetWeatherSnapshot() const;

	/** Restore a snapshot; bInstant skips the transition (correct for load). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	bool ApplyWeatherSnapshot(const FTODWeatherSnapshot& Snapshot, bool bInstant = true);

	/** Evaluate one weather file at a day progress. Pure; used by the editor preview too. */
	static FTODWeatherState EvaluateProfileAt(const FTODWeatherProfileData& Data, float DayProgress);

	/** Blend two evaluated states (sky alpha, and a separate slower alpha for ground state). */
	static FTODWeatherState BlendStates(const FTODWeatherState& From, const FTODWeatherState& To, float Alpha, float SurfaceAlpha);

	/** Seconds between automatic weather changes; <= 0 disables auto scheduling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="天气自动切换间隔(秒)"))
	float AutoChangeIntervalSeconds = 0.0f;

	/** Default transition duration when ForceWeather does not override it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="默认过渡时长(秒)"))
	float TransitionSeconds = 3.0f;

	/** Weather used on startup and when a snapshot is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="默认天气"))
	FName DefaultWeatherName = TEXT("ClearSky");

	static void HandleConsoleForceWeather(const TArray<FString>& Args);
	static void HandleConsoleWeatherInfo();

private:
	void RegisterFallbackProfiles();
	/** Populate from UTODWeatherProfile assets under /Game/TOD/Profiles. */
	bool DiscoverProfileAssets();
	void ApplyStateToMPC();
	UTODWeatherSubsystem* AsInstance(const UWorld* World) const { return World ? World->GetSubsystem<UTODWeatherSubsystem>() : nullptr; }

	/** Console helper: first game/PIE world with this subsystem. */
	static UTODWeatherSubsystem* GetForConsole();

	/** Registered weather files (asset-discovered and code-registered). */
	TMap<FName, FTODWeatherProfileData> Profiles;

	/** Asset-backed profiles are pinned here. */
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

	/** Soft reference to the weather MPC; resolved lazily. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> WeatherCollection;
};
