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
 * 纯 C++ 天空装置，基于 UTODSubsystem 驱动昼夜循环：
 *  - 两盏定向光注册为大气太阳光（索引 0 = 太阳，1 = 月亮）
 *  - SkyLight 采用实时捕获模式，使环境光跟随天空
 *  - SkyAtmosphere + VolumetricCloud + ExponentialHeightFog 组件
 *
 * 该 Actor 每 tick 推进世界时钟（经由子系统），然后
 * 定位太阳与月亮并设置其颜色。完全不使用 Blueprint：可直接放到
 * 关卡里，或在 C++/PIE 中生成。
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

	/** 每 tick 推进共享的世界时钟。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|时间", meta=(DisplayName="自动推进时间"))
	bool bAutoAdvanceTime = true;

	/* ───────────────── 编辑器预览套件（一组，无需 PIE） ─────────────────
	 * PreviewTimeHours 拖动时钟，PreviewWeatherProfile 挑选天气
	 * 资产（WP_*.uasset），RefreshSkyPreview 把两者应用到当前编辑器世界。
	 * 运行时从不读取这些——时钟来自 UTODSubsystem，
	 * 天气来自 UTODWeatherSubsystem。 */

	/** 编辑器专用的时间刻度：改它（或按下按钮）即可无需 PIE
	 *  就在视口里预览天空。运行时的循环由 UTODSubsystem 驱动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TOD|预览", meta=(DisplayName="预览时刻(小时)", ClampMin="0.0", ClampMax="24.0", ToolTip="编辑器视口显示的游戏时刻,与「预览天气文件」组合使用。"))
	float PreviewTimeHours = 12.0f;

	/** 按 PreviewTimeHours + PreviewWeatherProfile 在编辑器世界应用天体状态（细节面板按钮）。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category="TOD|预览", meta=(DisplayName="刷新天空预览"))
	void RefreshSkyPreview();

	/** 编辑器专用的天气预览（issue #12）：挑选一个 UTODWeatherProfile 资产
	 *  （WP_ClearSky、WP_Thunderstorm 等），该 Actor 就按此天气调制编辑器世界的光照
	 *  ——日月/天光缩放、雾密度、云层
	 *  可见性——叠加在 PreviewTimeHours 之上。留空则为中性调制。
	 *  运行时天气来自 UTODWeatherSubsystem，绝不取自此属性。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|预览", meta=(DisplayName="预览天气文件", ToolTip="仅用于编辑器预览的天气资产。改变即立刻刷新视口。"))
	TSoftObjectPtr<UTODWeatherProfile> PreviewWeatherProfile;

	/** 倾斜大圆上的完整天体状态（真实中纬度太阳轨迹）：
	 *  v(H) = (cos H, sin H·cos T, sin H·sin T)，其中 H = (t-6)·15°，T = 太阳倾角。
	 *  日出落在地平线上、升起方位角处，太阳在天空中划出*弧线*，
	 *  正午在高度角 = Tilt 处上中天（永不越过天顶，故阴影平滑扫过、
	 *  不会塌缩翻转），日落位于正对面。
	 *  Out*Elev 为高度角正弦值 [-1,1]；方位角以度为单位。
	 *  月亮沿同一圆的反日点运行（叠加相位偏移）。 */
	static void ComputeSunMoonState(float TimeHours, float SunRiseAzimuth, float MoonPhaseOffset, float SunTilt,
		float& OutSunElev, float& OutMoonElev, float& OutSunAzimuth, float& OutMoonAzimuth);

	/** 天光实时捕获（动态环境光）。全静态光照研究时可关闭。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|天空", meta=(DisplayName="天光实时捕获"))
	bool bSkyLightRealTimeCapture = true;

	/** 启用体积云组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|云层", meta=(DisplayName="启用体积云"))
	bool bEnableVolumetricCloud = true;

	/** 天顶处定向光的峰值强度（无量纲，UE 定向光默认为 10）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="太阳峰值强度", ClampMin="0.0"))
	float SunPeakIntensity = 8.0f;

	/** 月亮在天顶处的定向光峰值强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|月亮", meta=(DisplayName="月亮峰值强度", ClampMin="0.0"))
	float MoonPeakIntensity = 0.4f;

	/** 06:00 日出时太阳的方位角（度）。从此处起太阳每个
	 *  游戏日内旋转 360°：+180° 位于正对面（日落，18:00），午夜则在星球之下。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="日出方位角(度)"))
	float SunRiseAzimuthDegrees = 90.0f;

	/** 正午太阳的最大高度角（倾斜轨道平面的度数）。60° 给出
	 *  中纬度弧线：太阳明显划过天空；阴影永不消失。
	 *  接近 90° 时太阳会越过天顶，阴影在正午翻转（应避免）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|太阳", meta=(DisplayName="太阳轨道倾角(度)", ClampMin="20.0", ClampMax="85.0"))
	float SunTiltDegrees = 60.0f;

	/** 月亮相对反日点的方位角偏移（度）（0 = 每晚都是满月）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TOD|月亮", meta=(DisplayName="月相偏移(度)", ClampMin="-180.0", ClampMax="180.0"))
	float MoonPhaseOffsetDegrees = 0.0f;

	/** 施加到天体光源上的最大俯仰幅度（避开万向节死锁）。 */
	static constexpr float MaxCelestialPitch = 89.0f;

	/** 供未放置天空装置却查询高度的使用者（如 UTODSubsystem）使用的倾角。 */
	static constexpr float DefaultSunTiltDegrees = 60.0f;

