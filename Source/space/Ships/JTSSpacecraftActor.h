// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSSpacecraftGroundProbeComponent.h"
#include "space/Core/JTSGameState.h"
#include "space/Core/JTSExpeditionTypes.h"
#include "space/Interaction/IInteractable.h"
#include "space/Items/JTSItemTypes.h"
#include "space/Items/JTSResourceTypes.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSSpacecraftActor.generated.h"

class AJTSCharacter;
class AJTSPlayerState;
class AJTSPlanetAnchor;
class AJTSPlanetLandingSite;
class AJTSPlanetSurfaceAnchor;
class AController;
class APlayerController;
class UBoxComponent;
class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UJTSSpacecraftFlightMovementComponent;
struct FHitResult;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShipResourcesChanged, int32, FuelCount, int32, WaterCount, int32, FoodCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipBoostStateChanged, bool, bIsBoosting);

/**
 * Receives player-carried resources and tracks the spacecraft's small Earth-stage inventory.
 */
UCLASS()
class SPACE_API AJTSSpacecraftActor : public APawn, public IInteractable
{
	GENERATED_BODY()

public:
	AJTSSpacecraftActor();

	/** Transfers all resources carried by a pawn into this spacecraft. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Resources")
	bool TryDepositResourcesFromPawn(APawn* InteractingPawn);

	/** Server-authoritative purchase backed by this ship's shared material storage. */
	EJTSShopPurchaseResult TryPurchase(AJTSCharacter* Player, EJTSItemId ItemId);

	/** Boards a character that is currently inside the spacecraft trigger. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Boarding")
	bool TryBoardPlayer(APawn* InteractingPawn);

	/** Server-authoritative disembark; succeeds only while the boarded craft is parked. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Boarding")
	bool TryDisembarkPlayer(APawn* InteractingPawn);
	bool TryDisembarkPlayerForController(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool IsPlayerBoarded(const APawn* InteractingPawn) const;

	/**
	 * A boarded player may exit only while the craft is parked: Earth collection treats its static
	 * launch craft as parked, while SpaceWorld requires a completed real-planet landing.
	 */
	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool CanDisembarkPlayer(const APawn* InteractingPawn) const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool HasBoardedPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	AJTSCharacter* GetBoardedPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	const TArray<FJTSSpacecraftOccupantState>& GetOccupants() const { return Occupants; }

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	AJTSPlayerState* GetDriverPlayerState() const { return DriverPlayerState; }

	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	bool IsPawnInBoardingRange(const APawn* InteractingPawn) const;

	/** Center of the shared boarding/deposit/workshop interaction volume. */
	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	FVector GetBoardingInteractionCenter() const;

	/** Radius of the shared boarding/deposit/workshop interaction volume. */
	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	float GetBoardingInteractionRadius() const;

	/** Closest physical spacecraft-mesh bounds point used by camera-cone and LOS interaction targeting. */
	UFUNCTION(BlueprintPure, Category = "Ship|Boarding")
	FVector GetBoardingInteractionTargetWorldLocation(const FVector& ReferenceLocation) const;

	/**
	 * Furthest physical hull projection from this actor's root in a world-space direction.
	 * Used to keep a disembarking character outside Blueprint-authored visual hulls, rather than
	 * relying on the smaller flight collision proxy.
	 */
	float GetExteriorHullSupportDistance(const FVector& WorldDirection) const;

	USceneComponent* GetBoardingPoint() const;
	USceneComponent* GetExitPoint() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	UJTSSpacecraftFlightMovementComponent* GetFlightMovementComponent() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Ground Probe")
	UJTSSpacecraftGroundProbeComponent* GetGroundProbeComponent() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Ground Probe")
	FJTSSpacecraftGroundInfo GetGroundInfo() const;

