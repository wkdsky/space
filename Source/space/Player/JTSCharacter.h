// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "space/Core/JTSGameState.h"

#include "JTSCharacter.generated.h"

class AJTSSpacecraftActor;
class UCameraComponent;
class UJTSCarryComponent;
class UJTSPlayerEquipmentComponent;
class UJTSPlanetGravityComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInteractionComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * First playable native character for Jump to Space.
 */
UCLASS()
class SPACE_API AJTSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJTSCharacter();

	/** Returns this character's resource carry inventory. */
	UFUNCTION(BlueprintPure, Category = "Carry")
	UJTSCarryComponent* GetCarryComponent() const;

	/** Returns this character's four-slot equipment loadout. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UJTSPlayerEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Camera")
	bool IsFirstPersonView() const;

	/** The HUD queries this state to draw an equipment-slot hold ring. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEquipmentHoldSlotIndex() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetEquipmentHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	bool IsBoardingHoldActive() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	float GetBoardingHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	float GetBoardingHoldRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	bool IsBoarded() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	AJTSSpacecraftActor* GetNearbySpacecraft() const;

	UFUNCTION(BlueprintPure, Category = "Boarding")
	AJTSSpacecraftActor* GetBoardedSpacecraft() const;

	/** Called by a spacecraft's pawn-only trigger when this character enters. */
	void NotifySpacecraftEntered(AJTSSpacecraftActor* Spacecraft);

	/** Called by a spacecraft's pawn-only trigger when this character exits. */
	void NotifySpacecraftExited(AJTSSpacecraftActor* Spacecraft);

	/** Applies the minimal attached/hidden state used while boarding. */
	bool EnterBoardedState(AJTSSpacecraftActor* Spacecraft);

	/** Restores movement and visibility after a normal disembark. */
	void ExitBoardedState(AJTSSpacecraftActor* Spacecraft);

	/** Restores the character if its spacecraft is destroyed during teardown. */
	void HandleSpacecraftInvalidated(AJTSSpacecraftActor* Spacecraft);

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void InitializeInput();
	void RegisterInputMappingContext();
	void UnregisterInputMappingContext();
	void BindGameState();
	void UnbindGameState();

	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void GetMovementInputDirections(FVector& OutForward, FVector& OutRight) const;
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleInteractStarted(const FInputActionValue& Value);
	void HandleInteractCompleted(const FInputActionValue& Value);
	void HandleInteractCanceled(const FInputActionValue& Value);
	void HandleToggleCameraStarted(const FInputActionValue& Value);
	void HandleEquipmentSlotOneStarted(const FInputActionValue& Value);
	void HandleEquipmentSlotTwoStarted(const FInputActionValue& Value);
	void HandleEquipmentSlotThreeStarted(const FInputActionValue& Value);
	void HandleEquipmentSlotFourStarted(const FInputActionValue& Value);
	void HandleEquipmentSlotOneReleased(const FInputActionValue& Value);
	void HandleEquipmentSlotTwoReleased(const FInputActionValue& Value);
	void HandleEquipmentSlotThreeReleased(const FInputActionValue& Value);
	void HandleEquipmentSlotFourReleased(const FInputActionValue& Value);

	void BeginBoardingHold();
	void CancelBoardingHold();
	void CompleteBoardingHold();
	void BeginEquipmentSlotHold(int32 SlotIndex);
	void EndEquipmentSlotHold(int32 SlotIndex);
	void CancelEquipmentSlotHold();
	void CompleteEquipmentSlotHold();
	bool CanUseNormalGameplayInput() const;
	bool IsGameplayInputBlocked() const;
	void ApplyThirdPersonCameraOffset();
	void ApplyCameraView();
	void RestoreAfterBoarding(AJTSSpacecraftActor* Spacecraft, bool bMoveToExitPoint);
	bool FindSafeDisembarkLocation(AJTSSpacecraftActor* Spacecraft, FVector& OutLocation) const;

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	static constexpr float WalkingSpeed = 500.0f;
	static constexpr float SprintingSpeed = 800.0f;
	static constexpr float BoardingHoldDuration = 2.0f;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Temporary primitive visual; replace with a production character mesh later. */
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	TObjectPtr<UStaticMeshComponent> DebugVisual;

	/** Reusable nearby-target detection and interaction execution for this player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInteractionComponent> InteractionComponent;

	/** Fixed-capacity resource carry inventory for the current player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSCarryComponent> CarryComponent;

	/** Separate fixed-capacity equipment loadout; never stores ordinary resources. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSPlayerEquipmentComponent> EquipmentComponent;

	/** Legacy radial-gravity component retained for Blueprint compatibility; Moon fake worlds bypass it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Gravity", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Fake Moon uses standard World-Z gravity."))
	TObjectPtr<UJTSPlanetGravityComponent> PlanetGravityComponent;

	/** Camera socket offset used to keep the character left and below the centered crosshair. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true"))
	FVector ThirdPersonShoulderOffset = FVector(0.0f, 90.0f, 80.0f);

	/** Eye-level socket offset used while the spring arm is collapsed for first person. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true"))
	FVector FirstPersonCameraOffset = FVector(0.0f, 0.0f, 66.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float FirstPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float ThirdPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float EquipmentHoldToDropDuration = 0.8f;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookYawAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookPitchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleCameraAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> EquipmentSlotActions;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	TWeakObjectPtr<AJTSGameState> BoundGameState;
	TWeakObjectPtr<AJTSSpacecraftActor> NearbySpacecraft;
	TWeakObjectPtr<AJTSSpacecraftActor> BoardedSpacecraft;

	FTimerHandle BoardingHoldTimerHandle;
	FTimerHandle EquipmentHoldTimerHandle;
	double BoardingHoldStartTime = 0.0;
	double EquipmentHoldStartTime = 0.0;
	int32 HeldEquipmentSlotIndex = INDEX_NONE;
	bool bBoardingHoldActive = false;
	bool bInteractKeyHeld = false;
	bool bEquipmentHoldCompleted = false;
	bool bFirstPersonView = false;
	ECollisionEnabled::Type PreviousCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bPreviousDebugVisualVisible = true;
	bool bPreviousMeshVisible = true;
};
