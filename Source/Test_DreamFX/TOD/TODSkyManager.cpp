// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODSkyManager.h"

#include "TODSubsystem.h"
#include "TODWeatherSubsystem.h"
#include "TODWeatherProfile.h"

#include "Components/PostProcessComponent.h"
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
	/** 引擎默认的光线步进云层材质（UE 5.8）；精简构建中可能缺失。 */
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

	// 静态 CDO 姿态按默认时钟（9h）摆放，与倾斜轨道模型保持一致。
	{
		float InitSunElev = 0.f, InitMoonElev = 0.f, InitSunAz = 0.f, InitMoonAz = 0.f;
		ComputeSunMoonState(9.0f, SunRiseAzimuthDegrees, MoonPhaseOffsetDegrees, SunTiltDegrees,
			InitSunElev, InitMoonElev, InitSunAz, InitMoonAz);
		SunLight->SetRelativeRotation(FRotator(-InitSunElev * MaxCelestialPitch, InitSunAz, 0.0f));
		MoonLight->SetRelativeRotation(FRotator(-InitMoonElev * MaxCelestialPitch, InitMoonAz, 0.0f));
	}
	// 编辑器状态策略：任何时候只有一盏天体光源可见。UpdateCelestials()
	// 在运行期间独占切换可见性（太阳 XOR 月亮）；在此隐藏月亮
	// 可确保未被 Tick 的编辑器世界中也无双主光相争。
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
	// 引擎云层的高程以千米（km）为单位。
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

	// 时刻曝光限制载体：非绑定后处理组件，随 Actor 序列化，
	// 因此编辑器预览（无天气体积）与运行时都同样生效。
	// 高优先级 = 时刻 clamp 是硬约束；它只写曝光下限，其余后处理归天气体积。
	ExposurePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ExposurePostProcess"));
	ExposurePostProcess->SetupAttachment(Root);
	ExposurePostProcess->bUnbound = true;
	ExposurePostProcess->Priority = 1000.0f;
}

void ATODSkyManager::BeginPlay()
{
	Super::BeginPlay();

	ConfigureSky();
	EnsureWeatherPostProcess();
	// 首帧渲染前先求值一次天体与时刻曝光，避免第一帧仍以旧曝光下限显示。
	UpdateCelestials();
	ApplyTimeOfDayExposure();
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
		// 引擎组件自带默认云层材质；仅当该默认缺失时
		// （例如精简构建）才换上我们自己的。
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

	// 编辑器世界：动态预览（运行时时钟/天气子系统只存在于游戏世界，互不影响）。
	if (const UWorld* PreviewWorld = GetWorld(); PreviewWorld && !PreviewWorld->IsGameWorld())
	{
		TickEditorPreview(DeltaSeconds);
		return;
	}

	UTODSubsystem* TOD = GetTOD();
	if (!TOD)
	{
		return;
	}

	if (bAutoAdvanceTime)
	{
		TOD->AdvanceTime(DeltaSeconds);
	}

	// 天气混合与时钟共用同一驱动源。
	if (UTODWeatherSubsystem* Weather = GetTODWeather())
	{
		Weather->TickWeather(DeltaSeconds);
	}

	UpdateCelestials();
	UpdateWeatherApplication();
	ApplyTimeOfDayExposure();
}

