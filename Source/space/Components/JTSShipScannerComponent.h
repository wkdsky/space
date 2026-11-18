#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "space/Items/JTSDiscoveryTypes.h"
#include "JTSShipScannerComponent.generated.h"

class AJTSWorldPickupActor;
class UProceduralMeshComponent;
class UMaterialInterface;

USTRUCT()
struct FJTSGroundScanState
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AJTSWorldPickupActor> Target;
	UPROPERTY() FJTSItemInstance Sample;
	UPROPERTY() float Duration = 0;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() double StartedAt = 0;
	UPROPERTY() double CompletedAt = 0;
	UPROPERTY() bool bUnknown = false;
	UPROPERTY() bool bRunning = false;
	UPROPERTY() bool bComplete = false;
	UPROPERTY() bool bStorageBlocked = false;
};

/** Built-in ship receiver. Samples remain real world objects until an atomic server deposit. */
UCLASS(ClassGroup=(Ship), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSShipScannerComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UJTSShipScannerComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	const FJTSGroundScanState& GetScan() const { return Scan; }
	bool IsZoneActive() const { return bZoneActive; }
	FTransform GetGroundFrame() const { return GroundFrame; }
	bool ContainsPoint(const FVector& Point) const;
	float GetScanElapsed() const;
	float GetScanProgress() const;
	float DurationForSpan(float Span) const;
	double ServerTime() const;
	bool PlaceCarriedSample(APawn* Pawn,int32 Slot,FGuid ExpectedId);
	bool RetrieveCargo(APawn* Pawn,int32 Slot,FGuid ExpectedId);
	const TArray<FJTSItemInstance>& GetCargo() const { return Cargo; }
	/** Called by expedition restore, including a legacy sample left on the removed analysis table. */
	void RestoreState(const TArray<FJTSItemInstance>& SavedCargo,const FJTSStoredAnalysisSample& SavedSample);
	FJTSStoredAnalysisSample ExportPendingSample() const;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") TObjectPtr<UJTSAnalysisCatalog> AnalysisCatalog;
	/** Local Y is the front face of the existing central display. Z is resolved against real ground. */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") FVector OffsetFromScreen = FVector(0,200,0);
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner",meta=(ClampMin="40")) float ZoneRadius = 140;
	/** Optical coverage extends beyond the collection boundary, including both end chords. */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner",meta=(ClampMin="1.15")) float ScanCoverageScale = 1.35f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") float ZoneHeight = 150;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") float SmallSampleSeconds = 3;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") float LargeSampleSeconds = 16;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner") float LargeSampleSpan = 180;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner",meta=(ClampMin="1")) int32 CargoCapacity = 24;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner|Visual") TObjectPtr<UMaterialInterface> ProjectionMaterial;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner|Visual") TObjectPtr<UMaterialInterface> CollectionMaterial;
	/** Camera optical origin in the central screen frame; authored with the terminal Blueprint. */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner|Visual") FVector CameraOffsetFromScreen = FVector(0,0,65);
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Scanner|Visual",meta=(ClampMin="10")) float ColumnHeight = 220;
private:
	void AdvanceScan();
	bool ResolveGround(FTransform& Out) const;
	bool CanStore(const FJTSItemInstance& Item) const;
	bool Store(const FJTSItemInstance& Item);
	void Persist();
	void ResetScan();
	void BuildProjection();
	UFUNCTION() void OnRep_Zone();
	UPROPERTY(ReplicatedUsing=OnRep_Zone) bool bZoneActive = false;
	UPROPERTY(Replicated) FTransform GroundFrame;
	UPROPERTY(Replicated) FJTSGroundScanState Scan;
	UPROPERTY(Replicated) TArray<FJTSItemInstance> Cargo;
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Projection;
	FJTSStoredAnalysisSample PendingRestore;
	FTimerHandle ScanTimer;
	float LastCheckpoint = 0;
};
