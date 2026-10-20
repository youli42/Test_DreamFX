// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TODSkyManager.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UTODSubsystem;
class UTODWeatherSubsystem;

/**
 * ATODSkyManager
 *
 * Pure C++ sky rig that drives a day/night cycle from UTODSubsystem:
 *  - two directional lights registered as atmosphere sun lights (index 0 = sun, 1 = moon)
 *  - SkyLight in real-time capture mode so ambient light follows the sky
 *  - SkyAtmosphere + VolumetricCloud + ExponentialHeightFog components
 *
 * The actor advances the world clock (via the subsystem) every tick, then
 * positions/colors the sun and moon. No Blueprint anywhere: place this actor
 * directly in a level, or spawn it from C++/PIE.
 */
UCLASS()
class TEST_DREAMFX_API ATODSkyManager : public AActor
{
	GENERATED_BODY()

public:
	ATODSkyManager();

	//~ AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Advance the shared world clock each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Time")
	bool bAutoAdvanceTime = true;

	/** SkyLight real-time capture (dynamic ambient). Disable for fully static lighting studies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Sky")
	bool bSkyLightRealTimeCapture = true;

	/** Enable the volumetric cloud component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Cloud")
	bool bEnableVolumetricCloud = true;

	/** Peak directional light intensity at zenith (unitless, UE directional light default is 10). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Sun", meta=(ClampMin="0.0"))
	float SunPeakIntensity = 8.0f;

	/** Peak directional light intensity of the moon at zenith. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Moon", meta=(ClampMin="0.0"))
	float MoonPeakIntensity = 0.4f;

	/** Sun yaw (degrees). Shadow direction is constant across the day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Sun")
	float SunYawDegrees = -37.5f;

	/** Moon yaw (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|Moon")
	float MoonYawDegrees = 142.5f;

	/** Maximum pitch magnitude applied to a celestial light (keeps away from gimbal lock). */
	static constexpr float MaxCelestialPitch = 89.0f;

private:
	/** Cache subsystem, configure light indices and sky capture. */
	void ConfigureSky();

	/** Read clock from subsystem and drive sun/moon/skylight. */
	void UpdateCelestials();

	/** Apply blended weather to fog, cloud layer and light multipliers. */
	void UpdateWeatherApplication();

	UTODSubsystem* GetTOD() const;
	UTODWeatherSubsystem* GetTODWeather() const;

public:
	/** Directional light acting as the physical sun (atmosphere sun light index 0). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	/** Directional light acting as the moon (atmosphere sun light index 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** Real-time captured sky ambient. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<USkyLightComponent> SkyLight;

	/** Physically based atmosphere. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	/** Optional raymarched cloud layer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<UVolumetricCloudComponent> VolumetricClouds;

	/** Height fog for distance haze (driven by weather in later milestones, present for completeness). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|Components")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

private:
	/** Sun light color near the horizon (warm). */
	UPROPERTY(EditAnywhere, Category="TOD|Sun")
	FLinearColor SunHorizonColor = FLinearColor(1.0f, 0.45f, 0.25f);

	/** Sun light color near zenith. */
	UPROPERTY(EditAnywhere, Category="TOD|Sun")
	FLinearColor SunZenithColor = FLinearColor(1.0f, 0.96f, 0.92f);

	/** Moon light color. */
	UPROPERTY(EditAnywhere, Category="TOD|Moon")
	FLinearColor MoonColor = FLinearColor(0.60f, 0.70f, 1.00f);

	bool bSkyConfigured = false;
};
