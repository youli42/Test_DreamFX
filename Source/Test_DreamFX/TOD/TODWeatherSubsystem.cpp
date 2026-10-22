// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODWeatherSubsystem.h"

#include "TODSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/Package.h"

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

namespace
{
	float LerpFloat(float From, float To, float Alpha)
	{
		return FMath::Lerp(From, To, Alpha);
	}

	FTODWeatherProfileData MakeFallbackProfile(FName InName, float InWeight, float Cloud, float Fog, float Wind,
		float Wet, float SnowGround, float Sun, float Moon, float Sky, float Rain, float Snow, bool bLightning)
	{
		FTODWeatherProfileData Data;
		Data.DataVersion = 1;
		Data.Identity.Name = InName;
		Data.Identity.ScheduleWeight = InWeight;
		Data.Identity.bLightning = bLightning;

		Data.Surface.bEnabled = true;
		Data.Surface.Wetness = FTODWeatherCurve::Constant(Wet);
		Data.Surface.SnowAmount = FTODWeatherCurve::Constant(SnowGround);

		Data.Fog.bEnabled = Fog > UE_KINDA_SMALL_NUMBER;
		Data.Fog.Density = FTODWeatherCurve::Constant(Fog);

		Data.DirectionalLight.bEnabled = true;
		Data.DirectionalLight.SunIntensityScale = FTODWeatherCurve::Constant(Sun);
		Data.DirectionalLight.MoonIntensityScale = FTODWeatherCurve::Constant(Moon);

		Data.SkyLight.bEnabled = true;
		Data.SkyLight.IntensityScale = FTODWeatherCurve::Constant(Sky);

		Data.Wind.bEnabled = true;
		Data.Wind.Strength = FTODWeatherCurve::Constant(Wind);

		Data.CloudLayer.bEnabled = true;
		Data.CloudLayer.Coverage = FTODWeatherCurve::Constant(Cloud);

		if (Rain > UE_KINDA_SMALL_NUMBER)
		{
			Data.WeatherEffects.bEnabled = true;
			FTODWeatherEffect& Effect = Data.WeatherEffects.Effects.Add(TEXT("Rain"));
			Effect.System = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TODWeatherDefaults::RainSystemPath));
			Effect.Intensity = FTODWeatherCurve::Constant(Rain);
			Effect.RelativeLocation = FVector(0.0f, 0.0f, 1200.0f);
		}
		if (Snow > UE_KINDA_SMALL_NUMBER)
		{
			Data.WeatherEffects.bEnabled = true;
			FTODWeatherEffect& Effect = Data.WeatherEffects.Effects.Add(TEXT("Snow"));
			Effect.System = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TODWeatherDefaults::SnowSystemPath));
			Effect.Intensity = FTODWeatherCurve::Constant(Snow);
			Effect.RelativeLocation = FVector(0.0f, 0.0f, 1200.0f);
		}

		return Data;
	}
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

	// One weather = one UTODWeatherProfile asset under /Game/TOD/Profiles.
	// Code-built fallbacks only kick in when no asset was authored yet.
	if (!DiscoverProfileAssets())
	{
		UE_LOG(LogTODWeather, Warning, TEXT("No UTODWeatherProfile assets under /Game/TOD/Profiles - falling back to built-in curve profiles."));
		RegisterFallbackProfiles();
	}

	FName StartName = DefaultWeatherName;
	if (!Profiles.Contains(StartName))
	{
		for (const TPair<FName, FTODWeatherProfileData>& Pair : Profiles)
		{
			StartName = Pair.Key;
			break;
		}
	}
	if (const FTODWeatherProfileData* Found = Profiles.Find(StartName))
	{
		TargetData = *Found;
	}
	else
	{
		UE_LOG(LogTODWeather, Warning, TEXT("Weather subsystem has no registered presets; the sky stays neutral."));
	}

	Current = EvaluateProfileAt(TargetData, GetDayProgress());
	UE_LOG(LogTODWeather, Log, TEXT("TODWeatherSubsystem initialized. %d weather files, starting '%s' (day progress %.2f)."),
		Profiles.Num(), *Current.CurrentPreset.ToString(), GetDayProgress());
}