	/** Refreshes the independent ground probe. LandingManager supplies rule-specific probe lengths. */
	bool RefreshGroundInfo(AJTSPlanetAnchor* Planet, float MaxProbeDistance = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Ship|Camera")
	USpringArmComponent* GetFlightCameraBoom() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Camera")
	UCameraComponent* GetFlightCamera() const;

	/** Ensures the ship uses its exterior driving camera rather than a cockpit/legacy camera component. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Camera")
	void ActivateFlightCameraThirdPerson();

	void EnsureFlightInputMapping();

	/** Adjusts the driving camera boom length. Positive wheel input zooms in. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Camera")
	void AdjustFlightCameraDistance(float ScrollAmount);

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	bool IsBoosting() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	float GetCurrentSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	float GetSpeedNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	float GetThrottleNormalized() const;

	/** Called by SpaceWorld after a target planet is selected. */
	void SetFlightTargetPlanet(class AJTSPlanetAnchor* Planet);

	/** Configures an independently spawned arrival craft to fly under a specific planet's gravity. */
	void InitializeForPlanetArrival(AJTSPlanetAnchor* Planet);

	/** Starts an explicit developer-authorized landing request. Flying itself never depends on LandingSite. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Landing")
	bool RequestLanding();

	/** Called only by AJTSPlanetLandingManager after Site-union and surface validation succeed. */
	bool BeginLandingAssist(const FJTSPlanetLandingValidationResult& ValidationResult, float DurationSeconds);

	/** Cancels a pending request/assist and returns the craft to normal Flying state. */
	void CancelLandingRequest(EJTSLandingValidationFailure Failure);

	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	EJTSSpacecraftFlightState GetFlightState() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	bool IsLanded() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	EJTSLandingValidationFailure GetLastLandingFailure() const;

	/** Concise status for the visible controlled-landing flow. */
	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	EJTSSpacecraftLandingAssistPhase GetLandingAssistPhase() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Landing")
	AJTSPlanetAnchor* GetLandedPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Flight")
	AJTSPlanetAnchor* GetFlightPlanet() const;

	/** Compatibility entry point for callers that already resolved a surface-aligned landing transform. */
	bool BeginAssistedLanding(const FTransform& LandingTransform, float DurationSeconds);

	/** Marks this persistent spacecraft as parked on one real gameplay planet and disables flight movement. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Surface")
	void SetGroundedPlanet(AJTSPlanetAnchor* InPlanetAnchor);

	/** Restores normal flight-component activation after leaving a parked real-planet surface. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Surface")
	void ClearGroundedPlanet();

	/** Starts the minimal SpaceWorld surface takeoff and hands travel-state progression to SpaceWorldManager. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Surface")
	bool BeginSurfaceTakeoff();

	UFUNCTION(BlueprintPure, Category = "Ship|Surface")
	AJTSPlanetAnchor* GetGroundedPlanet() const;

	UFUNCTION(BlueprintPure, Category = "Ship|Surface")
	bool IsGroundedOnPlanet() const;

	/** Places the ship above a resolved real-mesh surface frame. The transform's local Z is the impact normal. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Surface")
	bool SnapSpacecraftToSurfaceTransform(AJTSPlanetAnchor* InPlanetAnchor, const FTransform& SurfaceTransform);

	/** Resolves an authored anchor, then parks this ship on that real gameplay surface. */
	UFUNCTION(BlueprintCallable, Category = "Ship|Surface")
	bool SnapSpacecraftToSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor);

	/** Tests the flight collision hull at a candidate landing transform without moving the ship. */
	bool CanOccupyLandingTransform(const FTransform& LandingTransform) const;

	/** Hull support distance plus configured ground clearance along the supplied surface-up direction. */
	float GetLandingCollisionClearance(const FVector& SurfaceUp) const;
	float GetLandingCollisionClearance() const;
	/** Computes hull support using the requested future ship orientation instead of the current flight attitude. */
	float GetLandingCollisionClearanceForRotation(const FQuat& ShipRotation, const FVector& SurfaceUp) const;

	/** Resolves the documented landed-ship respawn result through the PlanetLandingManager. */
	bool GetPlayerRespawnTransform(FJTSPlayerRespawnTransformResult& OutResult) const;
	bool GetTopRespawnTransform(FTransform& OutTransform) const;
	bool GetExitRespawnTransform(FTransform& OutTransform) const;

