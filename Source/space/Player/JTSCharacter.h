// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "space/Core/JTSGameState.h"

#include "JTSCharacter.generated.h"

class AJTSSpacecraftActor;
class AJTSPlanetAnchor;
class UCameraComponent;
class UJTSCarryComponent;
class UJTSHealthComponent;
class UJTSMeleeComponent;
class UJTSPlayerEquipmentComponent;
class UJTSPlanetGravityComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInteractionComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;
struct FJTSPlanetSurfaceFrame;

/** Selects how a character's body follows the camera while it is standing on a real spherical planet. */
UENUM(BlueprintType)
enum class EJTSPlanetBodyFacingMode : uint8
{
	/** Third-person action control: yaw follows the local gravity-relative camera tangent immediately. */
	FaceCamera UMETA(DisplayName = "Face Camera"),

	/** Conventional free-look control: body yaw follows movement direction in the local surface tangent. */
	OrientToMovement UMETA(DisplayName = "Orient To Movement")
};

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

	/** Returns the reusable player health pool used by UE's standard damage path. */
	UFUNCTION(BlueprintPure, Category = "Health")
	UJTSHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Camera")
	bool IsFirstPersonView() const;

	/** Binds this character to an explicit real gameplay planet. Earth and Legacy Fake Moon leave this unset. */
	UFUNCTION(BlueprintCallable, Category = "Planet")
	void SetGameplayPlanet(AJTSPlanetAnchor* InPlanetAnchor);

	UFUNCTION(BlueprintPure, Category = "Planet")
	AJTSPlanetAnchor* GetGameplayPlanet() const;

	/** Uses the real gameplay mesh collision to place the capsule just above the surface. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool SnapToPlanetSurface(AJTSPlanetAnchor* InPlanetAnchor, const FVector& TraceReferenceLocation);

	/** Current controller pitch normalized to the range consumed by character animation. */
	UFUNCTION(BlueprintPure, Category = "Player|Aim")
	float GetAimPitch() const;

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
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
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
	void HandleAttackStarted(const FInputActionValue& Value);
	void HandleAttackReleased(const FInputActionValue& Value);
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
	bool IsSpaceWorldSurfaceGameplayActive() const;
	bool IsRealPlanetGameplayActive() const;
	void UpdatePlanetGameplayFrame(float DeltaSeconds);
	FVector GetDesiredPlanetUp() const;
	FVector GetStablePlanetTangent(const FVector& UpVector, const FVector& PreferredDirection) const;
	void UpdatePlanetBodyOrientation(const FVector& DesiredUp, float DeltaSeconds);
	void UpdatePlanetCameraFrame(const FVector& CurrentUp, float DeltaSeconds);
	FVector GetPlanetCameraForward(const FVector& CurrentUp) const;
	void ApplyThirdPersonCameraOffset();
	void ApplyCameraView();
	void ApplyCameraPitchLimits();
	void RestoreAfterBoarding(AJTSSpacecraftActor* Spacecraft, bool bMoveToExitPoint);
	bool FindSafeCharacterSurfaceLocation(
		AJTSPlanetAnchor* InPlanetAnchor,
		const FVector& TraceReferenceLocation,
		const FVector& PreferredForward,
		const AActor* AdditionalIgnoredActor,
		FVector& OutLocation,
		FJTSPlanetSurfaceFrame* OutSurfaceFrame = nullptr) const;
	bool FindSafeDisembarkLocation(
		AJTSSpacecraftActor* Spacecraft,
		FVector& OutLocation,
		FJTSPlanetSurfaceFrame* OutSurfaceFrame = nullptr) const;
	bool FindGroundedSpacecraftDisembarkLocation(
		AJTSSpacecraftActor* Spacecraft,
		AJTSPlanetAnchor* Planet,
		FVector& OutLocation,
		FJTSPlanetSurfaceFrame* OutSurfaceFrame) const;
	bool FindLegacySafeDisembarkLocation(AJTSSpacecraftActor* Spacecraft, FVector& OutLocation) const;

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	static constexpr float WalkingSpeed = 500.0f;
	static constexpr float SprintingSpeed = 800.0f;
	static constexpr float BoardingHoldDuration = 2.0f;

	/** Stable camera origin attached to the capsule; both views rotate around this eye-height point. */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> CameraPivot;

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

	/** Shared player health state. Future weapons, monsters, and hazards use this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSHealthComponent> HealthComponent;

	/** One camera-agnostic Moon melee path for Punch, Knife, and Axe. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMeleeComponent> MeleeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlayerMaxHealth = 10.0f;

	/** Applies UE CharacterMovement custom gravity only while a real gameplay planet is active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSPlanetGravityComponent> PlanetGravityComponent;

	/** Explicit real-planet ownership. This prevents a character from selecting the first planet in the world. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> GameplayPlanet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlanetSurfaceSnapClearance = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetOrientationInterpolationSpeed = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetBodyTurnInterpolationSpeed = 12.0f;

	/**
	 * Blueprint-selectable third-person body behavior for real spherical planets only. Earth and the
	 * legacy flat Moon retain CharacterMovement's normal bOrientRotationToMovement behavior.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true"))
	EJTSPlanetBodyFacingMode PlanetBodyFacingMode = EJTSPlanetBodyFacingMode::FaceCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugPlanetSurface = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugPlanetCamera = false;

	/** Local height of the shared eye-level pivot above the capsule origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "200.0", UIMin = "0.0", UIMax = "150.0"))
	float CameraPivotHeight = 72.0f;

	/** Third-person boom length measured from CameraPivot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "2000.0", UIMin = "0.0", UIMax = "800.0"))
	float ThirdPersonArmLength = 400.0f;

	/** Third-person shoulder offset. Its Z value is ignored so camera height always comes from CameraPivot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true"))
	FVector ThirdPersonShoulderOffset = FVector(0.0f, 60.0f, 0.0f);

	/** Retained for existing Blueprint defaults; first-person camera position now always comes from CameraPivot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "First-person camera height is controlled by CameraPivotHeight."))
	FVector FirstPersonCameraOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float FirstPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float ThirdPersonFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-89.0", UIMax = "0.0"))
	float ThirdPersonViewPitchMin = -60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
	float ThirdPersonViewPitchMax = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-89.0", UIMax = "0.0"))
	float FirstPersonViewPitchMin = -85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
	float FirstPersonViewPitchMax = 85.0f;

	/** Multiplies raw Enhanced Input MouseX exactly once before controller yaw is updated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.001", UIMax = "1.0"))
	float MouseSensitivityX = 0.07f;

	/** Multiplies raw Enhanced Input MouseY exactly once before controller pitch is updated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.001", UIMax = "1.0"))
	float MouseSensitivityY = 0.07f;

	/** Lowest controller pitch supplied to the character animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "-90.0", ClampMax = "0.0", UIMin = "-90.0", UIMax = "0.0"))
	float AimPitchMin = -85.0f;

	/** Highest controller pitch supplied to the character animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0"))
	float AimPitchMax = 85.0f;

	/** Controller pitch normalized for the animation graph's upper-body aim adjustment. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Aim", meta = (AllowPrivateAccess = "true"))
	float AimPitch = 0.0f;

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
	TObjectPtr<UInputAction> AttackAction;

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
	bool bPlanetFrameInitialized = false;
	bool bPlanetCameraFrameInitialized = false;
	FVector LastPlanetUp = FVector::UpVector;
	FVector PlanetBodyForward = FVector::ForwardVector;
	FVector PlanetCameraTangentForward = FVector::ForwardVector;
	FVector LastPlanetCameraUp = FVector::UpVector;
	float PlanetCameraPitch = 0.0f;
	ECollisionEnabled::Type PreviousCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bPreviousDebugVisualVisible = true;
	bool bPreviousMeshVisible = true;
};
