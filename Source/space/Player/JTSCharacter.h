// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Core/JTSGameState.h"

#include "JTSCharacter.generated.h"

class AJTSSpacecraftActor;
class AJTSPlanetAnchor;
class AJTSPlayerState;
class UCameraComponent;
class UJTSCarryComponent;
class UJTSHealthComponent;
class UJTSInventoryComponent;
class UJTSPlanetGravityComponent;
class UJTSRangedWeaponComponent;
class UJTSWeaponVisualComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInteractionComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UAnimSequenceBase;
class UAnimMontage;
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

	/** Replicated general item inventory. It owns tools, weapons, resources, and worn-item data alike. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UJTSInventoryComponent* GetInventoryComponent() const;

	/** Returns the reusable player health pool used by UE's standard damage path. */
	UFUNCTION(BlueprintPure, Category = "Health")
	UJTSHealthComponent* GetHealthComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Camera")
	bool IsFirstPersonView() const;

	/** Reasserts the local Enhanced Input context after possession or seamless travel. */
	void EnsureGameplayInputMapping();

	/** Adjusts the third-person camera boom length. Positive wheel input zooms in. */
	UFUNCTION(BlueprintCallable, Category = "Player|Camera")
	void AdjustThirdPersonCameraDistance(float ScrollAmount);

	/** Binds this character to an explicit real gameplay planet. Earth leaves this unset. */
	UFUNCTION(BlueprintCallable, Category = "Planet")
	void SetGameplayPlanet(AJTSPlanetAnchor* InPlanetAnchor);

	UFUNCTION(BlueprintPure, Category = "Planet")
	AJTSPlanetAnchor* GetGameplayPlanet() const;

	/** Initializes the local planet/body/camera frame without tracing or moving the character. */
	UFUNCTION(BlueprintCallable, Category = "Planet")
	void InitializePlanetFrame();

	/** Begins normal CharacterMovement falling after gravity and the local frame have been bound. */
	UFUNCTION(BlueprintCallable, Category = "Planet")
	void BeginPlanetFalling();

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	bool IsPlanetGravityEnabled() const;

	/** Uses the real gameplay mesh collision to place the capsule just above the surface. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Surface")
	bool SnapToPlanetSurface(AJTSPlanetAnchor* InPlanetAnchor, const FVector& TraceReferenceLocation);

	/** Current controller pitch normalized to the range consumed by character animation. */
	UFUNCTION(BlueprintPure, Category = "Player|Aim")
	float GetAimPitch() const;

	/** Local camera response to a shot; gameplay hit traces remain server-owned. */
	void ApplyWeaponViewKick(float PitchDegrees);

	/** The HUD queries this state to draw the selected-item destroy hold ring. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemDiscardHoldSlotIndex() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetItemDiscardHoldProgress() const;

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

	/** Server-side safety net for resources collected after this character has already entered the ship range. */
	bool TryDepositCarriedResourcesToNearbySpacecraft();

	/** Applies the minimal attached/hidden state used while boarding. */
	bool EnterBoardedState(AJTSSpacecraftActor* Spacecraft);

	/** Restores movement and visibility after a normal disembark. */
	bool ExitBoardedState(AJTSSpacecraftActor* Spacecraft);

	/** Restores the character if its spacecraft is destroyed during teardown. */
	void HandleSpacecraftInvalidated(AJTSSpacecraftActor* Spacecraft);

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void InitializeInput();
	void RegisterInputMappingContext();
	void UnregisterInputMappingContext();
	void BindGameState();
	void UnbindGameState();
	void BindPlayerState();
	void UnbindPlayerState();

	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void GetMovementInputDirections(FVector& OutForward, FVector& OutRight) const;
	void LookYaw(const FInputActionValue& Value);
	void LookPitch(const FInputActionValue& Value);
	void StartSprint(const FInputActionValue& Value);
	void StopSprint(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleInteractStarted(const FInputActionValue& Value);
	void HandleBoardStarted(const FInputActionValue& Value);
	void HandleBoardTriggered(const FInputActionValue& Value);
	void HandleBoardCompleted(const FInputActionValue& Value);
	void HandleBoardCanceled(const FInputActionValue& Value);
	void HandleAttackStarted(const FInputActionValue& Value);
	void HandleAttackReleased(const FInputActionValue& Value);
	UFUNCTION()
	void HandleMeleeAttackStarted(EJTSAttackType AttackType);
	UFUNCTION()
	void HandleMeleeAttackFinished(EJTSAttackType AttackType);
	void HandleAimStarted(const FInputActionValue& Value);
	void HandleAimReleased(const FInputActionValue& Value);
	void HandleToggleCameraStarted(const FInputActionValue& Value);
	void HandleCameraZoom(const FInputActionValue& Value);
	void HandleQuickbarSlotOneStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotTwoStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotThreeStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotFourStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotFiveStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotSixStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotSevenStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotEightStarted(const FInputActionValue& Value);
	void HandleQuickbarSlotNineStarted(const FInputActionValue& Value);
	void HandlePreviousQuickbarPageStarted(const FInputActionValue& Value);
	void HandleNextQuickbarPageStarted(const FInputActionValue& Value);
	void HandleDiscardItemStarted(const FInputActionValue& Value);
	void HandleDiscardItemReleased(const FInputActionValue& Value);

	bool BeginBoardingHold();
	void CancelBoardingHold();
	void CompleteBoardingHold();
	AJTSSpacecraftActor* GetCurrentBoardingSpacecraft();
	void SelectQuickbarSlotByPage(int32 SlotIndexInPage);
	void BeginItemDiscardHold();
	void EndItemDiscardHold();
	void CancelItemDiscardHold();
	void CompleteItemDiscardHold();
	void ApplyProgressionMovementSpeed();
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
	void InitializeThirdPersonCameraDistance();
	void ApplyThirdPersonCameraOffset();
	void ApplyCameraView();
	void ApplyCameraPitchLimits();
	void UpdateAimCamera(float DeltaSeconds);
	void ShiftLocalViewPitch(float DeltaDegrees);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateAimPitch(float NewPitch);
	void PlayUnarmedPunchPresentation(bool bUseLeftPunch, bool bIsComboContinuation);
	void StopUnarmedPunchPresentation();
	void ApplySurfaceMovementSettings();
	bool RestoreAfterBoarding(AJTSSpacecraftActor* Spacecraft, bool bMoveToExitPoint);
	void ApplyBoardedPresentation();
	void ApplyAvatarColor();
	float GetCapsuleSupportDistanceAlongDirection(const FVector& SupportDirection, const FQuat& CapsuleRotation) const;
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
	bool IsDisembarkLocationClear(const FVector& Location, const FQuat& Rotation) const;

	UFUNCTION()
	void HandleGameplayPhaseChanged(EJTSGameplayPhase NewGameplayPhase);

	UFUNCTION()
	void HandlePlayerStateNetworkChanged();

	UFUNCTION()
	void HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_BoardedSpacecraft();

	UFUNCTION()
	void OnRep_GameplayPlanet();

	static constexpr float WalkingSpeed = 500.0f;
	static constexpr float SprintingSpeed = 800.0f;
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

	/** Legacy resource-only projection retained for Earth gameplay and old Blueprint references. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSCarryComponent> CarryComponent;

	/** Holds tools, weapons, ordinary items, and stackable materials. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSInventoryComponent> InventoryComponent;

	/** Shared player health state. Future weapons, monsters, and hazards use this component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSHealthComponent> HealthComponent;

	/** One camera-agnostic Moon melee path for Punch, Knife, and Axe. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMeleeComponent> MeleeComponent;

	/** Server-authoritative hitscan prototype for active RangedWeapon items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSRangedWeaponComponent> RangedWeaponComponent;

	/** Composed primitive mesh used as a network-safe placeholder for the active firearm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSWeaponVisualComponent> WeaponVisualComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlayerMaxHealth = 10.0f;

	/** Right and left punches are configured as assets while the shared melee component owns timing and damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> UnarmedPunchLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> UnarmedPunchRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float UnarmedPunchBlendInTime = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float UnarmedPunchBlendOutTime = 0.14f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Melee|Presentation", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float UnarmedPunchPlayRate = 1.42f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveUnarmedPunchMontage;

	/** Seconds F must be held while in a spacecraft boarding trigger before the player boards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boarding", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float BoardingHoldDuration = 2.0f;

	/** Applies UE CharacterMovement custom gravity only while a real gameplay planet is active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSPlanetGravityComponent> PlanetGravityComponent;

	/** Maximum terrain angle a character can stand on. Individual character Blueprints may tune this per project. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
	float MaxWalkableSlopeDegrees = 60.0f;

	/** Explicit real-planet ownership. This prevents a character from selecting the first planet in the world. */
	UPROPERTY(ReplicatedUsing = OnRep_GameplayPlanet, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> GameplayPlanet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlanetSurfaceSnapClearance = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetOrientationInterpolationSpeed = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetBodyTurnInterpolationSpeed = 12.0f;

	/**
	 * Blueprint-selectable third-person body behavior for real spherical planets only. Camera yaw is
	 * intentionally independent from body yaw; OrientToMovement is the normal third-person default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Orientation", meta = (AllowPrivateAccess = "true"))
	EJTSPlanetBodyFacingMode PlanetBodyFacingMode = EJTSPlanetBodyFacingMode::OrientToMovement;

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

	/** Closest third-person camera distance selectable with the mouse wheel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "2000.0", UIMin = "0.0", UIMax = "800.0"))
	float ThirdPersonCameraMinArmLength = 220.0f;

	/** Furthest third-person camera distance selectable with the mouse wheel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "3000.0", UIMin = "400.0", UIMax = "1600.0"))
	float ThirdPersonCameraMaxArmLength = 1000.0f;

	/** Camera-boom change, in centimeters, for one mouse-wheel step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", UIMax = "300.0"))
	float ThirdPersonCameraZoomStep = 80.0f;

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

	/** FOV used while holding the right mouse aim input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0", UIMin = "30.0", UIMax = "170.0"))
	float AimFOV = 60.0f;

	/** Additional third-person shoulder offset while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Aim", meta = (AllowPrivateAccess = "true"))
	FVector AimShoulderOffset = FVector(30.0f, 48.0f, 0.0f);

	/** Aim camera transition speed in seconds^-1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AimCameraInterpSpeed = 12.0f;

	/** Third-person arm length target while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera|Aim", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float AimThirdPersonArmLength = 260.0f;

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

	UPROPERTY(Replicated, Transient)
	float ReplicatedAimPitch = 0.0f;

	float PendingViewRecoilDegrees = 0.0f;
	double LastAimPitchSendSeconds = -100.0;
	float LastSentAimPitch = 0.0f;

	/** Seconds G must be held before the selected item is permanently destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float ItemDestroyHoldDuration = 0.8f;

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
	TObjectPtr<UInputAction> BoardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleCameraAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> QuickbarSlotActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PreviousQuickbarPageAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NextQuickbarPageAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DiscardItemAction;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	TWeakObjectPtr<AJTSGameState> BoundGameState;
	TWeakObjectPtr<AJTSPlayerState> BoundPlayerState;
	TWeakObjectPtr<AJTSSpacecraftActor> NearbySpacecraft;
	UPROPERTY(ReplicatedUsing = OnRep_BoardedSpacecraft, Transient)
	TObjectPtr<AJTSSpacecraftActor> BoardedSpacecraft;
	TWeakObjectPtr<AJTSSpacecraftActor> BoardingSpacecraft;

	FTimerHandle BoardingHoldTimerHandle;
	FTimerHandle ItemDiscardHoldTimerHandle;
	double BoardingHoldStartTime = 0.0;
	double ItemDiscardHoldStartTime = 0.0;
	int32 HeldItemDiscardSlotIndex = INDEX_NONE;
	bool bBoardingHoldActive = false;
	bool bInteractKeyHeld = false;
	bool bItemDiscardHoldCompleted = false;
	bool bSprintInputActive = false;
	bool bFirstPersonView = false;
	bool bThirdPersonCameraDistanceInitialized = false;
	bool bPlanetFrameInitialized = false;
	bool bPlanetCameraFrameInitialized = false;
	FVector LastPlanetUp = FVector::UpVector;
	FVector PlanetBodyForward = FVector::ForwardVector;
	FVector PlanetCameraTangentForward = FVector::ForwardVector;
	FVector LastPlanetCameraUp = FVector::UpVector;
	float PlanetCameraPitch = 0.0f;
	float CurrentThirdPersonCameraArmLength = 0.0f;
	float AimCameraAlpha = 0.0f;
	ECollisionEnabled::Type PreviousCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bBoardedPresentationApplied = false;
	bool bPreviousDebugVisualVisible = true;
	bool bPreviousMeshVisible = true;
};