	float GetPlayerRespawnSearchRadius() const;
	float GetPlayerRespawnCapsuleRadius() const;
	float GetPlayerRespawnCapsuleHalfHeight() const;
	float GetPlayerRespawnClearance() const;

	/** Physical spacecraft mesh bounds, excluding render-only bounds expansion. */
	FBox GetResourceExclusionBounds() const;

	/** Physical bounds-top anchor shared by the world interaction prompt and Moon navigation marker. */
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

	/** Server-only restore path used by UJTSExpeditionSubsystem after seamless travel. */
	void RestoreStorageFromExpedition(const TMap<EJTSResourceType, int32>& NewStorage);

	/** Restores the server-owned expedition snapshot once this ship becomes the active persistent runtime spacecraft. */
	void RestorePersistentStorage();

	/** Compatibility entry point for existing Moon surface code. */
	void RestoreStorageForMoonTravel();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Driver intent only. Server clamps and applies input to the authoritative movement component. */
	UFUNCTION(Server, Unreliable)
	void ServerSetFlightInput(const FJTSSpacecraftInputState& InputState);

	UFUNCTION(Server, Reliable)
	void ServerRequestLanding();

	UFUNCTION(Server, Reliable)
	void ServerRequestSurfaceTakeoff();

	/** Driver-only request sent through the possessed spacecraft's owning connection. */
	UFUNCTION(Server, Reliable)
	void ServerRequestDisembark();

	/** Broadcast after a successful resource deposit. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Resources")
	FOnShipResourcesChanged OnShipResourcesChanged;

	/** Presentation hook for future thruster sound, VFX, and upgrade feedback. */
	UPROPERTY(BlueprintAssignable, Category = "Ship|Flight")
	FOnShipBoostStateChanged OnBoostStateChanged;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FJTSBoardingRegression;
#endif

	void InitializeFlightInput();
	void RegisterFlightInputMappingContext();
	void UnregisterFlightInputMappingContext();
	void FlightMoveForward(const FInputActionValue& Value);
	void FlightMoveRight(const FInputActionValue& Value);
	void FlightMoveVertical(const FInputActionValue& Value);
	void FlightRoll(const FInputActionValue& Value);
	void FlightLookYaw(const FInputActionValue& Value);
	void FlightLookPitch(const FInputActionValue& Value);
	void FlightCameraZoom(const FInputActionValue& Value);
	void FlightBoostStarted(const FInputActionValue& Value);
	void FlightBoostStopped(const FInputActionValue& Value);
	void FlightBrakeStarted(const FInputActionValue& Value);
	void FlightBrakeStopped(const FInputActionValue& Value);
	void FlightLandingStarted(const FInputActionValue& Value);
	void FlightDisembarkStarted(const FInputActionValue& Value);
	void FlightDisembarkReleased(const FInputActionValue& Value);
	void ProcessDeferredDisembarkRequest();
	void UpdateDisembarkInputGate();
	void InitializeFlightCameraDistance();
	void UpdateFlightCamera(float DeltaSeconds);
	FTransform GetFlightCollisionTransformForSpacecraftTransform(const FTransform& SpacecraftTransform) const;
	void HandleAssistedLandingCompleted();
	void HandleAssistedLandingFailed(EJTSLandingValidationFailure Failure);
	void HandleAssistedLandingPhaseChanged(EJTSSpacecraftLandingAssistPhase NewPhase);
	void SubmitFlightInput();
	void ApplyFlightInputOnServer(const FJTSSpacecraftInputState& InputState);
	void SyncReplicatedStorage();
	void RebuildStorageFromReplicatedArray();
	AJTSCharacter* FindBoardedCharacterForPlayerState(const AJTSPlayerState* InPlayerState) const;
	bool IsPlayerStateOccupying(const AJTSPlayerState* InPlayerState) const;
	void RemoveOccupant(const AJTSPlayerState* InPlayerState);

	UFUNCTION()
	void OnRep_Storage();

	UFUNCTION()
	void OnRep_Occupants();

	UFUNCTION()
	void OnRep_FlightState();

