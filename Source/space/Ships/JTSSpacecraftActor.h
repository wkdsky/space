// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Core/JTSGameState.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSResourceTypes.h"

#include "JTSSpacecraftActor.generated.h"

class AJTSCharacter;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UJTSMoonWrappedActorComponent;
class UMaterialInterface;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShipResourcesChanged, int32, FuelCount, int32, WaterCount, int32, FoodCount);

/**
 * Receives player-carried resources and tracks the spacecraft's small Earth-stage inventory.
 */
UCLASS()
class SPACE_API AJTSSpacecraftActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSSpacecraftActor();

	/** Transfers all resources carried by a pawn into this spacecraft. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Resources")
	bool TryDepositResourcesFromPawn(APawn* InteractingPawn);

	/** Boards a character that is currently inside the spacecraft trigger. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Boarding")
	bool TryBoardPlayer(APawn* InteractingPawn);

	/** Immediately disembarks the currently boarded character during active exploration. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Boarding")
	bool TryDisembarkPlayer(APawn* InteractingPawn);

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool IsPlayerBoarded(const APawn* InteractingPawn) const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool HasBoardedPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	AJTSCharacter* GetBoardedPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool IsPawnInBoardingRange(const APawn* InteractingPawn) const;

	USceneComponent* GetBoardingPoint() const;
	USceneComponent* GetExitPoint() const;

	/** Physical spacecraft mesh bounds, excluding Fake Moon WPO culling expansion. */
	FBox GetResourceExclusionBounds() const;

	/** Bounds-top anchor shared by the world interaction prompt and Moon navigation marker. */
	UFUNCTION(BlueprintPure, Category = "Ship|Navigation")
	FVector GetNavigationMarkerWorldLocation() const;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	/** Returns the amount of one resource type currently stored in the spacecraft. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetResourceAmount(EJTSResourceType ResourceType) const;

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	bool HasResource(EJTSResourceType ResourceType, int32 ResourceAmount) const;

	/** Removes ResourceAmount units when the spacecraft has enough of the supplied resource. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Resources")
	bool TryConsumeResource(EJTSResourceType ResourceType, int32 ResourceAmount);

	/** Atomically removes a set of resource costs only when every amount is available. */
	bool TryConsumeResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts);

	/** Returns the number of fuel resources currently stored in the spacecraft. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetFuelCount() const;

	/** Returns the number of water resources currently stored in the spacecraft. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetWaterCount() const;

	/** Returns the number of food resources currently stored in the spacecraft. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetFoodCount() const;

	/** Returns the total number of resources currently stored in the spacecraft. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetTotalResourceCount() const;

	/** Adds each supplied resource to the matching spacecraft inventory count. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Resources")
	bool DepositResources(const TArray<EJTSResourceType>& Resources);

	/** Adds the supplied resource amounts to unbounded spacecraft storage. */
	bool DepositResourceAmounts(const TMap<EJTSResourceType, int32>& ResourceAmounts);

	/** Returns the active spacecraft storage, keyed by resource type. */
	const TMap<EJTSResourceType, int32>& GetStorage() const;

	/** Broadcast after a successful resource deposit. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Resources")
	FOnShipResourcesChanged OnShipResourcesChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBoardingTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleBoardingTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

private:
	bool IsEarthCollectionActive() const;
	bool IsMoonExplorationActive() const;
	bool DepositPlayerResources(AJTSCharacter* Player);
	void DepositResourcesFromOverlappingPlayers();
	void RestoreStorageForMoonTravel();
	void SaveStorageForMoonTravel() const;

	/** Non-visual transform root for the temporary spacecraft actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Temporary visible primitive used until a spacecraft model is available. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SpacecraftMesh;

	/** Pawn-only overlap volume used for automatic deposits and boarding. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> BoardingTrigger;

	/** Location where a boarded character is attached. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> BoardingPoint;

	/** Location where a character appears after disembarking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> ExitPoint;

	/** Supplies the single authoritative spacecraft with a nearest-image representation in Fake Moon worlds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	/** Optional shared Fake Moon WPO material. Earth worlds leave this untouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FakeMoonBendMaterial;

	/** Small visual clearance above the spacecraft mesh top for navigation and interaction labels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float NavigationMarkerHeightOffset = 44.0f;

	/** Unbounded resource storage used by both Earth and Moon collection. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TMap<EJTSResourceType, int32> Storage;

	/** Character currently attached to this spacecraft, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> BoardedPlayer;

	/** Character currently inside the boarding trigger, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> NearbyPlayer;
};