void ATODSkyManager::ComputeSunMoonState(float TimeHours, float SunRiseAzimuth, float MoonPhaseOffset, float SunTilt,
	float& OutSunElev, float& OutMoonElev, float& OutSunAzimuth, float& OutMoonAzimuth)
{
	// 倾斜大圆，如同真实的中纬度太阳轨迹：
	//   v(H) = (cos H, sin H * cos T, sin H * sin T),  H = (t - 6) * 15°,  T = tilt.
	// H = 0 落在地平线上、升起方位角处，正午在高度角 = Tilt 处上中天
	// （低于天顶，故阴影平滑扫过而非塌缩翻转），H = 180
	// 位于正对面落下。返回的高度为高度角正弦值。
	const float H = FMath::DegreesToRadians((TimeHours - 6.0f) * 15.0f);
	const float T = FMath::DegreesToRadians(FMath::Clamp(SunTilt, 5.0f, 89.5f));

	const float HorizonX = FMath::Cos(H);
	const float HorizonY = FMath::Sin(H) * FMath::Cos(T);
	OutSunElev = FMath::Sin(H) * FMath::Sin(T);
	OutSunAzimuth = SunRiseAzimuth + FMath::RadiansToDegrees(FMath::Atan2(HorizonY, HorizonX));

	// 月亮 = 同一圆上的反日点（叠加相位偏移）。
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

	// 天气调制天体光源与环境光。若没有天气子系统，
	// 天空装置以中性默认值运行（所有组均不生效）。
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

	// 供 ApplyTimeOfDayExposure 使用（晨昏带上与主光同步过渡）。
	CachedSunElevation = SunElev;

	// 定向光沿其旋转后的 -X 轴照射：负俯仰把灯光
	// 指向地面下方，俯仰 0 为水平（日出/日落）。
	const float SunPitch = -FMath::Clamp(SunElev, -1.0f, 1.0f) * MaxCelestialPitch;
	const float MoonPitch = -FMath::Clamp(MoonElev, -1.0f, 1.0f) * MaxCelestialPitch;

	SunLight->SetRelativeRotation(FRotator(SunPitch, SunAzimuth, 0.0f));
	MoonLight->SetRelativeRotation(FRotator(MoonPitch, MoonAzimuth, 0.0f));

	// 晨昏用渐变交替而非硬切换：日月在地平线带上平滑互换
	// 主大气光角色，从而消除 06:00/18:00 处
	// 经典的「主光突变」。
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
			// 太阳接近地平线时让灯光变暖（昼夜时刻配色）。
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
		return; // 运行时由子系统 tick 驱动
	}

	// 编辑器天气预览（issue #12）：编辑器世界中不存在天气子系统，所以
	// 在此按 PreviewTimeHours/24 求值所选天气文件的曲线，缓存到 PreviewState。
	// 未选/加载失败则回到中性状态。
	PreviewState = FTODWeatherState();
	FName PreviewWeatherName = NAME_None;
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
		}
		else
		{
			UE_LOG(LogTODSky, Warning, TEXT("Preview weather profile '%s' failed to load."), *PreviewWeatherProfile.ToSoftObjectPath().ToString());
		}
	}

	if (PreviewWeatherName.IsNone())
	{
		// 无预览天气：中性调制，沿用构造函数默认值。
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

	ApplyEditorPreview();

	const bool bExposureActive = ExposurePostProcess && ExposurePostProcess->Settings.bOverride_AutoExposureMinBrightness;
	UE_LOG(LogTODSky, Log, TEXT("Sky preview: %.1fh sun elev %.2f @az %.0f, moon elev %.2f @az %.0f, weather '%s' sunScale %.2f, exposureMin %s"),
		PreviewTimeHours, SunElev, SunAzimuth, MoonElev, MoonAzimuth, *PreviewWeatherName.ToString(), PreviewState.SunIntensityScale,
		bExposureActive
			? *FString::Printf(TEXT("%.1f EV100"), ExposurePostProcess->Settings.AutoExposureMinBrightness)
			: TEXT("off (engine default)"));
}

bool ATODSkyManager::ShouldTickIfViewportsOnly() const
{
	// 动态预览：编辑器视口（LEVELTICK_ViewportsOnly）下也要每帧 Tick，
	// 让天空随预览时钟连续演进；关闭时退回引擎默认（编辑器世界不 Tick）。
	return bEnableDynamicPreview || Super::ShouldTickIfViewportsOnly();
}

void ATODSkyManager::TickEditorPreview(float DeltaSeconds)
{
	if (!bEnableDynamicPreview || DeltaSeconds <= 0.0f)
	{
		return; // 静态预览仍由「刷新天空预览」与属性变更驱动
	}

	// 预览时钟只存在于编辑器世界：运行时时钟归 UTODSubsystem，两者互不影响。
	// 倍率语义：每真实秒推进 Scale 个游戏秒（360 ≈ 运行时默认一天 4 真实分钟）。
	PreviewTimeHours = FMath::Wrap(PreviewTimeHours + DeltaSeconds * PreviewTimeScale / 3600.0f, 0.0f, 24.0f);

	// 时钟在流动，所选天气文件的 24h 曲线必须跟着重求值。
	if (!PreviewWeatherProfile.IsNull())
	{
		if (const UTODWeatherProfile* Profile = PreviewWeatherProfile.Get())
		{
			PreviewState = UTODWeatherSubsystem::EvaluateProfileAt(Profile->Data, FMath::Frac(PreviewTimeHours / 24.0f));
		}
	}

	ApplyEditorPreview();
}