	UFUNCTION()
	void HandleFlightBoostStateChanged(bool bIsBoosting);
	bool IsEarthCollectionActive() const;
	bool IsMoonExplorationActive() const;
	bool IsSpaceWorldSurfaceActive() const;
	bool IsSpaceWorldRuntimeActive() const;
	bool IsMoonSurfaceRuntimeActive() const;
	bool DepositPlayerResources(AJTSCharacter* Player);
	bool TryDepositPlayerMaterials(AJTSCharacter* Player);
	bool BuildShopCosts(EJTSItemId ItemId, TMap<EJTSResourceType, int32>& OutCosts) const;
	bool DeliverShopPurchase(AJTSCharacter* Player, const FJTSItemInstance& Item, bool& bOutDropped);
	void DepositResourcesFromOverlappingPlayers();
	/** Reconciles occupants once after all startup BeginPlay calls have completed. */
	void ReconcileInitialBoardingOverlaps();
	void SavePersistentStorage() const;
	void UpdateBoardingTriggerFromSpacecraftMeshBounds();
	void UpdateFlightCollisionFromSpacecraftMeshBounds();
	bool GetPhysicalSpacecraftMeshLocalBounds(FBox& OutLocalBounds) const;

	/** Collision root moved by the flight component with Sweep enabled. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Flight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> FlightCollision;

	/** Non-visual transform root for existing spacecraft content. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Temporary visible primitive used until a spacecraft model is available. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SpacecraftMesh;

	/** Dedicated arcade flight movement. It owns all velocity, damping, boost, and collision movement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Flight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSSpacecraftFlightMovementComponent> FlightMovementComponent;

	/** Surface-query component used by LandingAssist and Landed checks; it never moves the spacecraft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Ground Probe", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSSpacecraftGroundProbeComponent> GroundProbeComponent;

	/** Dedicated driving boom, attached to the ship rather than any planet frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> FlightCameraBoom;

	/** Serialized compatibility alias for existing spacecraft Blueprints. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "Use FlightCameraBoom."))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FlightCamera;

	/** Pawn-only overlap volume used for automatic deposits and boarding. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> BoardingTrigger;

	/** Sizes the boarding/workshop trigger from the current physical spacecraft mesh instead of its legacy cube-era radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	bool bAutoSizeBoardingTriggerFromSpacecraftMesh = true;

	/** Extra distance beyond the physical mesh bounds accepted for boarding and Moon workshop use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float BoardingProximityMargin = 80.0f;

	/** Retains the legacy minimum range for compact spacecraft while large meshes grow automatically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float BoardingTriggerMinimumRadius = 300.0f;

	/** Location where a boarded character is attached. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> BoardingPoint;

	/** Location where a character appears after disembarking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> ExitPoint;

	/** Small visual clearance above the spacecraft mesh top for the navigation marker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Navigation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float NavigationMarkerHeightOffset = 20.0f;

	/** Small anti-z-fighting clearance above a real collision surface while this spacecraft is grounded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Surface", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ShipGroundClearance = 2.0f;

	/** Fits the flight proxy to the Blueprint-selected hull, including its offset and scale. */
	UPROPERTY(EditDefaultsOnly, Category = "Ship|Collision", meta = (AllowPrivateAccess = "true"))
	bool bAutoSizeFlightCollisionFromSpacecraftMesh = true;

