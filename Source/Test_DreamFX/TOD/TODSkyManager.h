// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TODSkyManager.generated.h"

class UDirectionalLightComponent;
class UNiagaraComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UWindDirectionalSourceComponent;
class APostProcessVolume;
class UTODSubsystem;
class UTODWeatherSubsystem;
class UTODWeatherProfile;
struct FTODWeatherState;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|时间", meta=(DisplayName="自动推进时间"))
	bool bAutoAdvanceTime = true;

	/* ───────────────── Editor preview kit (one group, no PIE needed) ─────────────────
	 * PreviewTimeHours scrubs the clock, PreviewWeatherProfile picks a weather
	 * asset (WP_*.uasset), RefreshSkyPreview applies both to this editor world.
	 * Runtime never reads these - the clock comes from UTODSubsystem and the
	 * weather from UTODWeatherSubsystem. */

	/** Editor-only time scrubber: change it (or press the button) to preview the sky
	 *  in the viewport without PIE. The runtime cycle is driven by UTODSubsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|预览", meta=(DisplayName="预览时刻(小时)", ClampMin="0.0", ClampMax="24.0", ToolTip="编辑器视口显示的游戏时刻,与「预览天气文件」组合使用。"))
	float PreviewTimeHours = 12.0f;

	/** Apply the sun/moon state for PreviewTimeHours + PreviewWeatherProfile in the editor world (details-panel button). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="TOD|预览", meta=(DisplayName="刷新天空预览"))
	void RefreshSkyPreview();

	/** Editor-only weather preview (issue #12): pick a UTODWeatherProfile asset
	 *  (WP_ClearSky, WP_Thunderstorm, ...) and this actor lights the editor world
	 *  as that weather modulates it - sun/moon/skylight scale, fog density, cloud
	 *  visibility - on top of PreviewTimeHours. Leave empty for neutral modulation.
	 *  Runtime weather comes from UTODWeatherSubsystem, never from this property. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|预览", meta=(DisplayName="预览天气文件", ToolTip="仅用于编辑器预览的天气资产。改变即立刻刷新视口。"))
	TSoftObjectPtr<UTODWeatherProfile> PreviewWeatherProfile;

	/** Full celestial state on a TILTED GREAT CIRCLE (a real mid-latitude sun track):
	 *  v(H) = (cos H, sin H·cos T, sin H·sin T) with H = (t-6)·15°, T = sun tilt.
	 *  Sunrise sits on the horizon at the rise azimuth, the sun *arcs* across the sky
	 *  culminating at altitude = Tilt at noon (never crossing the zenith, so shadows
	 *  sweep smoothly instead of collapsing and flipping), and it sets due opposite.
	 *  Out*Elev are sine-of-altitude values [-1,1]; azimuths are degrees.
	 *  The moon rides the anti-solar point on the same circle (+phase offset). */
	static void ComputeSunMoonState(float TimeHours, float SunRiseAzimuth, float MoonPhaseOffset, float SunTilt,
		float& OutSunElev, float& OutMoonElev, float& OutSunAzimuth, float& OutMoonAzimuth);

	/** SkyLight real-time capture (dynamic ambient). Disable for fully static lighting studies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|天空", meta=(DisplayName="天光实时捕获"))
	bool bSkyLightRealTimeCapture = true;

	/** Enable the volumetric cloud component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|云层", meta=(DisplayName="启用体积云"))
	bool bEnableVolumetricCloud = true;

	/** Peak directional light intensity at zenith (unitless, UE directional light default is 10). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="太阳峰值强度", ClampMin="0.0"))
	float SunPeakIntensity = 8.0f;

	/** Peak directional light intensity of the moon at zenith. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|月亮", meta=(DisplayName="月亮峰值强度", ClampMin="0.0"))
	float MoonPeakIntensity = 0.4f;

	/** Azimuth (degrees) of the sun at 06:00 (sunrise). From there the sun rotates
	 *  360° per in-game day: due at +180° (sunset, 18:00), below the planet at midnight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="日出方位角(度)"))
	float SunRiseAzimuthDegrees = 90.0f;

	/** Max sun altitude at noon (degrees of the tilted orbit plane). 60° gives a
	 *  mid-latitude arc: the sun visibly travels across the sky; shadows never vanish.
	 *  Near 90° the sun would cross the zenith and shadows flip at noon (avoid). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="太阳轨道倾角(度)", ClampMin="20.0", ClampMax="85.0"))
	float SunTiltDegrees = 60.0f;

	/** Moon azimuth offset (degrees) from the anti-solar point (0 = full moon each night). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|月亮", meta=(DisplayName="月相偏移(度)", ClampMin="-180.0", ClampMax="180.0"))
	float MoonPhaseOffsetDegrees = 0.0f;

	/** Maximum pitch magnitude applied to a celestial light (keeps away from gimbal lock). */
	static constexpr float MaxCelestialPitch = 89.0f;

	/** Tilt used by consumers that query elevations without a placed sky rig (e.g. UTODSubsystem). */
	static constexpr float DefaultSunTiltDegrees = 60.0f;

