#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSMoonLoopGroundActor.generated.h"

class APawn;
class UBoxComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneComponent;
struct FPropertyChangedEvent;

/** Reusable 3x3 ring of flat gameplay tiles around the continuously moving local player. */
UCLASS()
class SPACE_API AJTSMoonLoopGroundActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonLoopGroundActor();

	UFUNCTION(BlueprintCallable, Category = "Moon|Ground")
	void RebuildTiles();

	UFUNCTION(BlueprintPure, Category = "Moon|Ground")
	FIntPoint GetCurrentTile() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Ground")
	int32 GetTileCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool IsMoonWorld() const;
	void SetMoonWorldEnabled(bool bEnabled);
	void ResolveMapSize();
	void BuildVisualMeshes();
	void ApplyVisualMaterial();
	void ConfigureVisualTileRenderState(UProceduralMeshComponent* VisualTile) const;
	void LogVisualTileDiagnostics() const;
	void InvalidateTileAssignments();
	void UpdateTileRing(const FVector& PlayerLocation, UPrimitiveComponent* MovementBase);
	bool InitializeTileRing(const FIntPoint& DesiredCenter, UPrimitiveComponent* MovementBase);
	bool RecycleTiles(const FIntPoint& DesiredCenter, UPrimitiveComponent* MovementBase);
	bool ReassignTilePairs(
		const TArray<int32>& PairIndices,
		const TArray<FIntPoint>& DestinationCoordinates,
		UPrimitiveComponent* MovementBase,
		bool bLogTransitions);
	bool CanPositionTilePair(int32 PairIndex, const FIntPoint& TileCoordinate, UPrimitiveComponent* MovementBase) const;
	bool PositionTilePair(int32 PairIndex, const FIntPoint& TileCoordinate, bool bLogTransition);
	bool ValidateTileAssignments(const FIntPoint& ExpectedCenter) const;
	bool IsTileCoordinateInRing(const FIntPoint& TileCoordinate, const FIntPoint& CenterTile) const;
	int32 FindTilePairForPhysicalComponent(const UPrimitiveComponent* PhysicalComponent) const;
	UPrimitiveComponent* GetMovementBaseForPawn(APawn* LocalPawn) const;
	FIntPoint CalculateTileCoordinate(const FVector& PlayerLocation) const;
	FVector GetTileWorldLocation(const FIntPoint& TileCoordinate) const;
	FVector GetPhysicalTileWorldLocation(const FIntPoint& TileCoordinate) const;
	float GetVisualBoundsScale() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float VisualGridCellSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float CollisionThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> VisualMaterial;

	/** Optional master material that contains the shared JTSFakeMoon bend function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Ground|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FakeMoonBendMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Ground|Debug", meta = (AllowPrivateAccess = "true"))
	bool bEnableDebugLogging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UProceduralMeshComponent>> VisualTiles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ground", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UBoxComponent>> PhysicalTiles;

	FVector2D CachedMapSize = FVector2D(24000.0f, 24000.0f);

	TArray<FIntPoint> AssignedTileCoordinates;
	FIntPoint CurrentTile = FIntPoint::ZeroValue;
	bool bHasCurrentTile = false;
	bool bVisualMeshesBuilt = false;
	bool bMoonWorldEnabled = false;
};
