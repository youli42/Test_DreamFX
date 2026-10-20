// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODSubsystem.h"

#include "TODSkyManager.h"

#include "Engine/World.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogTOD, Log, All);

namespace TODCVars
{
	/** Global override for the time scale. Negative values fall back to the subsystem default. */
	TAutoConsoleVariable<float> CVarTODTimeScale(
		TEXT("tod.TimeScale"),
		-1.0f,
		TEXT("Override for TOD time scale (in-game hours per real-time second). Negative = use the subsystem default."),
		ECVF_Default);

	FAutoConsoleCommand CmdSetTime(
		TEXT("tod.SetTime"),
		TEXT("Set the in-game time of day in hours (0-24). Usage: tod.SetTime 21.5"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&UTODSubsystem::HandleConsoleSetTime));

	FAutoConsoleCommand CmdInfo(
		TEXT("tod.Info"),
		TEXT("Print the current TOD state (time, sun/moon elevation)."),
		FConsoleCommandDelegate::CreateStatic(&UTODSubsystem::HandleConsoleInfo));
}

bool UTODSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create for playable worlds (game + PIE); skip editor preview worlds.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		const EWorldType::Type WorldType = World->WorldType;
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
	return false;
}

void UTODSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTOD, Log, TEXT("TODSubsystem initialized. Default time %.2fh, day length %.2f min."), TimeOfDayHours, DayLengthMinutes);
}

void UTODSubsystem::Deinitialize()
{
	UE_LOG(LogTOD, Log, TEXT("TODSubsystem deinitialized at %.2fh."), TimeOfDayHours);
	Super::Deinitialize();
}

void UTODSubsystem::AdvanceTime(float DeltaSeconds)
{
	if (!bAutoAdvance || DeltaSeconds <= 0.f)
	{
		return;
	}
	SetTimeOfDayHours(TimeOfDayHours + DeltaSeconds * GetTimeScale());
}

void UTODSubsystem::SetTimeOfDayHours(float InHours)
{
	TimeOfDayHours = FMath::Wrap(InHours, 0.0f, 24.0f);
}

float UTODSubsystem::GetTimeScale() const
{
	const float CVarOverride = TODCVars::CVarTODTimeScale.GetValueOnGameThread();
	if (CVarOverride >= 0.0f)
	{
		return CVarOverride;
	}
	if (DayLengthMinutes <= 0.0f)
	{
		return 0.0f; // paused
	}
	return 24.0f / (60.0f * DayLengthMinutes);
}

void UTODSubsystem::SetDayLengthMinutes(float MinutesPerDay)
{
	DayLengthMinutes = FMath::Max(MinutesPerDay, 0.0f);
	UE_LOG(LogTOD, Log, TEXT("Day length set to %.2f real minutes per in-game day (%.3f h/s)."), DayLengthMinutes, GetTimeScale());
}

float UTODSubsystem::GetDayLengthMinutes() const
{
	return DayLengthMinutes;
}

FTODTimeSnapshot UTODSubsystem::GetTimeSnapshot() const
{
	FTODTimeSnapshot Snapshot;
	Snapshot.TimeOfDayHours = TimeOfDayHours;
	Snapshot.DayLengthMinutes = DayLengthMinutes;
	return Snapshot;
}

void UTODSubsystem::ApplyTimeSnapshot(const FTODTimeSnapshot& Snapshot)
{
	DayLengthMinutes = FMath::Max(Snapshot.DayLengthMinutes, 0.0f);
	SetTimeOfDayHours(Snapshot.TimeOfDayHours);
	UE_LOG(LogTOD, Log, TEXT("Time snapshot applied: %.2fh, %.2f min/day."), TimeOfDayHours, DayLengthMinutes);
}

float UTODSubsystem::GetSunElevation01() const
{
	// Single source of truth: delegate to the sky rig's tilted-orbit model.
	// Peaks at sin(noon altitude) < 1 — the sun never crosses the zenith.
	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAz = 0.0f;
	float MoonAz = 0.0f;
	ATODSkyManager::ComputeSunMoonState(TimeOfDayHours, 0.0f, 0.0f, ATODSkyManager::DefaultSunTiltDegrees,
		SunElev, MoonElev, SunAz, MoonAz);
	return SunElev;
}

float UTODSubsystem::GetMoonElevation01() const
{
	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAz = 0.0f;
	float MoonAz = 0.0f;
	ATODSkyManager::ComputeSunMoonState(TimeOfDayHours, 0.0f, 0.0f, ATODSkyManager::DefaultSunTiltDegrees,
		SunElev, MoonElev, SunAz, MoonAz);
	return MoonElev;
}

UTODSubsystem* UTODSubsystem::GetForConsole()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (UTODSubsystem* Subsystem = World->GetSubsystem<UTODSubsystem>())
				{
					return Subsystem;
				}
			}
		}
	}
	return nullptr;
}

void UTODSubsystem::HandleConsoleSetTime(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTOD, Warning, TEXT("Usage: tod.SetTime <hours 0-24>"));
		return;
	}
	UTODSubsystem* Subsystem = GetForConsole();
	if (!Subsystem)
	{
		UE_LOG(LogTOD, Warning, TEXT("tod.SetTime: no game world with TODSubsystem is running."));
		return;
	}
	const float Hours = FCString::Atof(*Args[0]);
	Subsystem->SetTimeOfDayHours(Hours);
	UE_LOG(LogTOD, Log, TEXT("tod.SetTime: clock set to %.2fh."), Subsystem->GetTimeOfDayHours());
}

void UTODSubsystem::HandleConsoleInfo()
{
	const UTODSubsystem* Subsystem = GetForConsole();
	if (!Subsystem)
	{
		UE_LOG(LogTOD, Warning, TEXT("tod.Info: no game world with TODSubsystem is running."));
		return;
	}
	UE_LOG(LogTOD, Log, TEXT("TOD: %.2fh | sun elevation %.2f | moon elevation %.2f | scale %.3fh/s"),
		Subsystem->GetTimeOfDayHours(),
		Subsystem->GetSunElevation01(),
		Subsystem->GetMoonElevation01(),
		Subsystem->GetTimeScale());
}
