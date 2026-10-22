// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODSkyManager.h"

#include "TODSubsystem.h"
#include "TODWeatherSubsystem.h"
#include "TODWeatherProfile.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Containers/Set.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Rendering/ExponentialHeightFogData.h"
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

	// Static CDO pose at the default clock (9h), consistent with the tilted-orbit model.
	{
		float InitSunElev = 0.f, InitMoonElev = 0.f, InitSunAz = 0.f, InitMoonAz = 0.f;
		ComputeSunMoonState(9.0f, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees, SunTiltDegrees,
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

	WindComponent = CreateDefaultSubobject<UWindDirectionalSourceComponent>(TEXT("Wind"));
	WindComponent->SetupAttachment(Root);
	WindComponent->SetStrength(0.0f);
}

void ATODSkyManager::BeginPlay()
{
	Super::BeginPlay();

	ConfigureSky();
	EnsureWeatherPostProcess();
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

void ATODSkyManager::ComputeSunMoonState(float TimeHours, float SunRiseAzimuth, float MoonPhaseOffset, float SunTilt,
	float& OutSunElev, float& OutMoonElev, float& OutSunAzimuth, float& OutMoonAzimuth)
{
	// Tilted great circle, like a real mid-latitude sun track:
	//   v(H) = (cos H, sin H * cos T, sin H * sin T),  H = (t - 6) * 15°,  T = tilt.
	// H = 0 sits on the horizon at the rise azimuth, noon culminates at altitude = Tilt
	// (below the zenith, so shadows sweep instead of collapsing and flipping), H = 180
	// sets due opposite. Elevation is returned as sine-of-altitude.
	const float H = FMath::DegreesToRadians((TimeHours - 6.0f) * 15.0f);
	const float T = FMath::DegreesToRadians(FMath::Clamp(SunTilt, 5.0f, 89.5f));

	const float HorizonX = FMath::Cos(H);
	const float HorizonY = FMath::Sin(H) * FMath::Cos(T);
	OutSunElev = FMath::Sin(H) * FMath::Sin(T);
	OutSunAzimuth = SunRiseAzimuth + FMath::RadiansToDegrees(FMath::Atan2(HorizonY, HorizonX));

	// Moon = anti-solar point on the same circle (plus phase offset).
	OutMoonElev = -OutSunElev;
	OutMoonAzimuth = OutSunAzimuth + 180.0f + MoonPhaseOffset;
}

void ATODSkyManager::UpdateCelestials()
{
	const UTODSubsystem* TOD = GetTOD();
	if (!TOD)
	{
		return;
	}

	// Weather modulates the celestial lights and the sky ambient. Without a
	// weather subsystem the rig runs on neutral defaults (all groups disabled).
	FTODWeatherState NeutralWeather;
	if (const UTODWeatherSubsystem* Weather = GetTODWeather())
	{
		NeutralWeather = Weather->GetCurrentState();
	}

	ApplySunMoon(TOD->GetTimeOfDayHours(), NeutralWeather);
}

void ATODSkyManager::ApplySunMoon(float TimeHours, const FTODWeatherState& State)
{
	if (!SunLight || !MoonLight)
	{
		return;
	}

	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAzimuth = 0.0f;
	float MoonAzimuth = 0.0f;
	ComputeSunMoonState(TimeHours, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees, SunTiltDegrees,
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

	const bool bWeatherLights = State.bLightEnabled;
	const float SunScale = bWeatherLights ? State.SunIntensityScale : 1.0f;
	const float MoonScale = bWeatherLights ? State.MoonIntensityScale : 1.0f;

	if (SunWeight > 0.01f)
	{
		const float Daylight = FMath::Pow(FMath::Clamp(SunElev, 0.0f, 1.0f), 0.6f);
		SunLight->Intensity = SunPeakIntensity * Daylight * SunWeight * SunScale;

		if (bWeatherLights && State.bHasSunColor)
		{
			SunLight->LightColor = State.SunLightColor.ToFColor(/*bSRGB*/ false);
		}
		else
		{
			// Warm the light as the sun approaches the horizon (time-of-day colour).
			const float Warmth = 1.0f - FMath::Clamp(SunElev / 0.35f, 0.0f, 1.0f);
			const FLinearColor SunColor = FLinearColor::LerpUsingHSV(SunZenithColor, SunHorizonColor, Warmth);
			SunLight->LightColor = SunColor.ToFColor(/*bSRGB*/ false);
		}

		if (bWeatherLights)
		{
			const bool bUseTemperature = State.SunTemperature > 1.0f;
			SunLight->SetUseTemperature(bUseTemperature);
			if (bUseTemperature)
			{
				SunLight->SetTemperature(State.SunTemperature);
			}
			SunLight->SetVolumetricScatteringIntensity(State.SunVolumetricScattering);
			SunLight->SetEnableLightShaftBloom(State.SunBloomScale > 0.0f);
			SunLight->SetBloomScale(State.SunBloomScale);
		}
	}

	if (MoonWeight > 0.01f)
	{
		const float Moonlight = FMath::Pow(FMath::Clamp(MoonElev, 0.0f, 1.0f), 0.7f);
		MoonLight->Intensity = MoonPeakIntensity * FMath::Max(Moonlight, 0.05f) * MoonWeight * MoonScale;
		MoonLight->LightColor = (bWeatherLights && State.bHasMoonColor)
			? State.MoonLightColor.ToFColor(/*bSRGB*/ false)
			: MoonColor.ToFColor(/*bSRGB*/ false);

		if (bWeatherLights)
		{
			MoonLight->SetVolumetricScatteringIntensity(State.MoonVolumetricScattering);
		}
	}

	if (SkyLight)
	{
		const bool bWeatherSky = State.bSkyLightEnabled;
		SkyLight->Intensity = bWeatherSky ? State.SkyLightScale : 1.0f;
		if (bWeatherSky)
		{
			if (State.bHasSkyLightColor)
			{
				SkyLight->LightColor = State.SkyLightColor.ToFColor(/*bSRGB*/ true);
			}
			SkyLight->VolumetricScatteringIntensity = State.SkyLightVolumetricScattering;
		}
	}
}

void ATODSkyManager::RefreshSkyPreview()
{
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		return; // runtime is driven by the subsystem tick
	}

	// Editor weather preview (issue #12): the weather subsystem does not exist
	// in the editor world, so evaluate the picked weather file's curves here at
	// PreviewTimeHours/24 and apply the same groups the runtime would apply.
	FTODWeatherState PreviewState;
	FName PreviewWeatherName = NAME_None;
	bool bHasPreviewWeather = false;
	if (!PreviewWeatherProfile.IsNull())
	{
		const UTODWeatherProfile* Profile = PreviewWeatherProfile.Get();
		if (!Profile)
		{
			Profile = Cast<UTODWeatherProfile>(StaticLoadObject(UTODWeatherProfile::StaticClass(), nullptr,
				*PreviewWeatherProfile.ToSoftObjectPath().GetAssetPathString()));
		}
		if (Profile)
		{
			const float DayProgress = FMath::Frac(FMath::Clamp(PreviewTimeHours, 0.0f, 24.0f) / 24.0f);
			PreviewState = UTODWeatherSubsystem::EvaluateProfileAt(Profile->Data, DayProgress);
			PreviewWeatherName = PreviewState.CurrentPreset;
			bHasPreviewWeather = true;
			ApplyWeatherGroups(PreviewState);
		}
		else
		{
			UE_LOG(LogTODSky, Warning, TEXT("Preview weather profile '%s' failed to load."), *PreviewWeatherProfile.ToSoftObjectPath().ToString());
		}
	}

	if (!bHasPreviewWeather)
	{
		// No preview weather: neutral modulation, constructor defaults.
		if (HeightFog)
		{
			HeightFog->SetFogDensity(0.0003f);
		}
		if (VolumetricClouds)
		{
			VolumetricClouds->SetVisibility(bEnableVolumetricCloud);
		}
	}

	float SunElev = 0.0f;
	float MoonElev = 0.0f;
	float SunAzimuth = 0.0f;
	float MoonAzimuth = 0.0f;
	ComputeSunMoonState(PreviewTimeHours, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees, SunTiltDegrees,
		SunElev, MoonElev, SunAzimuth, MoonAzimuth);
	ApplySunMoon(PreviewTimeHours, PreviewState);

	UE_LOG(LogTODSky, Log, TEXT("Sky preview: %.1fh sun elev %.2f @az %.0f, moon elev %.2f @az %.0f, weather '%s' sunScale %.2f."),
		PreviewTimeHours, SunElev, SunAzimuth, MoonElev, MoonAzimuth, *PreviewWeatherName.ToString(), PreviewState.SunIntensityScale);
}

void ATODSkyManager::UpdateWeatherApplication()
{
	const UTODWeatherSubsystem* Weather = GetTODWeather();
	if (!Weather)
	{
		return;
	}
	ApplyWeatherGroups(Weather->GetCurrentState());
}

void ATODSkyManager::ApplyWeatherGroups(const FTODWeatherState& State)
{
	// Height fog: an enabled group owns every value; a disabled group is left untouched.
	if (State.bFogEnabled && HeightFog)
	{
		HeightFog->SetFogDensity(State.FogDensity);
		HeightFog->SetFogHeightFalloff(State.FogHeightFalloff);
		HeightFog->SetStartDistance(State.FogStartDistance);
		HeightFog->SetEndDistance(State.FogEndDistance);
		if (State.bHasFogInscatteringColor)
		{
			HeightFog->SetFogInscatteringColor(State.FogInscatteringColor);
		}

		FExponentialHeightFogData SecondFog = HeightFog->SecondFogData;
		SecondFog.FogDensity = State.SecondFogDensity;
		SecondFog.FogHeightFalloff = State.SecondFogHeightFalloff;
		SecondFog.FogHeightOffset = State.SecondFogHeightOffset;
		HeightFog->SetSecondFogData(SecondFog);
	}

	if (State.bVolumetricFogEnabled && HeightFog)
	{
		HeightFog->SetVolumetricFogAlbedo(State.VolumetricFogAlbedo.ToFColor(/*bSRGB*/ true));
		HeightFog->SetVolumetricFogScatteringDistribution(State.VolumetricFogScatteringDistribution);
		HeightFog->SetVolumetricFogExtinctionScale(State.VolumetricFogExtinctionScale);
		HeightFog->SetVolumetricFogDistance(State.VolumetricFogViewDistance);
		HeightFog->SetVolumetricFogStartDistance(State.VolumetricFogStartDistance);
	}

	// Cloud layer: coverage gates visibility for now; altitude curves move the layer.
	if (State.bCloudEnabled && VolumetricClouds)
	{
		VolumetricClouds->SetLayerBottomAltitude(State.CloudLayerBottomKm);
		VolumetricClouds->SetLayerHeight(State.CloudLayerHeightKm);
		const bool bCloudsWanted = bEnableVolumetricCloud && State.CloudCoverage > 0.02f;
		VolumetricClouds->SetVisibility(bCloudsWanted);
	}

	// Physical atmosphere scattering.
	if (State.bAtmosphereEnabled && SkyAtmosphere)
	{
		SkyAtmosphere->SetRayleighScatteringScale(State.RayleighScatteringScale);
		SkyAtmosphere->SetMieScatteringScale(State.MieScatteringScale);
		SkyAtmosphere->SetMieAnisotropy(State.MieAnisotropy);
		SkyAtmosphere->SetMieExponentialDistribution(State.MieExponentialDistribution);
		SkyAtmosphere->SetMultiScatteringFactor(State.MultiScatteringFactor);
		SkyAtmosphere->SetHeightFogContribution(State.HeightFogContribution);
	}

	if (WindComponent)
	{
		WindComponent->SetStrength(State.bWindEnabled ? State.WindStrength : 0.0f);
	}

	// Post process + particles are runtime-only; the editor preview leaves them alone.
	const UWorld* World = GetWorld();
	const bool bGameWorld = World && World->IsGameWorld();

	if (bGameWorld && State.bPostProcessEnabled)
	{
		EnsureWeatherPostProcess();
		if (WeatherPostProcess)
		{
			FPostProcessSettings& PP = WeatherPostProcess->Settings;
			PP.bOverride_AutoExposureBias = true;
			PP.AutoExposureBias = State.PPExposureBias;
			PP.bOverride_AutoExposureMinBrightness = true;
			PP.AutoExposureMinBrightness = State.PPExposureMinBrightness;
			PP.bOverride_AutoExposureMaxBrightness = true;
			PP.AutoExposureMaxBrightness = State.PPExposureMaxBrightness;
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(State.PPSaturation, State.PPSaturation, State.PPSaturation, 1.0);
			PP.bOverride_ColorContrast = true;
			PP.ColorContrast = FVector4(State.PPContrast, State.PPContrast, State.PPContrast, 1.0);
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = State.PPVignetteIntensity;
			PP.bOverride_BloomIntensity = true;
			PP.BloomIntensity = State.PPBloomIntensity;
			PP.bOverride_FilmGrainIntensity = true;
			PP.FilmGrainIntensity = State.PPFilmGrainIntensity;
			PP.bOverride_IndirectLightingIntensity = true;
			PP.IndirectLightingIntensity = State.PPIndirectLightingIntensity;
			if (State.bHasPPIndirectLightingColor)
			{
				PP.bOverride_IndirectLightingColor = true;
				PP.IndirectLightingColor = State.PPIndirectLightingColor;
			}
		}
	}

	if (bGameWorld)
	{
		UpdateWeatherEffects(State);
	}
}

void ATODSkyManager::EnsureWeatherPostProcess()
{
	if (WeatherPostProcess)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), SpawnParams);
	if (Volume)
	{
		Volume->bUnbound = true;
		Volume->bEnabled = true;
		Volume->BlendWeight = 1.0f;
		WeatherPostProcess = Volume;
	}
}

