// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#include "TODWeatherCurve.h"

#include "Math/RandomStream.h"

bool FTODWeatherCurve::HasKeys() const
{
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	return RichCurve && RichCurve->Keys.Num() > 0;
}

float FTODWeatherCurve::Evaluate(float InDayProgress, float Fallback) const
{
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	if (!RichCurve || RichCurve->Keys.Num() == 0)
	{
		return Fallback;
	}
	return RichCurve->Eval(FMath::Clamp(InDayProgress, 0.0f, 1.0f));
}

void FTODWeatherCurve::SetConstant(float Value)
{
	FRichCurve* RichCurve = Curve.GetRichCurve();
	RichCurve->Reset();
	const FKeyHandle Start = RichCurve->AddKey(0.0f, Value);
	const FKeyHandle End = RichCurve->AddKey(1.0f, Value);
	RichCurve->SetKeyInterpMode(Start, RCIM_Cubic);
	RichCurve->SetKeyInterpMode(End, RCIM_Cubic);
	RichCurve->PreInfinityExtrap = RCCE_Constant;
	RichCurve->PostInfinityExtrap = RCCE_Constant;
}

void FTODWeatherCurve::SetKeys(const TArray<TPair<float, float>>& InKeys)
{
	FRichCurve* RichCurve = Curve.GetRichCurve();
	RichCurve->Reset();
	for (const TPair<float, float>& Key : InKeys)
	{
		const FKeyHandle Handle = RichCurve->AddKey(FMath::Clamp(Key.Key, 0.0f, 1.0f), Key.Value);
		RichCurve->SetKeyInterpMode(Handle, RCIM_Cubic);
	}
	RichCurve->PreInfinityExtrap = RCCE_Constant;
	RichCurve->PostInfinityExtrap = RCCE_Constant;
}

FTODWeatherCurve FTODWeatherCurve::Constant(float Value)
{
	FTODWeatherCurve Result;
	Result.SetConstant(Value);
	return Result;
}

FTODWeatherCurve FTODWeatherCurve::RandomAround(float Base, int32 Seed, float Variance)
{
	FRandomStream Stream(Seed);
	TArray<TPair<float, float>> Keys;
	Keys.Reserve(5);
	for (int32 Index = 0; Index <= 4; ++Index)
	{
		const float Time = static_cast<float>(Index) / 4.0f;
		const float Factor = 1.0f + Stream.FRandRange(-Variance, Variance);
		Keys.Emplace(Time, FMath::Max(0.0f, Base * Factor));
	}

	FTODWeatherCurve Result;
	Result.SetKeys(Keys);
	return Result;
}

bool FTODWeatherColorCurve::HasKeys() const
{
	return R.HasKeys() || G.HasKeys() || B.HasKeys() || A.HasKeys();
}

FLinearColor FTODWeatherColorCurve::Evaluate(float InDayProgress, const FLinearColor& Fallback) const
{
	return FLinearColor(
		R.Evaluate(InDayProgress, Fallback.R),
		G.Evaluate(InDayProgress, Fallback.G),
		B.Evaluate(InDayProgress, Fallback.B),
		A.Evaluate(InDayProgress, Fallback.A));
}

void FTODWeatherColorCurve::SetConstant(const FLinearColor& Value)
{
	R.SetConstant(Value.R);
	G.SetConstant(Value.G);
	B.SetConstant(Value.B);
	A.SetConstant(Value.A);
}

FTODWeatherColorCurve FTODWeatherColorCurve::Constant(const FLinearColor& Value)
{
	FTODWeatherColorCurve Result;
	Result.SetConstant(Value);
	return Result;
}

FTODWeatherColorCurve FTODWeatherColorCurve::RandomAround(const FLinearColor& Base, int32 Seed, float Variance)
{
	FTODWeatherColorCurve Result;
	Result.R = FTODWeatherCurve::RandomAround(Base.R, Seed, Variance);
	Result.G = FTODWeatherCurve::RandomAround(Base.G, Seed + 1, Variance);
	Result.B = FTODWeatherCurve::RandomAround(Base.B, Seed + 2, Variance);
	Result.A = FTODWeatherCurve::RandomAround(Base.A, Seed + 3, Variance);
	return Result;
}