void UTODWeatherSubsystem::Deinitialize()
{
	UE_LOG(LogTODWeather, Log, TEXT("TODWeatherSubsystem deinitialized at '%s'."), *Current.CurrentPreset.ToString());
	Super::Deinitialize();
}

void UTODWeatherSubsystem::RegisterFallbackProfiles()
{
	auto Register = [this](const FTODWeatherProfileData& Data)
	{
		Profiles.Add(Data.Identity.Name, Data);
	};

	Register(MakeFallbackProfile(TEXT("ClearSky"), 3.0f, 0.15f, 0.0002f, 0.25f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, false));
	Register(MakeFallbackProfile(TEXT("PartlyCloudy"), 3.0f, 0.45f, 0.0003f, 0.45f, 0.0f, 0.0f, 0.85f, 1.0f, 0.9f, 0.0f, 0.0f, false));
	Register(MakeFallbackProfile(TEXT("Overcast"), 2.0f, 0.9f, 0.0006f, 0.6f, 0.0f, 0.0f, 0.35f, 0.8f, 0.7f, 0.0f, 0.0f, false));
	Register(MakeFallbackProfile(TEXT("Foggy"), 1.0f, 0.75f, 0.0080f, 0.15f, 0.0f, 0.0f, 0.4f, 0.7f, 0.65f, 0.0f, 0.0f, false));
	Register(MakeFallbackProfile(TEXT("LightRain"), 1.5f, 0.85f, 0.0012f, 0.7f, 0.7f, 0.0f, 0.3f, 0.5f, 0.6f, 0.5f, 0.0f, false));
	Register(MakeFallbackProfile(TEXT("Thunderstorm"), 1.0f, 1.0f, 0.0018f, 1.0f, 1.0f, 0.0f, 0.18f, 0.4f, 0.5f, 1.0f, 0.0f, true));
	Register(MakeFallbackProfile(TEXT("LightSnow"), 1.0f, 0.8f, 0.0010f, 0.3f, 0.0f, 0.5f, 0.45f, 0.6f, 0.75f, 0.0f, 0.45f, false));
	Register(MakeFallbackProfile(TEXT("Snow"), 1.0f, 0.95f, 0.0015f, 0.45f, 0.0f, 1.0f, 0.3f, 0.5f, 0.7f, 0.0f, 1.0f, false));
}

bool UTODWeatherSubsystem::DiscoverProfileAssets()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UTODWeatherProfile::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game/TOD/Profiles"));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> FoundAssets;
	AssetRegistryModule.Get().GetAssets(Filter, FoundAssets);

	int32 Registered = 0;
	for (const FAssetData& AssetData : FoundAssets)
	{
		UTODWeatherProfile* Profile = Cast<UTODWeatherProfile>(AssetData.GetAsset());
		if (!Profile)
		{
			continue;
		}
		if (Profile->Data.Identity.Name.IsNone())
		{
			UE_LOG(LogTODWeather, Warning, TEXT("Weather profile asset %s has no Identity.Name - skipped."), *Profile->GetPathName());
			continue;
		}
		Profiles.Add(Profile->Data.Identity.Name, Profile->Data);
		ProfileAssets.Add(Profile->Data.Identity.Name, Profile);
		++Registered;
	}

	if (Registered > 0)
	{
		UE_LOG(LogTODWeather, Log, TEXT("Discovered %d weather file(s) under /Game/TOD/Profiles."), Registered);
	}
	return Registered > 0;
}

void UTODWeatherSubsystem::RegisterProfileFromData(const FTODWeatherProfileData& NewData)
{
	if (NewData.Identity.Name.IsNone())
	{
		UE_LOG(LogTODWeather, Warning, TEXT("RegisterProfileFromData refused a profile with no Identity.Name."));
		return;
	}

	FTODWeatherProfileData Data = NewData;
	if (Data.DataVersion < 1)
	{
		Data.MigrateLegacyCore();
	}

	Profiles.Add(Data.Identity.Name, Data);
	UE_LOG(LogTODWeather, Log, TEXT("Weather file '%s' registered from code."), *Data.Identity.Name.ToString());
}

bool UTODWeatherSubsystem::GetProfileData(FName PresetName, FTODWeatherProfileData& OutData) const
{
	if (const FTODWeatherProfileData* Found = Profiles.Find(PresetName))
	{
		OutData = *Found;
		return true;
	}
	return false;
}