void ATODSkyManager::UpdateWeatherEffects(const FTODWeatherState& State)
{
	TSet<FName> Wanted;

	if (State.bEffectsEnabled)
	{
		for (const TPair<FName, FTODWeatherEffectState>& Pair : State.Effects)
		{
			const FTODWeatherEffectState& Effect = Pair.Value;
			const float Intensity = Effect.Intensity * Effect.Scale;
			if (Intensity <= 0.001f || Effect.System.IsNull())
			{
				continue;
			}

			UNiagaraSystem* System = Effect.System.LoadSynchronous();
			if (!System)
			{
				continue;
			}

			TObjectPtr<UNiagaraComponent>* Existing = WeatherEffectComponents.Find(Pair.Key);
			UNiagaraComponent* Component = Existing ? Existing->Get() : nullptr;
			if (!Component)
			{
				Component = NewObject<UNiagaraComponent>(this,
					MakeUniqueObjectName(this, UNiagaraComponent::StaticClass(), Pair.Key));
				Component->SetupAttachment(RootComponent);
				Component->RegisterComponent();
				WeatherEffectComponents.Add(Pair.Key, Component);
			}

			if (Component->GetAsset() != System)
			{
				Component->SetAsset(System);
			}
			Component->SetRelativeLocationAndRotation(Effect.RelativeLocation, Effect.RelativeRotation);
			Component->SetVariableFloat(Effect.ParameterName, Intensity);
			if (!Component->IsActive())
			{
				Component->Activate(true);
			}
			Wanted.Add(Pair.Key);
		}
	}

	// Retire effects that left the table or faded to zero (including during transitions).
	for (auto It = WeatherEffectComponents.CreateIterator(); It; ++It)
	{
		if (Wanted.Contains(It.Key()) && It.Value())
		{
			continue;
		}
		if (It.Value())
		{
			It.Value()->DestroyComponent();
		}
		It.RemoveCurrent();
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
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, SunTiltDegrees) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, MoonPhaseOffsetDegrees) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, SunPeakIntensity) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, MoonPeakIntensity) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, PreviewWeatherProfile))
	{
		RefreshSkyPreview();
	}
}
#endif
