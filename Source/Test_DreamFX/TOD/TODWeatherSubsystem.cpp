// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODWeatherSubsystem.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogTODWeather, Log, All);

namespace TODWeatherCVars
{
	/** Runtime override for the auto-change interval; <= 0 keeps the subsystem default. */
	TAutoConsoleVariable<float> CVarAutoChange(
		TEXT("tod.AutoChangeSeconds"),
		0.0f,
		TEXT("Override the automatic weather change interval in seconds (>0 enables). 0 keeps the subsystem default (usually disabled)."),
		ECVF_Default);

	FAutoConsoleCommand CmdForceWeather(
		TEXT("tod.ForceWeather"),
		TEXT("Transition to a named weather preset. Usage: tod.ForceWeather Thunderstorm"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&UTODWeatherSubsystem::HandleConsoleForceWeather));

	FAutoConsoleCommand CmdWeatherInfo(
		TEXT("tod.WeatherInfo"),
		TEXT("Print the current blended weather state."),
		FConsoleCommandDelegate::CreateStatic(&UTODWeatherSubsystem::HandleConsoleWeatherInfo));
}

bool UTODWeatherSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		const EWorldType::Type WorldType = World->WorldType;
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
	return false;
}

void UTODWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterDefaultPresets();

	// Seed the blended state from the first preset so frame 0 is coherent.
	const FTODWeatherPreset* First = Presets.Find(TEXT("ClearSky"));
	TargetPreset = First ? *First : FTODWeatherPreset();
	Current.CurrentPreset = TargetPreset.Name;
	Current.CloudCoverage = TargetPreset.CloudCoverage;
	Current.FogDensity = TargetPreset.FogDensity;
	Current.WindStrength = TargetPreset.WindStrength;
	Current.Wetness = TargetPreset.Wetness;
	Current.SnowAmount = TargetPreset.SnowAmount;
	Current.SunIntensityScale = TargetPreset.SunIntensityScale;
	Current.MoonIntensityScale = TargetPreset.MoonIntensityScale;
	Current.SkyLightScale = TargetPreset.SkyLightScale;
	Current.bLightning = TargetPreset.bLightning;

	UE_LOG(LogTODWeather, Log, TEXT("TODWeatherSubsystem initialized. %d presets, starting '%s'."), Presets.Num(), *TargetPreset.Name.ToString());
}

void UTODWeatherSubsystem::Deinitialize()
{
	UE_LOG(LogTODWeather, Log, TEXT("TODWeatherSubsystem deinitialized at '%s'."), *Current.CurrentPreset.ToString());
	Super::Deinitialize();
}

void UTODWeatherSubsystem::RegisterDefaultPresets()
{
	FTODWeatherPreset Clear;
	Clear.Name = TEXT("ClearSky");
	Clear.ScheduleWeight = 3.0f;
	Clear.CloudCoverage = 0.15f;
	Clear.FogDensity = 0.0002f;
	Clear.WindStrength = 0.25f;
	Clear.Wetness = 0.0f;
	Clear.SunIntensityScale = 1.0f;
	Clear.SkyLightScale = 1.0f;
	RegisterPreset(Clear);

	FTODWeatherPreset Partly;
	Partly.Name = TEXT("PartlyCloudy");
	Partly.ScheduleWeight = 3.0f;
	Partly.CloudCoverage = 0.45f;
	Partly.FogDensity = 0.0003f;
	Partly.WindStrength = 0.45f;
	Partly.SunIntensityScale = 0.85f;
	Partly.SkyLightScale = 0.9f;
	RegisterPreset(Partly);

	FTODWeatherPreset Overcast;
	Overcast.Name = TEXT("Overcast");
	Overcast.ScheduleWeight = 2.0f;
	Overcast.CloudCoverage = 0.9f;
	Overcast.FogDensity = 0.0006f;
	Overcast.WindStrength = 0.6f;
	Overcast.SunIntensityScale = 0.35f;
	Overcast.SkyLightScale = 0.7f;
	RegisterPreset(Overcast);

	FTODWeatherPreset Foggy;
	Foggy.Name = TEXT("Foggy");
	Foggy.ScheduleWeight = 1.0f;
	Foggy.CloudCoverage = 0.75f;
	Foggy.FogDensity = 0.0080f;
	Foggy.WindStrength = 0.15f;
	Foggy.SunIntensityScale = 0.4f;
	Foggy.SkyLightScale = 0.65f;
	RegisterPreset(Foggy);

	FTODWeatherPreset LightRain;
	LightRain.Name = TEXT("LightRain");
	LightRain.ScheduleWeight = 1.5f;
	LightRain.CloudCoverage = 0.85f;
	LightRain.FogDensity = 0.0012f;
	LightRain.WindStrength = 0.7f;
	LightRain.Wetness = 0.7f;
	LightRain.SunIntensityScale = 0.3f;
	LightRain.SkyLightScale = 0.6f;
	RegisterPreset(LightRain);

	FTODWeatherPreset Thunder;
	Thunder.Name = TEXT("Thunderstorm");
	Thunder.ScheduleWeight = 1.0f;
	Thunder.CloudCoverage = 1.0f;
	Thunder.FogDensity = 0.0018f;
	Thunder.WindStrength = 1.0f;
	Thunder.Wetness = 1.0f;
	Thunder.SunIntensityScale = 0.18f;
	Thunder.SkyLightScale = 0.5f;
	Thunder.bLightning = true;
	RegisterPreset(Thunder);
}