TArray<FName> UTODWeatherSubsystem::GetRegisteredPresetNames() const
{
	TArray<FName> Names;
	Names.Reserve(Profiles.Num());
	for (const TPair<FName, FTODWeatherProfileData>& Pair : Profiles)
	{
		Names.Add(Pair.Key);
	}
	return Names;
}

float UTODWeatherSubsystem::GetDayProgress() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UTODSubsystem* TOD = World->GetSubsystem<UTODSubsystem>())
		{
			return FMath::Frac(FMath::Max(TOD->GetTimeOfDayHours(), 0.0f) / 24.0f);
		}
	}
	// No clock: evaluate the files at noon so the curves are visible.
	return 0.5f;
}

FTODWeatherState UTODWeatherSubsystem::EvaluateProfileAt(const FTODWeatherProfileData& Data, float DayProgress)
{
	const float T = FMath::Clamp(DayProgress, 0.0f, 1.0f);
	FTODWeatherState State;

	State.CurrentPreset = Data.Identity.Name;
	State.bLightning = Data.Identity.bLightning;

	// Ground state.
	State.bSurfaceEnabled = Data.Surface.bEnabled;
	State.Wetness = Data.Surface.Wetness.Evaluate(T, 0.0f);
	State.SnowAmount = Data.Surface.SnowAmount.Evaluate(T, 0.0f);

	// Height fog.
	State.bFogEnabled = Data.Fog.bEnabled;
	State.FogDensity = Data.Fog.Density.Evaluate(T, 0.0003f);
	State.bHasFogInscatteringColor = Data.Fog.InscatteringColor.HasKeys();
	State.FogInscatteringColor = Data.Fog.InscatteringColor.Evaluate(T, FLinearColor(0.62f, 0.70f, 0.80f, 1.0f));
	State.FogStartDistance = Data.Fog.StartDistance.Evaluate(T, 0.0f);
	State.FogEndDistance = Data.Fog.EndDistance.Evaluate(T, 0.0f);
	State.FogHeightFalloff = Data.Fog.HeightFalloff.Evaluate(T, 0.2f);
	State.SecondFogDensity = Data.Fog.SecondFogDensity.Evaluate(T, 0.0f);
	State.SecondFogHeightOffset = Data.Fog.SecondFogHeightOffset.Evaluate(T, 0.0f);
	State.SecondFogHeightFalloff = Data.Fog.SecondFogHeightFalloff.Evaluate(T, 0.2f);

	// Sun / moon / ambient.
	State.bLightEnabled = Data.DirectionalLight.bEnabled;
	State.SunIntensityScale = Data.DirectionalLight.SunIntensityScale.Evaluate(T, 1.0f);
	State.MoonIntensityScale = Data.DirectionalLight.MoonIntensityScale.Evaluate(T, 1.0f);
	State.bHasSunColor = Data.DirectionalLight.SunColor.HasKeys();
	State.SunLightColor = Data.DirectionalLight.SunColor.Evaluate(T, FLinearColor::White);
	State.bHasMoonColor = Data.DirectionalLight.MoonColor.HasKeys();
	State.MoonLightColor = Data.DirectionalLight.MoonColor.Evaluate(T, FLinearColor::White);
	State.SunTemperature = Data.DirectionalLight.SunTemperature.Evaluate(T, 0.0f);
	State.SunVolumetricScattering = Data.DirectionalLight.SunVolumetricScattering.Evaluate(T, 1.0f);
	State.MoonVolumetricScattering = Data.DirectionalLight.MoonVolumetricScattering.Evaluate(T, 1.0f);
	State.SunBloomScale = Data.DirectionalLight.SunBloomScale.Evaluate(T, 1.0f);

	State.bSkyLightEnabled = Data.SkyLight.bEnabled;
	State.SkyLightScale = Data.SkyLight.IntensityScale.Evaluate(T, 1.0f);
	State.bHasSkyLightColor = Data.SkyLight.LightColor.HasKeys();
	State.SkyLightColor = Data.SkyLight.LightColor.Evaluate(T, FLinearColor::White);
	State.SkyLightVolumetricScattering = Data.SkyLight.VolumetricScattering.Evaluate(T, 1.0f);

	State.bVolumetricFogEnabled = Data.VolumetricFog.bEnabled;
	State.bHasVolumetricFogAlbedo = Data.VolumetricFog.Albedo.HasKeys();
	State.VolumetricFogAlbedo = Data.VolumetricFog.Albedo.Evaluate(T, FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
	State.VolumetricFogScatteringDistribution = Data.VolumetricFog.ScatteringDistribution.Evaluate(T, 0.5f);
	State.VolumetricFogExtinctionScale = Data.VolumetricFog.ExtinctionScale.Evaluate(T, 1.0f);
	State.VolumetricFogViewDistance = Data.VolumetricFog.ViewDistance.Evaluate(T, 6000.0f);
	State.VolumetricFogStartDistance = Data.VolumetricFog.StartDistance.Evaluate(T, 0.0f);

	State.bWindEnabled = Data.Wind.bEnabled;
	State.WindStrength = Data.Wind.Strength.Evaluate(T, 0.0f);

	State.bAtmosphereEnabled = Data.Atmosphere.bEnabled;
	State.RayleighScatteringScale = Data.Atmosphere.RayleighScatteringScale.Evaluate(T, 1.0f);
	State.MieScatteringScale = Data.Atmosphere.MieScatteringScale.Evaluate(T, 1.0f);
	State.MieAnisotropy = Data.Atmosphere.MieAnisotropy.Evaluate(T, 0.8f);
	State.MieExponentialDistribution = Data.Atmosphere.MieExponentialDistribution.Evaluate(T, 0.2f);
	State.MultiScatteringFactor = Data.Atmosphere.MultiScatteringFactor.Evaluate(T, 1.0f);
	State.HeightFogContribution = Data.Atmosphere.HeightFogContribution.Evaluate(T, 1.0f);

	State.bCloudEnabled = Data.CloudLayer.bEnabled;
	State.CloudCoverage = Data.CloudLayer.Coverage.Evaluate(T, 0.2f);
	State.CloudLayerBottomKm = Data.CloudLayer.LayerBottomKm.Evaluate(T, 1.5f);
	State.CloudLayerHeightKm = Data.CloudLayer.LayerHeightKm.Evaluate(T, 4.0f);

	State.bPostProcessEnabled = Data.PostProcess.bEnabled;
	State.PPExposureBias = Data.PostProcess.ExposureBias.Evaluate(T, 1.0f);
	State.PPExposureMinBrightness = Data.PostProcess.ExposureMinBrightness.Evaluate(T, -1.0f);
	State.PPExposureMaxBrightness = Data.PostProcess.ExposureMaxBrightness.Evaluate(T, 3.0f);
	State.PPSaturation = Data.PostProcess.Saturation.Evaluate(T, 1.0f);
	State.PPContrast = Data.PostProcess.Contrast.Evaluate(T, 1.0f);
	State.PPVignetteIntensity = Data.PostProcess.VignetteIntensity.Evaluate(T, 0.0f);
	State.PPBloomIntensity = Data.PostProcess.BloomIntensity.Evaluate(T, 0.675f);
	State.PPFilmGrainIntensity = Data.PostProcess.FilmGrainIntensity.Evaluate(T, 0.0f);
	State.PPIndirectLightingIntensity = Data.PostProcess.IndirectLightingIntensity.Evaluate(T, 1.0f);
	State.bHasPPIndirectLightingColor = Data.PostProcess.IndirectLightingColor.HasKeys();
	State.PPIndirectLightingColor = Data.PostProcess.IndirectLightingColor.Evaluate(T, FLinearColor::White);

	State.bEffectsEnabled = Data.WeatherEffects.bEnabled;
	if (State.bEffectsEnabled)
	{
		for (const TPair<FName, FTODWeatherEffect>& Pair : Data.WeatherEffects.Effects)
		{
			const FTODWeatherEffect& Effect = Pair.Value;
			if (!Effect.bEnabled || Effect.System.IsNull())
			{
				continue;
			}
			FTODWeatherEffectState& Entry = State.Effects.Add(Pair.Key);
			Entry.System = Effect.System;
			Entry.ParameterName = Effect.ParameterName;
			Entry.Intensity = Effect.Intensity.Evaluate(T, 0.0f);
			Entry.Scale = Effect.Scale;
			Entry.RelativeLocation = Effect.RelativeLocation;
			Entry.RelativeRotation = Effect.RelativeRotation;
		}
	}

	return State;
}

FTODWeatherState UTODWeatherSubsystem::BlendStates(const FTODWeatherState& From, const FTODWeatherState& To,
	float Alpha, float SurfaceAlpha)
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float SA = FMath::Clamp(SurfaceAlpha, 0.0f, 1.0f);

	FTODWeatherState State = To;

	State.Wetness = LerpFloat(From.Wetness, To.Wetness, SA);
	State.SnowAmount = LerpFloat(From.SnowAmount, To.SnowAmount, SA);

	State.FogDensity = LerpFloat(From.FogDensity, To.FogDensity, A);
	State.FogInscatteringColor = FMath::Lerp(From.FogInscatteringColor, To.FogInscatteringColor, A);
	State.FogStartDistance = LerpFloat(From.FogStartDistance, To.FogStartDistance, A);
	State.FogEndDistance = LerpFloat(From.FogEndDistance, To.FogEndDistance, A);
	State.FogHeightFalloff = LerpFloat(From.FogHeightFalloff, To.FogHeightFalloff, A);
	State.SecondFogDensity = LerpFloat(From.SecondFogDensity, To.SecondFogDensity, A);
	State.SecondFogHeightOffset = LerpFloat(From.SecondFogHeightOffset, To.SecondFogHeightOffset, A);
	State.SecondFogHeightFalloff = LerpFloat(From.SecondFogHeightFalloff, To.SecondFogHeightFalloff, A);

	State.SunIntensityScale = LerpFloat(From.SunIntensityScale, To.SunIntensityScale, A);
	State.MoonIntensityScale = LerpFloat(From.MoonIntensityScale, To.MoonIntensityScale, A);
	State.SunLightColor = FMath::Lerp(From.SunLightColor, To.SunLightColor, A);
	State.MoonLightColor = FMath::Lerp(From.MoonLightColor, To.MoonLightColor, A);
	State.SunTemperature = LerpFloat(From.SunTemperature, To.SunTemperature, A);
	State.SunVolumetricScattering = LerpFloat(From.SunVolumetricScattering, To.SunVolumetricScattering, A);
	State.MoonVolumetricScattering = LerpFloat(From.MoonVolumetricScattering, To.MoonVolumetricScattering, A);
	State.SunBloomScale = LerpFloat(From.SunBloomScale, To.SunBloomScale, A);

	State.SkyLightScale = LerpFloat(From.SkyLightScale, To.SkyLightScale, A);
	State.SkyLightColor = FMath::Lerp(From.SkyLightColor, To.SkyLightColor, A);
	State.SkyLightVolumetricScattering = LerpFloat(From.SkyLightVolumetricScattering, To.SkyLightVolumetricScattering, A);

	State.VolumetricFogAlbedo = FMath::Lerp(From.VolumetricFogAlbedo, To.VolumetricFogAlbedo, A);
	State.VolumetricFogScatteringDistribution = LerpFloat(From.VolumetricFogScatteringDistribution, To.VolumetricFogScatteringDistribution, A);
	State.VolumetricFogExtinctionScale = LerpFloat(From.VolumetricFogExtinctionScale, To.VolumetricFogExtinctionScale, A);
	State.VolumetricFogViewDistance = LerpFloat(From.VolumetricFogViewDistance, To.VolumetricFogViewDistance, A);
	State.VolumetricFogStartDistance = LerpFloat(From.VolumetricFogStartDistance, To.VolumetricFogStartDistance, A);

	State.WindStrength = LerpFloat(From.WindStrength, To.WindStrength, A);

	State.RayleighScatteringScale = LerpFloat(From.RayleighScatteringScale, To.RayleighScatteringScale, A);
	State.MieScatteringScale = LerpFloat(From.MieScatteringScale, To.MieScatteringScale, A);
	State.MieAnisotropy = LerpFloat(From.MieAnisotropy, To.MieAnisotropy, A);
	State.MieExponentialDistribution = LerpFloat(From.MieExponentialDistribution, To.MieExponentialDistribution, A);
	State.MultiScatteringFactor = LerpFloat(From.MultiScatteringFactor, To.MultiScatteringFactor, A);
	State.HeightFogContribution = LerpFloat(From.HeightFogContribution, To.HeightFogContribution, A);

	State.CloudCoverage = LerpFloat(From.CloudCoverage, To.CloudCoverage, A);
	State.CloudLayerBottomKm = LerpFloat(From.CloudLayerBottomKm, To.CloudLayerBottomKm, A);
	State.CloudLayerHeightKm = LerpFloat(From.CloudLayerHeightKm, To.CloudLayerHeightKm, A);

	State.PPExposureBias = LerpFloat(From.PPExposureBias, To.PPExposureBias, A);
	State.PPExposureMinBrightness = LerpFloat(From.PPExposureMinBrightness, To.PPExposureMinBrightness, A);
	State.PPExposureMaxBrightness = LerpFloat(From.PPExposureMaxBrightness, To.PPExposureMaxBrightness, A);
	State.PPSaturation = LerpFloat(From.PPSaturation, To.PPSaturation, A);
	State.PPContrast = LerpFloat(From.PPContrast, To.PPContrast, A);
	State.PPVignetteIntensity = LerpFloat(From.PPVignetteIntensity, To.PPVignetteIntensity, A);
	State.PPBloomIntensity = LerpFloat(From.PPBloomIntensity, To.PPBloomIntensity, A);
	State.PPFilmGrainIntensity = LerpFloat(From.PPFilmGrainIntensity, To.PPFilmGrainIntensity, A);
	State.PPIndirectLightingIntensity = LerpFloat(From.PPIndirectLightingIntensity, To.PPIndirectLightingIntensity, A);
	State.PPIndirectLightingColor = FMath::Lerp(From.PPIndirectLightingColor, To.PPIndirectLightingColor, A);

	// Effect table: union of both files, intensities cross-faded, descriptor taken from the target.
	State.Effects.Reset();
	for (const TPair<FName, FTODWeatherEffectState>& Pair : To.Effects)
	{
		FTODWeatherEffectState Entry = Pair.Value;
		const FTODWeatherEffectState* FromEntry = From.Effects.Find(Pair.Key);
		Entry.Intensity = LerpFloat(FromEntry ? FromEntry->Intensity : 0.0f, Entry.Intensity, A);
		State.Effects.Add(Pair.Key, Entry);
	}
	for (const TPair<FName, FTODWeatherEffectState>& Pair : From.Effects)
	{
		if (To.Effects.Contains(Pair.Key))
		{
			continue;
		}
		FTODWeatherEffectState Entry = Pair.Value;
		Entry.Intensity = Entry.Intensity * (1.0f - A);
		State.Effects.Add(Pair.Key, Entry);
	}

	return State;
}