void ATODSkyManager::ApplyEditorPreview()
{
	const UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	// 与运行时 Tick 相同的顺序：天体（更新 CachedSunElevation）→
	// 天气组（编辑器世界只生效雾/云/大气/风）→ 时刻曝光。
	ApplySunMoon(PreviewTimeHours, PreviewState);
	ApplyWeatherGroups(PreviewState);
	ApplyTimeOfDayExposure();
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

void ATODSkyManager::ApplyTimeOfDayExposure()
{
	if (!ExposurePostProcess)
	{
		return;
	}

	if (!bEnableTimeOfDayExposure)
	{
		// 关闭时刻限制：撤下本组件的曝光范围覆盖，天气后处理组的曝光曲线重新接管。
		ExposurePostProcess->Settings.bOverride_AutoExposureMinBrightness = false;
		ExposurePostProcess->Settings.bOverride_AutoExposureMaxBrightness = false;
		return;
	}

	// 与日月主光切换同一晨昏带（见 ApplySunMoon 的 SunWeight/MoonWeight）：
	// 适应范围在昼/夜值之间平滑过渡，日出日落无跳变。
	// 夜间下限=上限（默认）时曝光被钉死在单一 EV，夜景亮度确定、
	// 不随月光/云量/雾的亮度波动漂移；调宽窗口可为夜晚室内保留适应余量。
	const float DayFactor = FMath::SmoothStep(-0.04f, 0.08f, CachedSunElevation);
	const float MinEV = FMath::Lerp(NightExposureMinEV100, DayExposureMinEV100, DayFactor);
	const float MaxEV = FMath::Lerp(NightExposureMaxEV100, DayExposureMaxEV100, DayFactor);

	ExposurePostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
	ExposurePostProcess->Settings.AutoExposureMinBrightness = MinEV;
	ExposurePostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
	ExposurePostProcess->Settings.AutoExposureMaxBrightness = MaxEV;
}

void ATODSkyManager::ApplyWeatherGroups(const FTODWeatherState& State)
{
	// 高度雾：启用的组接管全部值；未启用的组一概不碰。
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

	// 云层：暂以云量决定可见性；高度曲线负责移动云层。
	if (State.bCloudEnabled && VolumetricClouds)
	{
		VolumetricClouds->SetLayerBottomAltitude(State.CloudLayerBottomKm);
		VolumetricClouds->SetLayerHeight(State.CloudLayerHeightKm);
		const bool bCloudsWanted = bEnableVolumetricCloud && State.CloudCoverage > 0.02f;
		VolumetricClouds->SetVisibility(bCloudsWanted);
	}

	// 物理大气散射。
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

	// 后处理 + 粒子仅运行时生效；编辑器预览对它们不做处理。
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
			if (bEnableTimeOfDayExposure)
			{
				// 时刻曝光限制启用时拥有曝光下限/上限所有权；
				// 撤掉体积上的旧覆盖，避免双写打架（组件优先级更高）。
				PP.bOverride_AutoExposureMinBrightness = false;
				PP.bOverride_AutoExposureMaxBrightness = false;
			}
			else
			{
				// 曲线无键 = 作者未制作曝光：撤掉覆盖保持引擎默认，
				// 而不是把 fallback 数值写上（旧 fallback -1/+3 会把白天钳到过曝）。
				PP.bOverride_AutoExposureMinBrightness = State.bHasPPExposureMin;
				if (State.bHasPPExposureMin)
				{
					PP.AutoExposureMinBrightness = State.PPExposureMinBrightness;
				}
				PP.bOverride_AutoExposureMaxBrightness = State.bHasPPExposureMax;
				if (State.bHasPPExposureMax)
				{
					PP.AutoExposureMaxBrightness = State.PPExposureMaxBrightness;
				}
			}
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

	// 回收已离开特效表或淡到 0 的特效（包括过渡期间淡出的）。
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
		// 拖动滑条即可无需 PIE 重新点亮视口。
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
	else if (Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, bEnableTimeOfDayExposure) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, DayExposureMinEV100) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, DayExposureMaxEV100) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, NightExposureMinEV100) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, NightExposureMaxEV100) ||
		Changed == GET_MEMBER_NAME_CHECKED(ATODSkyManager, bEnableDynamicPreview))
	{
		// 曝光/动态预览开关改动：编辑器里即时重放预览（开启即见当前时刻）；
		// 运行时下一 tick 自然生效。PreviewTimeScale 无需刷新，下一 tick 生效。
		RefreshSkyPreview();
	}
}
#endif