private:
	/** 缓存子系统，配置灯光索引与天空捕获。 */
	void ConfigureSky();

	/** 从子系统读取时钟并驱动日月/天光。 */
	void UpdateCelestials();

	/** 按游戏内小时对两盏天体光源定位、淡变与上色
	 *  （方位角环绕 + 晨昏地平线带渐变交替），并由天气状态调制。 */
	void ApplySunMoon(float TimeHours, const FTODWeatherState& State);

	/** 将混合后的天气应用到雾、云层、大气、风与后处理。 */
	void UpdateWeatherApplication();

	UTODSubsystem* GetTOD() const;
	UTODWeatherSubsystem* GetTODWeather() const;

public:
	/** 充当物理太阳的定向光（大气太阳光索引 0）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="阳光灯"))
	TObjectPtr<UDirectionalLightComponent> SunLight;

	/** 充当月亮的定向光（大气太阳光索引 1）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="月光灯"))
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** 实时捕获的天空环境光。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="天光"))
	TObjectPtr<USkyLightComponent> SkyLight;

	/** 基于物理的大气系统。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="大气系统"))
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	/** 可选的光线步进云层。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="体积云"))
	TObjectPtr<UVolumetricCloudComponent> VolumetricClouds;

	/** 用于远景雾霭的高度雾（后续里程碑由天气驱动，为组件完整性而存在）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="高度雾"))
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	/** 由当前天气文件风组驱动的全局风源。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TOD|组件", meta=(DisplayName="全局风源"))
	TObjectPtr<UWindDirectionalSourceComponent> WindComponent;

	/* ───────────────── 天气应用 ───────────────── */

	/** 将混合后的天气状态应用到雾 / 灯光 / 云 / 大气 / 风 / 后处理。 */
	void ApplyWeatherGroups(const FTODWeatherState& State);

	/** 为天气特效表的每一行生成、驱动并回收一个 Niagara 组件。 */
	void UpdateWeatherEffects(const FTODWeatherState& State);

	/** 惰性创建天气后处理组所写入的非绑定后处理体积。 */
	void EnsureWeatherPostProcess();

private:
	/** 由天空装置持有的全局非绑定后处理体积（仅运行时）。 */
	UPROPERTY()
	TObjectPtr<APostProcessVolume> WeatherPostProcess;

	/** 在用的天气特效组件，以强度表行名为键。 */
	UPROPERTY()
	TMap<FName, TObjectPtr<UNiagaraComponent>> WeatherEffectComponents;

	/** 地平线附近的阳光颜色（暖色）。 */
	UPROPERTY(EditAnywhere, Category="TOD|太阳", meta=(DisplayName="太阳-地平线色"))
	FLinearColor SunHorizonColor = FLinearColor(1.0f, 0.45f, 0.25f);

	/** 天顶附近的阳光颜色。 */
	UPROPERTY(EditAnywhere, Category="TOD|太阳", meta=(DisplayName="太阳-天顶色"))
	FLinearColor SunZenithColor = FLinearColor(1.0f, 0.96f, 0.92f);

	/** 月光颜色。 */
	UPROPERTY(EditAnywhere, Category="TOD|月亮", meta=(DisplayName="月亮色"))
	FLinearColor MoonColor = FLinearColor(0.60f, 0.70f, 1.00f);

	bool bSkyConfigured = false;
};
