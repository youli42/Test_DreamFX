// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODWeatherProfile.h"
#include "TODWeatherSubsystem.generated.h"

// FTODWeatherPreset moved to TODWeatherProfile.h with issue #12
// (one weather = one UTODWeatherProfile asset).

/**
 * Blended, frame-current weather values (what consumers should read).
 */
USTRUCT(BlueprintType)
struct FTODWeatherState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	FName CurrentPreset = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float CloudCoverage = 0.2f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float FogDensity = 0.0003f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float WindStrength = 0.3f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float Wetness = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SnowAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SunIntensityScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float MoonIntensityScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SkyLightScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	bool bLightning = false;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float RainIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="TOD|Weather")
	float SnowIntensity = 0.0f;
};

/** Serializable weather state; games persist the preset name with their save. */
USTRUCT(BlueprintType)
struct FTODWeatherSnapshot
{
	GENERATED_BODY()

	/** Preset the world was in at save time. */
	UPROPERTY(BlueprintReadWrite, Category="TOD|Weather")
	FName PresetName = TEXT("ClearSky");
};

/**
 * UTODWeatherSubsystem
 *
 * Weather state machine: named presets, timed transitions (no hard cuts), a
 * weighted random scheduler and a forced-weather override. Outputs the blended
 * FTODWeatherState each tick and pushes scalar parameters into the project's
 * Material Parameter Collection (/Game/TOD/MPC/TOD_WeatherMPC) so ground/cloud
 * materials can react without per-material plumbing.
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
	FName GetTargetPresetName() const { return TargetPreset.Name; }

	/** Blended current weather — read this every frame. */
	UFUNCTION(BlueprintPure, Category="TOD|Weather")
	const FTODWeatherState& GetCurrentState() const { return Current; }

	/** Register or replace a preset at runtime (also used by tests). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	void RegisterPreset(const FTODWeatherPreset& Preset);

	/** Register a full weather profile from code (issue #12) - equivalent to a
	 *  UTODWeatherProfile asset: Core feeds the state machine, the other panels
	 *  are stored for the appliers (M2-M4). */
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

	/** Seconds between automatic weather changes; <= 0 disables auto scheduling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="天气自动切换间隔(秒)"))
	float AutoChangeIntervalSeconds = 0.0f;

	/** Default transition duration when ForceWeather does not override it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|天气", meta=(DisplayName="默认过渡时长(秒)"))
	float TransitionSeconds = 3.0f;

	static void HandleConsoleForceWeather(const TArray<FString>& Args);
	static void HandleConsoleWeatherInfo();

private:
	void RegisterDefaultPresets();
	/** Populate from UTODWeatherProfile assets (issue #12: one weather = one
	 *  asset, discovered under /Game/TOD/Profiles). Returns false when the
	 *  search found nothing, letting the caller fall back to built-ins. */
	bool DiscoverProfileAssets();
	void ApplyStateToMPC();
	UTODWeatherSubsystem* AsInstance(const UWorld* World) const { return World ? World->GetSubsystem<UTODWeatherSubsystem>() : nullptr; }

	/** Console helper: first game/PIE world with this subsystem. */
	static UTODWeatherSubsystem* GetForConsole();

	FTODWeatherState Current;
	FTODWeatherPreset TargetPreset;
	FTODWeatherState FromState;
	TMap<FName, FTODWeatherPreset> Presets;

	/** Full profile per weather: Core duplicates the Presets map, the rest of
	 *  the panels (atmosphere/clouds/PP/Niagara) live here for the appliers. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UTODWeatherProfile>> ProfileAssets;

	/** Code-registered profiles kept alive (asset-backed ones need no pinning). */
	UPROPERTY()
	TArray<TObjectPtr<UTODWeatherProfile>> InMemoryProfiles;

	bool bTransitioning = false;
	float TransitionElapsed = 0.0f;
	float ActiveTransitionSeconds = 3.0f;
	float AutoChangeCountdown = 0.0f;

	/** Soft reference to the weather MPC; resolved lazily. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> WeatherCollection;
};