bool UTODWeatherSubsystem::ForceWeather(FName PresetName, float InTransitionSeconds)
{
	const FTODWeatherProfileData* Found = Profiles.Find(PresetName);
	if (!Found)
	{
		FString RegisteredNames;
		for (const FName& Name : GetRegisteredPresetNames())
		{
			RegisteredNames += Name.ToString();
			RegisteredNames += TEXT(", ");
		}
		UE_LOG(LogTODWeather, Warning, TEXT("ForceWeather: unknown weather file '%s'. Registered: %s"),
			*PresetName.ToString(), *RegisteredNames);
		return false;
	}

	FromData = TargetData;
	if (FromData.Identity.Name.IsNone())
	{
		FromData = *Found;
	}
	TargetData = *Found;

	ActiveTransitionSeconds = (InTransitionSeconds >= 0.0f) ? InTransitionSeconds : FMath::Max(TransitionSeconds, 0.1f);
	ActiveSurfaceSeconds = FMath::Max(TargetData.Identity.SurfaceTransitionSeconds, 0.1f);
	TransitionElapsed = 0.0f;
	bTransitioning = true;

	UE_LOG(LogTODWeather, Log, TEXT("Weather '%s' -> '%s' (sky %.1fs, ground %.1fs)."),
		*FromData.Identity.Name.ToString(), *PresetName.ToString(), ActiveTransitionSeconds, ActiveSurfaceSeconds);
	return true;
}

