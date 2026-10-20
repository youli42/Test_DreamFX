// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODWeatherSubsystem.generated.h"

/**
 * One named weather preset. Values are absolute targets; the subsystem blends
 * from the current state to the target over the transition duration.
 */
USTRUCT(BlueprintType)
struct FTODWeatherPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather")
	FName Name = NAME_None;

	/** Relative chance of being picked by the random scheduler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0"))
	float ScheduleWeight = 1.0f;

	/** 0 = clear, 1 = fully overcast. Drives cloud visibility today, cloud material in M3+. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CloudCoverage = 0.2f;

	/** Absolute ExponentialHeightFog fog density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather")
	float FogDensity = 0.0003f;

	/** Global wind strength (Niagara / vegetation consumers read this via MPC in M3+). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0"))
	float WindStrength = 0.3f;

	/** Ground wetness 0-1 (drives the Wetness MPC parameter; ground materials in M3+). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Wetness = 0.0f;

	/** Ground snow amount 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SnowAmount = 0.0f;

	/** Multiplier on the sun's computed intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0"))
	float SunIntensityScale = 1.0f;

	/** Multiplier on the moon's computed intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0"))
	float MoonIntensityScale = 1.0f;

	/** Multiplier on the SkyLight intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0"))
	float SkyLightScale = 1.0f;

	/** Marks lightning-capable weather (thunderstorm). Consumed by M4. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather")
	bool bLightning = false;

	/** Rain particle intensity 0-1; drives NS_TOD_Rain User.SpawnRateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RainIntensity = 0.0f;

	/** Snow particle intensity 0-1; drives NS_TOD_Snow User.SpawnRateScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SnowIntensity = 0.0f;
};

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
 * Material Parameter Collection (/Game/TOD/TOD_WeatherMPC) so ground/cloud
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

	/** Snapshot the blend target for persistence (issue #10 save interface). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	FTODWeatherSnapshot GetWeatherSnapshot() const;

	/** Restore a snapshot; bInstant skips the transition (correct for load). */
	UFUNCTION(BlueprintCallable, Category="TOD|Weather")
	bool ApplyWeatherSnapshot(const FTODWeatherSnapshot& Snapshot, bool bInstant = true);

	/** Seconds between automatic weather changes; <= 0 disables auto scheduling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather")
	float AutoChangeIntervalSeconds = 0.0f;

	/** Default transition duration when ForceWeather does not override it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|Weather")
	float TransitionSeconds = 3.0f;

	static void HandleConsoleForceWeather(const TArray<FString>& Args);
	static void HandleConsoleWeatherInfo();

private:
	void RegisterDefaultPresets();
	void ApplyStateToMPC();
	UTODWeatherSubsystem* AsInstance(const UWorld* World) const { return World ? World->GetSubsystem<UTODWeatherSubsystem>() : nullptr; }

	/** Console helper: first game/PIE world with this subsystem. */
	static UTODWeatherSubsystem* GetForConsole();

	FTODWeatherState Current;
	FTODWeatherPreset TargetPreset;
	FTODWeatherState FromState;
	TMap<FName, FTODWeatherPreset> Presets;

	bool bTransitioning = false;
	float TransitionElapsed = 0.0f;
	float ActiveTransitionSeconds = 3.0f;
	float AutoChangeCountdown = 0.0f;

	/** Soft reference to the weather MPC; resolved lazily. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> WeatherCollection;
};
