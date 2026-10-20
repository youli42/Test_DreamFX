// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODSkyManager.h"

#include "TODSubsystem.h"
#include "TODWeatherSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTODSky, Log, All);

namespace
{
	/** Engine default raymarched cloud material (UE 5.8); may be missing in stripped builds. */
	constexpr TCHAR VolumetricCloudMaterialPath[] =
		TEXT("/Engine/EngineSky/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst");
}

ATODSkyManager::ATODSkyManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(Root);
	SunLight->bAtmosphereSunLight = true;
	SunLight->AtmosphereSunLightIndex = 0;
	SunLight->bCastCloudShadows = true;
	SunLight->Intensity = 8.0f;

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(Root);
	MoonLight->bAtmosphereSunLight = true;
	MoonLight->AtmosphereSunLightIndex = 1;
	MoonLight->bCastCloudShadows = true;
	MoonLight->Intensity = 0.2f;

	// Static CDO pose at the default clock (9h), consistent with the daily orbit model.
	{
		float InitSunElev = 0.f, InitMoonElev = 0.f, InitSunAz = 0.f, InitMoonAz = 0.f;
		ComputeSunMoonState(9.0f, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees,
			InitSunElev, InitMoonElev, InitSunAz, InitMoonAz);
		SunLight->SetRelativeRotation(FRotator(-InitSunElev * MaxCelestialPitch, InitSunAz, 0.0f));
		MoonLight->SetRelativeRotation(FRotator(-InitMoonElev * MaxCelestialPitch, InitMoonAz, 0.0f));
	}
	// Editor-state policy: only ONE celestial light is ever visible. UpdateCelestials()
	// toggles visibility exclusively (sun XOR moon) while running; hiding the moon here
	// guarantees no dual-primary-light contest in the un-ticked editor world either.
	MoonLight->SetVisibility(false);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = 1.0f;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	VolumetricClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricClouds"));
	VolumetricClouds->SetupAttachment(Root);
	// Altitudes are in kilometers for the engine cloud layer.
	VolumetricClouds->LayerBottomAltitude = 1.5f;
	VolumetricClouds->LayerHeight = 4.0f;
	VolumetricClouds->SetVisibility(bEnableVolumetricCloud);

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(Root);
	HeightFog->FogDensity = 0.0003f;
	HeightFog->bEnableVolumetricFog = true;
}

void ATODSkyManager::BeginPlay()
{
	Super::BeginPlay();

	ConfigureSky();
	if (UTODSubsystem* TOD = GetTOD())
	{
		UE_LOG(LogTODSky, Log, TEXT("TODSkyManager online. Clock %.2fh, scale %.3fh/s."),
			TOD->GetTimeOfDayHours(), TOD->GetTimeScale());
	}
}

void ATODSkyManager::ConfigureSky()
{
	if (bSkyConfigured)
	{
		return;
	}
	bSkyConfigured = true;

	if (bEnableVolumetricCloud && VolumetricClouds)
	{
		// The engine component ships a default cloud material; only patch in our own
		// when that default is missing (e.g. stripped builds).
		if (!VolumetricClouds->GetMaterial())
		{
			if (UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr, VolumetricCloudMaterialPath))
			{
				VolumetricClouds->SetMaterial(CloudMaterial);
			}
			else
			{
				UE_LOG(LogTODSky, Warning, TEXT("Volumetric cloud material '%s' not found and the component has no default; clouds disabled."), VolumetricCloudMaterialPath);
				VolumetricClouds->SetVisibility(false);
			}
		}
	}

	if (SkyLight)
	{
		SkyLight->bRealTimeCapture = bSkyLightRealTimeCapture;
		SkyLight->bLowerHemisphereIsBlack = true;
	}
}

void ATODSkyManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UTODSubsystem* TOD = GetTOD();
	if (!TOD)
	{
		return;
	}

	if (bAutoAdvanceTime)
	{
		TOD->AdvanceTime(DeltaSeconds);
	}

	// Weather blending shares the same driver source as the clock.
	if (UTODWeatherSubsystem* Weather = GetTODWeather())
	{
		Weather->TickWeather(DeltaSeconds);
	}

	UpdateCelestials();
	UpdateWeatherApplication();
}

