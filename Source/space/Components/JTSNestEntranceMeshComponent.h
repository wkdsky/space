#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "JTSNestEntranceMeshComponent.generated.h"

UENUM(BlueprintType)
enum class EJTSNestEntranceProfile : uint8
{
	IrregularArch,
	Olive,
	Slit,
	Leaning
};

/** Saved instance parameters; randomization is explicit, never performed during reconstruction. */
USTRUCT(BlueprintType)
struct FJTSNestEntranceShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	EJTSNestEntranceProfile Profile = EJTSNestEntranceProfile::IrregularArch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "10", ClampMax = "200", Units = "cm"))
	float Width = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "3", ClampMax = "150", Units = "cm"))
	float Height = 45.0f;

	/** Larger values taper the two ends, producing an olive or fissure outline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0.5", ClampMax = "3"))
	float ProfilePower = 1.0f;

	/** Rounds the lower outline up at the sides; zero keeps a floor-level arch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0", ClampMax = "1"))
	float FloorRoundness = 0.0f;

	/** Horizontal shear within the fitted wall plane, rather than rotating away from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "-0.8", ClampMax = "0.8"))
	float Lean = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0", ClampMax = "0.3"))
	float Irregularity = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	int32 Seed = 1;
};

/** Optional entrance presentation attached to a fallback static mesh. No gameplay or Tick. */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSNestEntranceMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UJTSNestEntranceMeshComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = RebuildEntrance, Category = "Nest Entrance")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = RebuildEntrance, Category = "Nest Entrance")
	FJTSNestEntranceShape Shape;

	/** Local wall is X + WallSlope * Z = 0. Supplied by the authored terrain fit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = RebuildEntrance, Category = "Nest Entrance|Surface")
	float WallSlope = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = RebuildEntrance, Category = "Nest Entrance", meta = (ClampMin = "0.5", ClampMax = "15", Units = "cm"))
	float RimWidth = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nest Entrance|Materials")
	TObjectPtr<UMaterialInterface> VoidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nest Entrance|Materials")
	TObjectPtr<UMaterialInterface> RimMaterial;

	UFUNCTION(BlueprintCallable, Category = "Nest Entrance")
	void RandomizeShapeWithSeed(int32 InSeed, EJTSNestEntranceProfile Profile);

	/** Rolls once and stores the result on this instance. */
	UFUNCTION(CallInEditor, Category = "Nest Entrance")
	void RandomizeEntrance();

	UFUNCTION(BlueprintCallable, Category = "Nest Entrance")
	void RebuildEntrance();

protected:
	virtual void OnRegister() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};