private:
	/** Cache subsystem, configure light indices and sky capture. */
	void ConfigureSky();

	/** Read clock from subsystem and drive sun/moon/skylight. */
	void UpdateCelestials();

	/** Position, fade and color the two celestial lights for an in-game hour
	 *  (azimuth orbit + horizon-crossfade at dawn/dusk), modulated by the weather state. */
	void ApplySunMoon(float TimeHours, const FTODWeatherState& State);

	/** Apply blended weather to fog, cloud layer, atmosphere, wind and post process. */
	void UpdateWeatherApplication();

	UTODSubsystem* GetTOD() const;
	UTODWeatherSubsystem* GetTODWeather() const;

public:
	/** Directional light acting as the physical sun (atmosphere sun light index 0). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="阳光灯"))
	TObjectPtr<UDirectionalLightComponent> SunLight;

	/** Directional light acting as the moon (atmosphere sun light index 1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="月光灯"))
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** Real-time captured sky ambient. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="天光"))
	TObjectPtr<USkyLightComponent> SkyLight;

	/** Physically based atmosphere. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="大气系统"))
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	/** Optional raymarched cloud layer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="体积云"))
	TObjectPtr<UVolumetricCloudComponent> VolumetricClouds;

	/** Height fog for distance haze (driven by weather in later milestones, present for completeness). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="高度雾"))
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	/** Global wind source driven by the active weather file's wind group. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="全局风源"))
	TObjectPtr<UWindDirectionalSourceComponent> WindComponent;

	/* ───────────────── Weather application ───────────────── */

	/** Apply the blended weather state to fog / lights / clouds / atmosphere / wind / post process. */
	void ApplyWeatherGroups(const FTODWeatherState& State);

	/** Spawn, drive and retire one Niagara component per row of the weather-effect table. */
	void UpdateWeatherEffects(const FTODWeatherState& State);

	/** Lazily create the unbound post-process volume the weather post-process group writes into. */
	void EnsureWeatherPostProcess();

private:
	/** Global unbound post-process volume owned by the sky rig (runtime only). */
	UPROPERTY()
	TObjectPtr<APostProcessVolume> WeatherPostProcess;

	/** Live weather-effect components, keyed by the intensity table row name. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UNiagaraComponent>> WeatherEffectComponents;

	/** Sun light color near the horizon (warm). */
	UPROPERTY(EditAnywhere, Category="TOD|太阳", meta=(DisplayName="太阳-地平线色"))
	FLinearColor SunHorizonColor = FLinearColor(1.0f, 0.45f, 0.25f);

	/** Sun light color near zenith. */
	UPROPERTY(EditAnywhere, Category="TOD|太阳", meta=(DisplayName="太阳-天顶色"))
	FLinearColor SunZenithColor = FLinearColor(1.0f, 0.96f, 0.92f);

	/** Moon light color. */
	UPROPERTY(EditAnywhere, Category="TOD|月亮", meta=(DisplayName="月亮色"))
	FLinearColor MoonColor = FLinearColor(0.60f, 0.70f, 1.00f);

	bool bSkyConfigured = false;
};
