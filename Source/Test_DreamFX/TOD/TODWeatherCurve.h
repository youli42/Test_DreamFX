// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "TODWeatherCurve.generated.h"

/**
 * A 24-hour weather curve.
 *
 * The horizontal axis is *day progress* in [0,1] (0 = midnight, 0.5 = noon),
 * the vertical axis is the weather value itself. Keys are stored inside the
 * asset (FRuntimeFloatCurve::EditorCurveData), so one weather file is
 * self-contained and every numeric field can change over the day.
 *
 * An empty curve means "not authored": consumers leave the target property
 * untouched instead of snapping it to zero. That is what makes the per-group
 * Enable switches useful - a weather file may drive only fog and leave the
 * sky alone.
 */
USTRUCT(BlueprintType)
struct TEST_DREAMFX_API FTODWeatherCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="曲线", meta=(DisplayName="曲线"))
	FRuntimeFloatCurve Curve;

	/** True when the curve has at least one key authored. */
	bool HasKeys() const;

	/** Evaluate at day progress [0,1]; returns Fallback when nothing is authored. */
	float Evaluate(float InDayProgress, float Fallback = 0.0f) const;

	/** Replace the keys with a flat line at Value. */
	void SetConstant(float Value);

	/** Replace the keys (times are day progress). Cubic interpolation, auto tangents. */
	void SetKeys(const TArray<TPair<float, float>>& InKeys);

	static FTODWeatherCurve Constant(float Value);

	/** A deterministic wobble around Base so a migrated preset visibly moves over the day. */
	static FTODWeatherCurve RandomAround(float Base, int32 Seed, float Variance = 0.35f);
};

/** RGBA colour authored as four independent 24-hour curves (matches typical weather tooling). */
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
