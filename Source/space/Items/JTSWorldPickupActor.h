#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSWorldPickupItemType.h"

#include "JTSWorldPickupActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class APawn;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;
struct FHitResult;

/** One manually collected Moon world item. It can represent resources or a unique equipment item. */
UCLASS()
class SPACE_API AJTSWorldPickupActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSWorldPickupActor();

	/** Spawns one non-physical pickup on nearby valid ground. PreferredDirection biases shop drops forward of a player. */
	static AJTSWorldPickupActor* SpawnGroundedPickup(
		UWorld* World,
		EJTSWorldPickupItemType NewItemType,
		const FVector& Origin,
		APawn* SafetyPawn,
		AActor* SourceActor,
		const FVector& PreferredDirection = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category = "Pickup")
	EJTSWorldPickupItemType GetItemType() const;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	FText GetItemDisplayName() const;

	/** Bounds-based points used for forgiving target acquisition and world-space prompt placement. */
	UFUNCTION(BlueprintPure, Category = "Pickup|Interaction")
	FVector GetInteractionTargetWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Pickup|Interaction")
	FVector GetInteractionAnchorWorldLocation() const;

	/** Sets this pickup's single-item payload before deferred spawning completes. */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void InitializeItem(EJTSWorldPickupItemType NewItemType);

	/** World-space visual extent used by ground-placement helpers. */
	FVector GetVisualBoundsExtent() const;
	void AdjustToGround(const FVector& GroundHitLocation);

	/** Starts the lightweight non-physics drop movement after safe placement succeeds. */
	void StartDropMotion(
		const FVector& InitialVelocity,
		const FVector& GravityAcceleration,
		const FVector& SafeGroundLocation,
		AActor* SourceActor,
		APawn* SafetyPawn);

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool TryPickup(APawn* InteractingPawn, FString& OutFailureReason);
	bool IsResourceItem() const;
	float GetVisualSupportDistance(const FVector& GravityDirection) const;
	bool TraceDropGround(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutGroundHit) const;
	void BuildDropTraceIgnoredActors(AActor* SourceActor, APawn* SafetyPawn);
	void SettleDropOnGround(const FVector& GroundHitLocation);
	void UpdateMoonWrappedLogicalPosition();
	void ConfigureAppearance();
	void ApplyItemAppearance();
	void ShowFailureFeedback(const FString& FailureReason);
	FText GetFailureFeedback() const;
	static FString ItemTypeToString(EJTSWorldPickupItemType InItemType);

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Placeholder primitive only. It has no collision, overlap, or physics behavior. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Reuses the existing Moon wrapping and Fake World WPO behavior. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FakeMoonBendMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RockMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> OreMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> EquipmentMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	EJTSWorldPickupItemType ItemType = EJTSWorldPickupItemType::Rock;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PickupMaterial;

	/** A failed manual pickup remains attached to this target instead of drawing over the inventory. */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup|Interaction", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float FailureFeedbackDuration = 1.0f;

	FVector DropVelocity = FVector::ZeroVector;
	FVector DropGravityAcceleration = FVector::ZeroVector;
	FVector PlannedGroundLocation = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>> DropTraceIgnoredActors;
	float DropElapsedSeconds = 0.0f;
	double FailureFeedbackEndTime = 0.0;
	FString FailureFeedbackText;
	bool bIsDropping = false;
	bool bPickupConsumed = false;
};