void UTODWeatherSubsystem::RandomizeWeather(float InTransitionSeconds)
{
	if (Profiles.Num() == 0)
	{
		return;
	}
	float TotalWeight = 0.0f;
	for (const TPair<FName, FTODWeatherProfileData>& Pair : Profiles)
	{
		TotalWeight += FMath::Max(Pair.Value.Identity.ScheduleWeight, 0.0f);
	}
	if (TotalWeight <= 0.0f)
	{
		return;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const TPair<FName, FTODWeatherProfileData>& Pair : Profiles)
	{
		Roll -= FMath::Max(Pair.Value.Identity.ScheduleWeight, 0.0f);
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

	const float DayProgress = GetDayProgress();

	if (bTransitioning)
	{
		const float LongestSeconds = FMath::Max(ActiveTransitionSeconds, ActiveSurfaceSeconds);
		TransitionElapsed = FMath::Min(TransitionElapsed + DeltaSeconds, LongestSeconds);

		const float SkyAlpha = FMath::InterpEaseInOut(0.0f, 1.0f,
			FMath::Clamp(TransitionElapsed / FMath::Max(ActiveTransitionSeconds, 0.1f), 0.0f, 1.0f), 2.0f);
		const float SurfaceAlpha = FMath::InterpEaseInOut(0.0f, 1.0f,
			FMath::Clamp(TransitionElapsed / FMath::Max(ActiveSurfaceSeconds, 0.1f), 0.0f, 1.0f), 2.0f);

		Current = BlendStates(
			EvaluateProfileAt(FromData, DayProgress),
			EvaluateProfileAt(TargetData, DayProgress),
			SkyAlpha, SurfaceAlpha);

		if (TransitionElapsed >= LongestSeconds)
		{
			bTransitioning = false;
			UE_LOG(LogTODWeather, Log, TEXT("Weather transition to '%s' complete."), *TargetData.Identity.Name.ToString());
		}
	}
	else
	{
		// The active file is re-evaluated every frame: the 24h curves animate the
		// weather even while the weather itself is "stable".
		Current = EvaluateProfileAt(TargetData, DayProgress);
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
		WeatherCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/TOD/MPC/TOD_WeatherMPC.TOD_WeatherMPC"));
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

FTODWeatherSnapshot UTODWeatherSubsystem::GetWeatherSnapshot() const
{
	FTODWeatherSnapshot Snapshot;
	Snapshot.PresetName = TargetData.Identity.Name;
	return Snapshot;
}

bool UTODWeatherSubsystem::ApplyWeatherSnapshot(const FTODWeatherSnapshot& Snapshot, bool bInstant)
{
	if (!bInstant)
	{
		return ForceWeather(Snapshot.PresetName, -1.0f);
	}

	const FTODWeatherProfileData* Found = Profiles.Find(Snapshot.PresetName);
	if (!Found)
	{
		UE_LOG(LogTODWeather, Warning, TEXT("ApplyWeatherSnapshot: unknown weather file '%s'."), *Snapshot.PresetName.ToString());
		return false;
	}

	TargetData = *Found;
	FromData = TargetData;
	bTransitioning = false;
	Current = EvaluateProfileAt(TargetData, GetDayProgress());
	ApplyStateToMPC();

	UE_LOG(LogTODWeather, Log, TEXT("Weather snapshot applied instantly: '%s'."), *Found->Identity.Name.ToString());
	return true;
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
	UE_LOG(LogTODWeather, Log, TEXT("Weather '%s' @day %.2f: cloud=%.2f fog=%.5f wind=%.2f wet=%.2f snow=%.2f sun=%.2f moon=%.2f sky=%.2f lightning=%d effects=%d"),
		*S.CurrentPreset.ToString(), Subsystem->GetDayProgress(), S.CloudCoverage, S.FogDensity, S.WindStrength,
		S.Wetness, S.SnowAmount, S.SunIntensityScale, S.MoonIntensityScale, S.SkyLightScale,
		S.bLightning ? 1 : 0, S.Effects.Num());
	for (const TPair<FName, FTODWeatherEffectState>& Pair : S.Effects)
	{
		UE_LOG(LogTODWeather, Log, TEXT("  effect '%s': %.2f -> '%s'"), *Pair.Key.ToString(), Pair.Value.Intensity,
			*Pair.Value.System.ToSoftObjectPath().ToString());
	}
}
