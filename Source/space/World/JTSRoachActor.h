#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "TimerManager.h"

#include "JTSRoachActor.generated.h"

class AJTSRoachNestActor;
class APawn;
class AController;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UJTSHealthComponent;
class UJTSMoonWrappedActorComponent;

/** Lightweight Moon Ant behavior states. The legacy native class name is retained for asset compatibility. */
UENUM(BlueprintType)
enum class EJTSAntState : uint8
{
	Emerging UMETA(DisplayName = "Emerging"),
	Roaming UMETA(DisplayName = "Roaming"),
	ReactingToHit UMETA(DisplayName = "Reacting To Hit"),
	Fleeing UMETA(DisplayName = "Fleeing"),
	Burrowing UMETA(DisplayName = "Burrowing")
};

/**
 * Runtime Moon Ant actor. One instance represents exactly one surface activity cycle and is always
 * destroyed after burrowing. The legacy class name remains only for existing Blueprint parent references.
 */
UCLASS()
class SPACE_API AJTSRoachActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSRoachActor();

	void InitializeAnt(AJTSRoachNestActor* InOriginNest, const FVector& InGroundLocation);

	UFUNCTION(BlueprintPure, Category = "Moon|Ant")
	EJTSAntState GetAntState() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	UJTSHealthComponent* GetHealthComponent() const;

	virtual bool CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	const class AJTSMoonGameMode* GetMoonGameMode() const;
	void RefreshVisualMode();
	void ConfigureAntVisuals();
	void CaptureBaseVisualTransforms();
	void RecalculateGroundMetrics();
	void ChooseRoamTarget(bool bForceNearNest = false);
	bool GetOriginNestLocation(FVector& OutNestLocation) const;
	float GetDistanceToOriginNest() const;
	FVector GetShortestWrappedDeltaTo(const FVector2D& TargetLogicalPosition) const;
	bool MoveAlongGround(const FVector& Direction, float Speed, float DeltaSeconds);
	void RotateTowardsDirection(const FVector& Direction, float DeltaSeconds);
	void PlaceOnGround(const FVector& NewGroundLocation);
	UPrimitiveComponent* GetActiveVisualComponent() const;
	void SetMeleeHitCollisionEnabled(bool bEnabled);
	void UpdateAntVisualTransform();
	void ConfigureAntHealthBar();
	void UpdateAntHealthBarTransform();
	void ShowAntHealthBar();
	void HideAntHealthBar();
	void SetVisualBurrowOffset(float RelativeZ);
	void BeginFleeing();
	void BeginBurrowing();
	void SetAntState(EJTSAntState NewState);
	void UpdateFallbackMaterial();
	void UpdateMoonWrappedLogicalPosition();

	UFUNCTION()
	void HandleHealthDamaged(float CurrentHealth, float MaxHealth, float Damage, AActor* DamageCauser);

	UFUNCTION()
	void HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser);

	UPROPERTY(VisibleAnywhere, Category = "Moon|Ant")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSHealthComponent> HealthComponent;

	/** Assign Skeletal Mesh, Antwalk animation, and materials on this component in BP_MoonAnt. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> AntMesh;

	/** Temporary primitive shown only when no Skeletal Mesh has been assigned to AntMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> AntFallbackMesh;

	/** Invisible query collider is the only gameplay collision for both Skeletal and fallback visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> AntHitCollider;

	/** Native screen-space health bar that follows the visual Moon-bend transform without inheriting Ant scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Ant|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> AntHealthBarComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSMoonWrappedActorComponent> MoonWrappedActorComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntTargetBodyLength = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntScaleVariationMin = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntScaleVariationMax = 1.15f;

	/** Imported Ant mesh forward-axis correction. This changes only the visual mesh, never Actor movement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Visual", meta = (AllowPrivateAccess = "true"))
	float AntMeshForwardYawOffset = 0.0f;

	/** Emits one visual-transform diagnostic per Ant per second while enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moon|Ant|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugAntVisualTransform = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float AntMaxHealth = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|Ant|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float AntHealthBarVisibleDuration = 2.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Ant", meta = (AllowPrivateAccess = "true"))
	EJTSAntState AntState = EJTSAntState::Emerging;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AntFallbackMaterial;

	TWeakObjectPtr<AJTSRoachNestActor> OriginNest;
	FVector GroundLocation = FVector::ZeroVector;
	FVector2D RoamTargetLogicalPosition = FVector2D::ZeroVector;
	FVector FleeSourceLocation = FVector::ZeroVector;
	FVector FleeDirection = FVector::ForwardVector;
	FVector AntMeshBaseRelativeLocation = FVector::ZeroVector;
	FRotator AntMeshBaseRelativeRotation = FRotator::ZeroRotator;
	FVector AntFallbackBaseRelativeLocation = FVector::ZeroVector;
	FVector AntFallbackBaseMeshScale = FVector(0.55f, 0.34f, 0.16f);
	FVector CurrentMoonBendWorldOffset = FVector::ZeroVector;
	float AntMeshUniformScale = 1.0f;
	float GroundSupportHeight = 12.0f;
	float BurrowDepth = 30.0f;
	float BurrowVisualOffset = 0.0f;
	float StateElapsed = 0.0f;
	float SurfaceElapsed = 0.0f;
	float SurfaceDuration = 0.0f;
	float RoamRetargetElapsed = 0.0f;
	float RoamRetargetInterval = 0.0f;
	float FleeDuration = 0.0f;
	float FleeDistanceTravelled = 0.0f;
	float NextAntVisualDebugLogTime = 0.0f;
	FTimerHandle AntHealthBarHideTimerHandle;
	bool bInitialized = false;
	bool bUsingSkeletalAntMesh = false;
	bool bAntMeshBaseTransformCaptured = false;
	bool bAntVisualTransformReady = false;
};
