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
	SunLight->SetRelativeRotation(FRotator(-45.0f, SunYawDegrees, 0.0f));

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(Root);
	MoonLight->bAtmosphereSunLight = true;
	MoonLight->AtmosphereSunLightIndex = 1;
	MoonLight->bCastCloudShadows = true;
	MoonLight->Intensity = 0.2f;
	MoonLight->SetRelativeRotation(FRotator(-45.0f, MoonYawDegrees, 0.0f));
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

void ATODSkyManager::UpdateCelestials()
{
	const UTODSubsystem* TOD = GetTOD();
	if (!TOD || !SunLight || !MoonLight)
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

	const float SunElev = TOD->GetSunElevation01();
	const float MoonElev = TOD->GetMoonElevation01();

	// Directional light shines along its rotated -X axis: negative pitch points
	// the light down toward the ground, pitch 0 is horizontal (sunrise/sunset).
	const float SunPitch = -FMath::Clamp(SunElev, -1.0f, 1.0f) * MaxCelestialPitch;
	const float MoonPitch = -FMath::Clamp(MoonElev, -1.0f, 1.0f) * MaxCelestialPitch;

	SunLight->SetRelativeRotation(FRotator(SunPitch, SunYawDegrees, 0.0f));
	MoonLight->SetRelativeRotation(FRotator(MoonPitch, MoonYawDegrees, 0.0f));

	const bool bSunUp = SunElev > 0.0f;
	const bool bMoonUp = !bSunUp;

	SunLight->SetVisibility(bSunUp);
	MoonLight->SetVisibility(bMoonUp);

	if (bSunUp)
	{
		const float Daylight = FMath::Pow(SunElev, 0.6f);
		SunLight->Intensity = SunPeakIntensity * Daylight * WeatherSunScale;

		// Warm the light as the sun approaches the horizon.
		const float Warmth = 1.0f - FMath::Clamp(SunElev / 0.35f, 0.0f, 1.0f);
		const FLinearColor SunColor = FLinearColor::LerpUsingHSV(SunZenithColor, SunHorizonColor, Warmth);
		SunLight->LightColor = SunColor.ToFColor(/*bSRGB*/ false);
	}
	else
	{
		const float Moonlight = FMath::Pow(FMath::Max(MoonElev, 0.0f), 0.7f);
		MoonLight->Intensity = MoonPeakIntensity * FMath::Max(Moonlight, 0.05f) * WeatherMoonScale;
		MoonLight->LightColor = MoonColor.ToFColor(/*bSRGB*/ false);
	}

	if (SkyLight)
	{
		SkyLight->Intensity = 1.0f * WeatherSkyScale;
	}
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

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ATODSkyManager, bEnableVolumetricCloud) &&
		VolumetricClouds)
	{
		VolumetricClouds->SetVisibility(bEnableVolumetricCloud);
	}
}
#endif