void ATODSkyManager::ComputeElevations(float TimeHours, float& OutSunElev, float& OutMoonElev)
{
	// sin((t-6)/12 * 2π) per hemisphere: 0 at 6h, +1 at noon, 0 at 18h, -1 at midnight.
	const float SunPhase = FMath::DegreesToRadians((TimeHours - 6.0f) * (360.0f / 24.0f));
	const float MoonPhase = FMath::DegreesToRadians((TimeHours - 18.0f) * (360.0f / 24.0f));
	OutSunElev = FMath::Sin(SunPhase);
	OutMoonElev = FMath::Sin(MoonPhase);
}

void ATODSkyManager::ComputeSunMoonState(float TimeHours, float SunRiseAzimuth, float MoonPhaseOffset,
	float& OutSunElev, float& OutMoonElev, float& OutSunAzimuth, float& OutMoonAzimuth)
{
	ComputeElevations(TimeHours, OutSunElev, OutMoonElev);

	// The sun *orbits* the planet: its azimuth sweeps a full 360° per in-game day
	// (15°/hour) starting at the rise azimuth, so it sets opposite where it rose
	// instead of retracing. The moon rides the anti-solar point plus a phase offset.
	OutSunAzimuth = SunRiseAzimuth + (TimeHours - 6.0f) * 15.0f;
	OutMoonAzimuth = OutSunAzimuth + 180.0f + MoonPhaseOffset;
}

void ATODSkyManager::UpdateCelestials()
{
	const UTODSubsystem* TOD = GetTOD();
	if (!TOD)
	{
		return;
	}

	// Weather modulates the celestial lights (1.0 when no weather subsystem).
	float WeatherSunScale = 1.0f;
	float WeatherMoonScale = 1.0f;
	float WeatherSkyScale = 1.0f;
	if (const UTODWeatherSubsystem* Weather = GetTODWeather())
	{
		const FTODWeatherState& State = Weather->GetCurrentState();
		WeatherSunScale = State.SunIntensityScale;
		WeatherMoonScale = State.MoonIntensityScale;
		WeatherSkyScale = State.SkyLightScale;
	}

	ApplySunMoon(TOD->GetTimeOfDayHours(), WeatherSunScale, WeatherMoonScale, WeatherSkyScale);
}

void ATODSkyManager::ApplySunMoon(float TimeHours, float WeatherSunScale, float WeatherMoonScale, float WeatherSkyScale)
{
	if (!SunLight || !MoonLight)
	{
		return;
	}

	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAzimuth = 0.0f;
	float MoonAzimuth = 0.0f;
	ComputeSunMoonState(TimeHours, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees,
		SunElev, MoonElev, SunAzimuth, MoonAzimuth);

	// Directional light shines along its rotated -X axis: negative pitch points
	// the light down toward the ground, pitch 0 is horizontal (sunrise/sunset).
	const float SunPitch = -FMath::Clamp(SunElev, -1.0f, 1.0f) * MaxCelestialPitch;
	const float MoonPitch = -FMath::Clamp(MoonElev, -1.0f, 1.0f) * MaxCelestialPitch;

	SunLight->SetRelativeRotation(FRotator(SunPitch, SunAzimuth, 0.0f));
	MoonLight->SetRelativeRotation(FRotator(MoonPitch, MoonAzimuth, 0.0f));

	// Dawn/dusk crossfade instead of a hard switch: sun and moon swap the role of
	// primary atmosphere light smoothly across the horizon band, which removes the
	// classic "primary-light pop" at 06:00/18:00.
	const float SunWeight = FMath::SmoothStep(-0.04f, 0.08f, SunElev);
	const float MoonWeight = FMath::SmoothStep(-0.04f, 0.08f, MoonElev);

	SunLight->SetVisibility(SunWeight > 0.01f);
	MoonLight->SetVisibility(MoonWeight > 0.01f);

	if (SunWeight > 0.01f)
	{
		const float Daylight = FMath::Pow(FMath::Clamp(SunElev, 0.0f, 1.0f), 0.6f);
		SunLight->Intensity = SunPeakIntensity * Daylight * SunWeight * WeatherSunScale;

		// Warm the light as the sun approaches the horizon.
		const float Warmth = 1.0f - FMath::Clamp(SunElev / 0.35f, 0.0f, 1.0f);
		const FLinearColor SunColor = FLinearColor::LerpUsingHSV(SunZenithColor, SunHorizonColor, Warmth);
		SunLight->LightColor = SunColor.ToFColor(/*bSRGB*/ false);
	}

	if (MoonWeight > 0.01f)
	{
		const float Moonlight = FMath::Pow(FMath::Clamp(MoonElev, 0.0f, 1.0f), 0.7f);
		MoonLight->Intensity = MoonPeakIntensity * FMath::Max(Moonlight, 0.05f) * MoonWeight * WeatherMoonScale;
		MoonLight->LightColor = MoonColor.ToFColor(/*bSRGB*/ false);
	}

	if (SkyLight)
	{
		SkyLight->Intensity = 1.0f * WeatherSkyScale;
	}
}

