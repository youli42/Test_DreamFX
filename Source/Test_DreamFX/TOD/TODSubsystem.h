// Copyright (c) 2026 Test_DreamFX Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TODSubsystem.generated.h"

/** 可序列化的时钟状态；游戏将其与自身存档一并持久化。 */
USTRUCT(BlueprintType)
struct FTODTimeSnapshot
{
	GENERATED_BODY()

	/** 存档时刻的游戏内小时数 [0, 24)。 */
	UPROPERTY(BlueprintReadWrite, Category="TOD")
	float TimeOfDayHours = 9.0f;

	/** 存档时刻一个游戏内日所对应的真实分钟数。 */
	UPROPERTY(BlueprintReadWrite, Category="TOD")
	float DayLengthMinutes = 4.0f;
};

/**
 * UTODSubsystem
 *
 * 世界范围的昼夜时间状态。持有权威的世界时钟，并
 * 将其暴露给任意驱动者（默认由 ATODSkyManager 推进）。
 *
 * 设计说明：本子系统*只负责状态*，从不自行 Tick。场景中的 Actor
 * （ATODSkyManager）推进时钟，使时间只在关卡中存在天空装置
 * 时才流动。这呼应了生产级天气/TOD 系统所采用的
 * “环境管理器广播状态”模式。
 */
UCLASS(Blueprintable, BlueprintType)
class TEST_DREAMFX_API UTODSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ UWorldSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 推进游戏内时钟。由驱动者 Actor 每帧调用一次。 */
	void AdvanceTime(float DeltaSeconds);

	/** 当前游戏内时间（小时）[0, 24)。0 = 午夜，12 = 正午。 */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	/** 直接设置游戏内时钟（回卷到 [0, 24)）。 */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void SetTimeOfDayHours(float InHours);

	/** 太阳在倾斜轨道上的高度角正弦（见 ATODSkyManager）：日出/日落为 0，正午达到 sin(SunTilt) ≈ 0.87。 */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetSunElevation01() const;

	/** 月球高度角的正弦（同一倾斜大圆上的反日点）。 */
	UFUNCTION(BlueprintPure, Category="TOD")
	float GetMoonElevation01() const;

	/** 推进时每个真实秒对应的游戏内小时数。 */
	float GetTimeScale() const;

	/** 设置时间流速：一个游戏日对应的真实分钟数（0 表示停止推进）。 */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void SetDayLengthMinutes(float MinutesPerDay);

	UFUNCTION(BlueprintPure, Category="TOD")
	float GetDayLengthMinutes() const;

	/** 为持久化而对时钟做快照（issue #10 "天气/时间可随存档保存"）。 */
	UFUNCTION(BlueprintCallable, Category="TOD")
	FTODTimeSnapshot GetTimeSnapshot() const;

	/** 恢复由 GetTimeSnapshot 取得的快照。 */
	UFUNCTION(BlueprintCallable, Category="TOD")
	void ApplyTimeSnapshot(const FTODTimeSnapshot& Snapshot);

	/** 控制台命令处理：设置时钟，例如 `tod.SetTime 21.5`。 */
	static void HandleConsoleSetTime(const TArray<FString>& Args);

	/** 控制台命令处理：打印当前 TOD 状态，`tod.Info`。 */
	static void HandleConsoleInfo();

	/** 在无世界上下文时为控制台命令解析子系统（取 PIE / 首个游戏世界）。 */
	static UTODSubsystem* GetForConsole();

private:
	/** 游戏内小时数 [0, 24)。 */
	float TimeOfDayHours = 9.0f;

	/** 每个游戏日的真实分钟数；决定 TimeScaleHoursPerSecond（24 / (60*minutes)）。 */
	UPROPERTY(EditAnywhere, Category="TOD|时间", meta=(DisplayName="一天时长(真实分钟)"))
	float DayLengthMinutes = 4.0f;

	/** 为 false 时 AdvanceTime 不作任何事（时钟仍可手动设置）。 */
	bool bAutoAdvance = true;
};
