// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "TODWeatherCurve.generated.h"

/**
 * 24 小时天气曲线。
 *
 * 横轴是*一天进度*，区间 [0,1]（0 = 午夜，0.5 = 正午），
 * 纵轴是天气值本身。关键帧保存在资产内部
 * （FRuntimeFloatCurve::EditorCurveData），因此一个天气文件
 * 是自包含的，每个数值字段都能随一天进度变化。
 *
 * 空曲线表示「未制作」：使用者会保持目标属性原样，
 * 而不是将其归零。这正是各分组启用开关的价值——一个天气文件
 * 可以只驱动雾，
 * 而让天空保持原样。
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="曲线"))
	FRuntimeFloatCurve Curve;

	/** 曲线至少制作了一个关键帧时为真。 */
	bool HasKeys() const;

	/** 在一天进度 [0,1] 处求值；未制作时返回 Fallback。 */
	float Evaluate(float InDayProgress, float Fallback = 0.0f) const;

	/** 将关键帧替换为值为 Value 的水平直线。 */
	void SetConstant(float Value);

	/** 替换关键帧（时间为一天进度）。三次插值，自动切线。 */
	void SetKeys(const TArray<TPair<float, float>>& InKeys);

	static FTODWeatherCurve Constant(float Value);

	/** 围绕 Base 的确定性波动，使迁移后的预设在一天内有可见的变化。 */
	static FTODWeatherCurve RandomAround(float Base, int32 Seed, float Variance = 0.35f);
};

/** RGBA 颜色，制作为四条独立的 24 小时曲线（与常见天气工具的做法一致）。 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherColorCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="红"))
	FTODWeatherCurve R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="绿"))
	FTODWeatherCurve G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="蓝"))
	FTODWeatherCurve B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="透明度"))
	FTODWeatherCurve A;

	bool HasKeys() const;

	FLinearColor Evaluate(float InDayProgress, const FLinearColor& Fallback = FLinearColor::White) const;

	void SetConstant(const FLinearColor& Value);

	static FTODWeatherColorCurve Constant(const FLinearColor& Value);

	static FTODWeatherColorCurve RandomAround(const FLinearColor& Base, int32 Seed, float Variance = 0.25f);
};