	/** Search extent for a respawn inside the union of nearby legal landing areas. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlayerRespawnSearchRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlayerRespawnCapsuleRadius = 42.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float PlayerRespawnCapsuleHalfHeight = 96.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float PlayerRespawnClearance = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0"))
	float NormalFlightFOV = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "30.0", ClampMax = "170.0"))
	float BoostFlightFOV = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float FlightFOVInterpolationSpeed = 5.0f;

	/** Exterior driving camera default. This is deliberately independent of the character's camera range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "5000.0", UIMin = "800.0", UIMax = "3200.0"))
	float FlightCameraDefaultArmLength = 1800.0f;

	/** Keeps the craft below the sight line instead of locking it in the centre of the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true"))
	FVector FlightCameraSocketOffset = FVector(0.0f, 0.0f, 300.0f);

	/** Dedicated exterior-camera pitch limits; player-character limits are never reused while driving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-89.0", UIMax = "0.0"))
	float FlightCameraPitchMin = -70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
	float FlightCameraPitchMax = 55.0f;

	/** Multiplies raw mouse look for the exterior camera only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.001", ClampMax = "10.0", UIMin = "0.001", UIMax = "1.0"))
	float FlightCameraLookSensitivity = 0.18f;

	/** Closest driving camera distance selectable with the mouse wheel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "3000.0", UIMin = "0.0", UIMax = "1600.0"))
	float FlightCameraMinArmLength = 1000.0f;

	/** Furthest driving camera distance selectable with the mouse wheel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "5000.0", UIMin = "900.0", UIMax = "3200.0"))
	float FlightCameraMaxArmLength = 3600.0f;

	/** Camera-boom change, in centimeters, for one mouse-wheel step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Camera|Zoom", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", UIMax = "500.0"))
	float FlightCameraZoomStep = 180.0f;

	/** Server-side cap for one raw mouse sample. Continuous axes remain clamped to [-1, 1]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Flight|Input", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0", UIMax = "64.0"))
	float MaxFlightLookInputPerSample = 20.0f;

	/** Unbounded resource storage used by both Earth and Moon collection. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TMap<EJTSResourceType, int32> Storage;

	UPROPERTY(ReplicatedUsing = OnRep_Storage, VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TArray<FJTSResourceAmount> ReplicatedStorage;

	UPROPERTY(ReplicatedUsing = OnRep_Occupants, VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TArray<FJTSSpacecraftOccupantState> Occupants;

	UPROPERTY(ReplicatedUsing = OnRep_Occupants, VisibleAnywhere, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlayerState> DriverPlayerState;

	/** Character currently attached to this spacecraft, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> BoardedPlayer;

	/** Character currently inside the boarding trigger, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AJTSCharacter> NearbyPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FlightInputMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightForwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightVerticalAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightRollAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightLookYawAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightLookPitchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightCameraZoomAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightBoostAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightBrakeAction;

	/** Default landing-request binding; projects can replace the presentation/input layer in Blueprint. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightLandingAction;

	/** Available only while a player is driving a grounded SpaceWorld spacecraft. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlightDisembarkAction;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredFlightInputSubsystem;
	TWeakObjectPtr<UInputComponent> BoundFlightInputComponent;

	/** Transient real-planet parking state. Earth spacecraft leave this unset. */
	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Surface", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> GroundedPlanet;

	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Surface", meta = (AllowPrivateAccess = "true"))
	bool bIsGroundedOnPlanet = false;

	/** Planet that supplies free-flight gravity. It stays valid while the craft is Flying. */
	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Flight", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> FlightPlanet;

	/** Site whose accepted rule set produced the current landed state. Null for legacy surface snaps. */
	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Landing", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetLandingSite> ActiveLandingSite;

	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Landing", meta = (AllowPrivateAccess = "true"))
	EJTSSpacecraftFlightState FlightState = EJTSSpacecraftFlightState::Flying;

	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Landing", meta = (AllowPrivateAccess = "true"))
	EJTSLandingValidationFailure LastLandingFailure = EJTSLandingValidationFailure::None;

	UPROPERTY(ReplicatedUsing = OnRep_FlightState, VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ship|Landing", meta = (AllowPrivateAccess = "true"))
	EJTSSpacecraftLandingAssistPhase LandingAssistPhase = EJTSSpacecraftLandingAssistPhase::None;

	TWeakObjectPtr<AJTSPlanetLandingSite> PendingLandingSite;
	FTransform PendingLandingTransform = FTransform::Identity;
	float PendingLandingClearance = 0.0f;
	float CurrentFlightCameraArmLength = 0.0f;
	bool bDisembarkInputArmed = false;
	bool bDisembarkRequestPending = false;

	bool bPersistedStorageRestoreAttempted = false;
	bool bFlightCameraDistanceInitialized = false;
	FJTSSpacecraftInputState LocalFlightInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ship|Boarding", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "4"))
	int32 MaximumOccupants = 4;
};
