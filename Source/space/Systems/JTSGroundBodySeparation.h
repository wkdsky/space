#pragma once

#include "CoreMinimal.h"
#include "JTSGroundBodySeparation.generated.h"

/** Position constraints only. These settings do not control navigation or desired velocity. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSGroundBodySeparationSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1", ClampMax = "12"))
	int32 Iterations = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1"))
	float CellSize = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0"))
	float Skin = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0"))
	float Tolerance = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0", ClampMax = "1"))
	float Relaxation = 0.8f;
	/** Total displacement budget across all iterations of one simulation step, in cm/second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0"))
	float MaxCorrectionSpeed = 900.0f;
	/** Absolute total correction cap per simulation step, including unusually long frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0"))
	float MaxCorrectionPerStep = 45.0f;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSGroundBodySeparationStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Collision") int32 Participants = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") int32 Iterations = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") int32 PairChecks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") int32 ResidualPairs = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") float MaxPenetration = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") float MaxStepCorrection = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Collision") float Milliseconds = 0;
};

/** Numeric snapshot of a circular ground footprint with a vertical extent. No UObject ownership. */
struct FJTSGroundBody
{
	uint64 Id = 0;
	uint32 Group = 0;
	int32 Layer = 0;
	FVector Center = FVector::ZeroVector;
	FVector Position = FVector::ZeroVector;
	float Radius = 45;
	float HalfHeight = 45;
	float InverseMass = 1;
	bool bParticipating = true;
};

/** Jacobi projection: read a complete snapshot, accumulate, constrain, then apply simultaneously. */
class SPACE_API FJTSGroundBodySeparation
{
public:
	using FConstraint = TFunctionRef<FVector(int32, const FVector&, const FVector&)>;
	static void Solve(TArray<FJTSGroundBody>& Bodies, const FJTSGroundBodySeparationSettings& Settings,
		float DeltaSeconds, FConstraint Constrain, FJTSGroundBodySeparationStats& Stats);
	static FVector CoincidentNormal(uint64 A, uint64 B, const FVector& Up);
};
