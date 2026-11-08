#pragma once

#include "CoreMinimal.h"
#include "JTSPopulationRecovery.generated.h"

/** Shared server policy. Each depletion cohort gets its own delay and finite, fast-to-slow recovery. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPopulationRecoverySettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery") bool bEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="0")) float Delay = 60;
	/** Time after the delay to restore a depleted cohort, provided placement is possible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="1")) float Duration = 600;
	/** Larger values concentrate recovery near the beginning of the period. Zero is linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="0", ClampMax="8")) float FrontLoad = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="0.25")) float CheckInterval = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="1", ClampMax="512")) int32 BatchSize = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recovery", meta=(ClampMin="0")) float MinimumPlayerDistance = 1800;

	double Fraction(double Age) const;
};

/** Pure accounting; owners retain their existing timers, authority checks and placement rules. */
class SPACE_API FJTSPopulationRecovery
{
public:
	void Reset(int32 LiveCount, int32 Seed = 0);
	int32 Update(int32 LiveCount, int32 Capacity, double Now, const FJTSPopulationRecoverySettings& Settings);
	void Commit(int32 Spawned);

private:
	struct FCohort { double Start = 0; int32 Count = 0; int32 Released = 0; float RoundingPhase = 0; };
	TArray<FCohort> Cohorts;
	FRandomStream Random;
	double Credit = 0;
	int32 PreviousLive = 0;
};
