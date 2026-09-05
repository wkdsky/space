// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	/** Immediately disembarks the currently boarded character during Earth collection. */
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

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

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

private:
	bool IsEarthCollectionActive() const;

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

	/** Fuel resources deposited into the spacecraft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	int32 FuelCount = 0;

	/** Water resources deposited into the spacecraft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	int32 WaterCount = 0;

	/** Food resources deposited into the spacecraft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	int32 FoodCount = 0;

	/** Character currently attached to this spacecraft, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> BoardedPlayer;

	/** Character currently inside the boarding trigger, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> NearbyPlayer;
};