void UTODWeatherSubsystem::RegisterPreset(const FTODWeatherPreset& Preset)
{
	if (Preset.Name.IsNone())
	{
		UE_LOG(LogTODWeather, Warning, TEXT("RegisterPreset refused a preset with no name."));
		return;
	}
	Presets.Add(Preset.Name, Preset);
}

bool UTODWeatherSubsystem::ForceWeather(FName PresetName, float InTransitionSeconds)
{
	const FTODWeatherPreset* Found = Presets.Find(PresetName);
	if (!Found)
	{
		UE_LOG(LogTODWeather, Warning, TEXT("ForceWeather: unknown preset '%s'. Known: ClearSky, PartlyCloudy, Overcast, Foggy, LightRain, Thunderstorm (+ any registered)."), *PresetName.ToString());
		return false;
	}

	FromState = Current;
	TargetPreset = *Found;
	ActiveTransitionSeconds = (InTransitionSeconds >= 0.0f) ? InTransitionSeconds : FMath::Max(TransitionSeconds, 0.1f);
	TransitionElapsed = 0.0f;
	bTransitioning = true;

	UE_LOG(LogTODWeather, Log, TEXT("Weather '%s' -> '%s' over %.1fs."), *Current.CurrentPreset.ToString(), *PresetName.ToString(), ActiveTransitionSeconds);
	return true;
}

void UTODWeatherSubsystem::RandomizeWeather(float InTransitionSeconds)
{
	if (Presets.Num() == 0)
	{
		return;
	}
	float TotalWeight = 0.0f;
	for (const TPair<FName, FTODWeatherPreset>& Pair : Presets)
	{
		TotalWeight += FMath::Max(Pair.Value.ScheduleWeight, 0.0f);
	}
	if (TotalWeight <= 0.0f)
	{
		return;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const TPair<FName, FTODWeatherPreset>& Pair : Presets)
	{
		Roll -= FMath::Max(Pair.Value.ScheduleWeight, 0.0f);
		if (Roll <= 0.0f)
		{
			ForceWeather(Pair.Key, InTransitionSeconds);
			return;
		}
	}
}