void ATODSkyManager::RefreshSkyPreview()
{
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		return; // runtime is driven by the subsystem tick
	}

	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAzimuth = 0.0f;
	float MoonAzimuth = 0.0f;
	ComputeSunMoonState(PreviewTimeHours, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees,
		SunElev, MoonElev, SunAzimuth, MoonAzimuth);
	ApplySunMoon(PreviewTimeHours, 1.0f, 1.0f, 1.0f);

	UE_LOG(LogTODSky, Log, TEXT("Sky preview: %.1fh sun elev %.2f @az %.0f, moon elev %.2f @az %.0f."),
		PreviewTimeHours, SunElev, SunAzimuth, MoonElev, MoonAzimuth);
}

void ATODSkyManager::UpdateWeatherApplication()
{
	const UTODWeatherSubsystem* Weather = GetTODWeather();
	if (!Weather)
	{
		return;
	}
	const FTODWeatherState& State = Weather->GetCurrentState();

	if (HeightFog)
	{
		// Weather owns the fog density outright (absolute value from the preset).
		HeightFog->FogDensity = State.FogDensity;
	}

	if (VolumetricClouds)
	{
		// Coverage fades the whole cloud layer in/out for now; material-driven
		// coverage arrives with the M3 cloud work.
		const bool bCloudsWanted = bEnableVolumetricCloud && State.CloudCoverage > 0.02f;
		VolumetricClouds->SetVisibility(bCloudsWanted);
	}

	// Precipitation systems read User.SpawnRateScale (0-1) inside DreamFX sources.
	if (RainComponent)
	{
		RainComponent->SetVariableFloat(TEXT("User.SpawnRateScale"), State.RainIntensity);
	}
	if (SnowComponent)
	{
		SnowComponent->SetVariableFloat(TEXT("User.SpawnRateScale"), State.SnowIntensity);
	}
}

UTODWeatherSubsystem* ATODSkyManager::GetTODWeather() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTODWeatherSubsystem>() : nullptr;
}

UTODSubsystem* ATODSkyManager::GetTOD() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTODSubsystem>() : nullptr;
}

#if WITH_EDITOR
void ATODSkyManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName Changed = PropertyChangedEvent.Property->GetFName();

	if (Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, bEnableVolumetricCloud) && VolumetricClouds)
	{
		VolumetricClouds->SetVisibility(bEnableVolumetricCloud);
	}
	else if (Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, PreviewTimeHours))
	{
		// Scrubbing the slider re-lights the viewport without PIE.
		RefreshSkyPreview();
	}
	else if (Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, SunRiseAzimuthDegrees) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, MoonPhaseOffsetDegrees) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, SunPeakIntensity) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, MoonPeakIntensity))
	{
		RefreshSkyPreview();
	}
}
#endif
