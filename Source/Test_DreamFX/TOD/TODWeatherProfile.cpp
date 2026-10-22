// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODWeatherProfile.h"

#include "UObject/ObjectMacros.h"

namespace
{
	int32 SeedFor(FName WeatherName, int32 Offset)
	{
		return static_cast<int32>(GetTypeHash(WeatherName)) + Offset * 7919;
	}
}

void FTODWeatherProfileData::MigrateLegacyCore()
{
	if (Core.Name.IsNone())
	{
		// Fresh asset or already migrated: just stamp the version.
		DataVersion = FMath::Max(DataVersion, 1);
		return;
	}

	// Identity / scheduling.
	Identity.Name = Core.Name;
	Identity.ScheduleWeight = FMath::Max(Core.ScheduleWeight, 0.0f);
	Identity.bLightning = Core.bLightning;

	const int32 Seed = SeedFor(Core.Name, 1);

	// Ground state.
	Surface.bEnabled = true;
	Surface.Wetness = FTODWeatherCurve::RandomAround(Core.Wetness, Seed + 10);
	Surface.SnowAmount = FTODWeatherCurve::RandomAround(Core.SnowAmount, Seed + 11);

	// Height fog: only authored when the old preset actually had fog.
	if (Core.FogDensity > UE_KINDA_SMALL_NUMBER)
	{
		Fog.bEnabled = true;
		Fog.Density = FTODWeatherCurve::RandomAround(Core.FogDensity, Seed + 20, 0.25f);
		Fog.InscatteringColor = FTODWeatherColorCurve::RandomAround(
			FLinearColor(0.62f, 0.70f, 0.80f, 1.0f), Seed + 21, 0.10f);
	}

	// Sun/moon + sky ambient scales (colour is left to the time-of-day rig).
	DirectionalLight.bEnabled = true;
	DirectionalLight.SunIntensityScale = FTODWeatherCurve::RandomAround(Core.SunIntensityScale, Seed + 30, 0.20f);
	DirectionalLight.MoonIntensityScale = FTODWeatherCurve::RandomAround(Core.MoonIntensityScale, Seed + 31, 0.25f);

	SkyLight.bEnabled = true;
	SkyLight.IntensityScale = FTODWeatherCurve::RandomAround(Core.SkyLightScale, Seed + 40, 0.15f);

	Wind.bEnabled = true;
	Wind.Strength = FTODWeatherCurve::RandomAround(Core.WindStrength, Seed + 50, 0.30f);

	CloudLayer.bEnabled = true;
	CloudLayer.Coverage = FTODWeatherCurve::RandomAround(Core.CloudCoverage, Seed + 60, 0.20f);

	// Rain/snow become rows of the weather-effect intensity table instead of
	// dedicated fields, so the sky rig drives any particle system uniformly.
	if (Core.RainIntensity > UE_KINDA_SMALL_NUMBER)
	{
		WeatherEffects.bEnabled = true;
		FTODWeatherEffect& Rain = WeatherEffects.Effects.Add(TEXT("Rain"));
		Rain.System = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TODWeatherDefaults::RainSystemPath));
		Rain.Intensity = FTODWeatherCurve::RandomAround(Core.RainIntensity, Seed + 70, 0.25f);
		Rain.RelativeLocation = FVector(0.0f, 0.0f, 1200.0f);
	}

	if (Core.SnowIntensity > UE_KINDA_SMALL_NUMBER)
	{
		WeatherEffects.bEnabled = true;
		FTODWeatherEffect& Snow = WeatherEffects.Effects.Add(TEXT("Snow"));
		Snow.System = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TODWeatherDefaults::SnowSystemPath));
		Snow.Intensity = FTODWeatherCurve::RandomAround(Core.SnowIntensity, Seed + 71, 0.25f);
		Snow.RelativeLocation = FVector(0.0f, 0.0f, 1200.0f);
	}

	// The legacy tag is cleared so a re-save drops it and stops re-migrating.
	Core = FTODWeatherPreset();
	DataVersion = 1;
}

void UTODWeatherProfile::PostLoad()
{
	Super::PostLoad();

	if (Data.DataVersion < 1)
	{
		Data.MigrateLegacyCore();
	}
}