void UTODWeatherSubsystem::TickWeather(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Transition blending (smoothstep for a soft start/end).
	if (bTransitioning)
	{
		TransitionElapsed = FMath::Min(TransitionElapsed + DeltaSeconds, ActiveTransitionSeconds);
		const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, TransitionElapsed / ActiveTransitionSeconds, 2.0f);

		Current.CloudCoverage = FMath::Lerp(FromState.CloudCoverage, TargetPreset.CloudCoverage, Alpha);
		Current.FogDensity = FMath::Lerp(FromState.FogDensity, TargetPreset.FogDensity, Alpha);
		Current.WindStrength = FMath::Lerp(FromState.WindStrength, TargetPreset.WindStrength, Alpha);
		Current.Wetness = FMath::Lerp(FromState.Wetness, TargetPreset.Wetness, Alpha);
		Current.SnowAmount = FMath::Lerp(FromState.SnowAmount, TargetPreset.SnowAmount, Alpha);
		Current.SunIntensityScale = FMath::Lerp(FromState.SunIntensityScale, TargetPreset.SunIntensityScale, Alpha);
		Current.MoonIntensityScale = FMath::Lerp(FromState.MoonIntensityScale, TargetPreset.MoonIntensityScale, Alpha);
		Current.SkyLightScale = FMath::Lerp(FromState.SkyLightScale, TargetPreset.SkyLightScale, Alpha);
		Current.bLightning = TargetPreset.bLightning;
		Current.CurrentPreset = TargetPreset.Name;

		if (TransitionElapsed >= ActiveTransitionSeconds)
		{
			bTransitioning = false;
			UE_LOG(LogTODWeather, Log, TEXT("Weather transition to '%s' complete."), *TargetPreset.Name.ToString());
		}
	}

	// Weighted random scheduler (CVar override wins over the subsystem default).
	float Interval = AutoChangeIntervalSeconds;
	if (const float CVarInterval = TODWeatherCVars::CVarAutoChange.GetValueOnGameThread(); CVarInterval > 0.0f)
	{
		Interval = CVarInterval;
	}
	if (Interval > 0.0f)
	{
		AutoChangeCountdown -= DeltaSeconds;
		if (AutoChangeCountdown <= 0.0f)
		{
			AutoChangeCountdown = Interval;
			RandomizeWeather(-1.0f);
		}
	}

	ApplyStateToMPC();
}

void UTODWeatherSubsystem::ApplyStateToMPC()
{
	// Lazy-load once; the asset lives in the project (created in the editor).
	if (!WeatherCollection)
	{
		WeatherCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/TOD/TOD_WeatherMPC.TOD_WeatherMPC"));
	}
	if (!WeatherCollection)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(World, WeatherCollection, TEXT("Wetness"), Current.Wetness);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WeatherCollection, TEXT("SnowAmount"), Current.SnowAmount);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WeatherCollection, TEXT("CloudCoverage"), Current.CloudCoverage);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WeatherCollection, TEXT("WindStrength"), Current.WindStrength);
		UKismetMaterialLibrary::SetScalarParameterValue(World, WeatherCollection, TEXT("FogDensity"), Current.FogDensity);
	}
}

UTODWeatherSubsystem* UTODWeatherSubsystem::GetForConsole()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				if (UTODWeatherSubsystem* Subsystem = World->GetSubsystem<UTODWeatherSubsystem>())
				{
					return Subsystem;
				}
			}
		}
	}
	return nullptr;
}

void UTODWeatherSubsystem::HandleConsoleForceWeather(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTODWeather, Warning, TEXT("Usage: tod.ForceWeather <PresetName> [transitionSeconds]"));
		return;
	}
	UTODWeatherSubsystem* Subsystem = GetForConsole();
	if (!Subsystem)
	{
		UE_LOG(LogTODWeather, Warning, TEXT("tod.ForceWeather: no game world with TODWeatherSubsystem is running."));
		return;
	}
	const float Transition = Args.Num() > 1 ? FCString::Atof(*Args[1]) : -1.0f;
	Subsystem->ForceWeather(FName(*Args[0]), Transition);
}

void UTODWeatherSubsystem::HandleConsoleWeatherInfo()
{
	const UTODWeatherSubsystem* Subsystem = GetForConsole();
	if (!Subsystem)
	{
		UE_LOG(LogTODWeather, Warning, TEXT("tod.WeatherInfo: no game world with TODWeatherSubsystem is running."));
		return;
	}
	const FTODWeatherState& S = Subsystem->GetCurrentState();
	UE_LOG(LogTODWeather, Log, TEXT("Weather '%s': cloud=%.2f fog=%.5f wind=%.2f wet=%.2f snow=%.2f sunScale=%.2f skyScale=%.2f lightning=%d"),
		*S.CurrentPreset.ToString(), S.CloudCoverage, S.FogDensity, S.WindStrength, S.Wetness, S.SnowAmount, S.SunIntensityScale, S.SkyLightScale, S.bLightning ? 1 : 0);
}
