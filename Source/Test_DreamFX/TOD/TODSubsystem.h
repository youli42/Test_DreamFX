// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODSubsystem.generated.h"

/** Serializable clock state; games persist this alongside their save. */
USTRUCT(BlueprintType)
struct FTODTimeSnapshot
{
	GENERATED_BODY()

	/** In-game hours [0, 24) at save time. */
	UPROPERTY(BlueprintReadWrite, Category="TOD")
	float TimeOfDayHours = 9.0f;

	/** Real minutes one in-game day takes at save time. */
	UPROPERTY(BlueprintReadWrite, Category="TOD")
	float DayLengthMinutes = 4.0f;
};

/**
 * UTODSubsystem
 *
 * World-scoped time-of-day state. Holds the authoritative in-game clock and
 * exposes it to any driver (ATODSkyManager advances it by default).
 *
 * Design note: the subsystem owns *state only*; it never ticks. A scene actor
 * (ATODSkyManager) advances the clock so that time only moves while a sky
 * rig exists in the level. This mirrors the "environment manager broadcasts
 * state" pattern used by production weather/TOD systems.
 */
UCLASS(Blueprintable, BlueprintType)
class TEST_DREAMFX_API UTODSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Advance the in-game clock. Call once per frame from a driver actor. */
	void AdvanceTime(float DeltaSeconds);

	/** Current in-game time in hours [0, 24). 0 = midnight, 12 = noon. */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	/** Set the in-game clock directly (wraps into [0, 24)). */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void SetTimeOfDayHours(float InHours);

	/** Sine of sun altitude on the tilted orbit (see ATODSkyManager): 0 at sunrise/sunset, peaks at sin(SunTilt) ≈ 0.87 at noon. */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetSunElevation01() const;

	/** Sine of the moon's altitude (anti-solar point on the same tilted circle). */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetMoonElevation01() const;

	/** In-game hours per real-time second while advancing. */
	float GetTimeScale() const;

	/** Set the pace of time: real minutes an in-game day takes (0 disables advancing). */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void SetDayLengthMinutes(float MinutesPerDay);

	UFUNCTION(BlueprintPure, Category="TOD")
	float GetDayLengthMinutes() const;

	/** Snapshot the clock for persistence (issue #10 "天气/时间可随存档保存"). */
	UFUNCTION(BlueprintCallable, Category="TOD")
	FTODTimeSnapshot GetTimeSnapshot() const;

	/** Restore a snapshot taken with GetTimeSnapshot. */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void ApplyTimeSnapshot(const FTODTimeSnapshot& Snapshot);

	/** Console command handler: set the clock, e.g. `tod.SetTime 21.5`. */
	static void HandleConsoleSetTime(const TArray<FString>& Args);

	/** Console command handler: print current TOD state, `tod.Info`. */
	static void HandleConsoleInfo();

	/** Resolve a subsystem for console commands without a world context (uses PIE / first game world). */
	static UTODSubsystem* GetForConsole();

private:
	/** In-game hours [0, 24). */
	float TimeOfDayHours = 9.0f;

	/** Real minutes per in-game day; drives TimeScaleHoursPerSecond (24 / (60*minutes)). */
	UPROPERTY(EditAnywhere, Category="TOD|时间", meta=(DisplayName="一天时长(真实分钟)"))
	float DayLengthMinutes = 4.0f;

	/** When false, AdvanceTime is a no-op (clock still settable). */
	bool bAutoAdvance = true;
};
