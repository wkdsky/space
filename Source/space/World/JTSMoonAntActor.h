#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "TimerManager.h"

#include "JTSMoonAntActor.generated.h"

class AJTSMoonAntNestActor;
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
class UJTSExperienceRewardComponent;
class IJTSMoonSurfaceGameplaySettings;
class AJTSPlanetAnchor;

/** Lightweight MoonAnt behavior states. The legacy native class name is retained for asset compatibility. */
UENUM(BlueprintType)
enum class EJTSMoonAntState : uint8
{
	Emerging UMETA(DisplayName = "Emerging"),
	Roaming UMETA(DisplayName = "Roaming"),
	ReactingToHit UMETA(DisplayName = "Reacting To Hit"),
	Fleeing UMETA(DisplayName = "Fleeing"),
	Burrowing UMETA(DisplayName = "Burrowing")
};

/**
 * Runtime MoonAnt actor. One instance represents exactly one surface activity cycle and is always
 * destroyed after burrowing. The legacy class name remains only for existing Blueprint parent references.
 */
UCLASS()
class SPACE_API AJTSMoonAntActor : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	AJTSMoonAntActor();

	void InitializeMoonAnt(AJTSMoonAntNestActor* InOriginNest, const FVector& InGroundLocation);

	UFUNCTION(BlueprintPure, Category = "Moon|MoonAnt")
	EJTSMoonAntState GetMoonAntState() const;

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_MoonAntState();

	const IJTSMoonSurfaceGameplaySettings* GetMoonGameMode() const;
	void RefreshVisualMode();
	void ConfigureMoonAntVisuals();
	void CaptureBaseVisualTransforms();
	void RecalculateGroundMetrics();
	void ChooseRoamTarget(bool bForceNearNest = false);
	bool GetOriginNestLocation(FVector& OutNestLocation) const;
	float GetDistanceToOriginNest() const;
	AJTSPlanetAnchor* GetSurfacePlanet() const;
	bool IsUsingRealPlanetSurface() const;
	FVector GetSurfaceTangentTo(const FVector& TargetLocation) const;
	bool MoveAlongGround(const FVector& Direction, float Speed, float DeltaSeconds);
	void RotateTowardsDirection(const FVector& Direction, float DeltaSeconds);
	void PlaceOnGround(const FVector& NewGroundLocation);
	UPrimitiveComponent* GetActiveVisualComponent() const;
	void SetMeleeHitCollisionEnabled(bool bEnabled);
	void UpdateMoonAntVisualTransform();
	void ConfigureMoonAntHealthBar();
	void UpdateMoonAntHealthBarTransform();
	void ShowMoonAntHealthBar();
	void HideMoonAntHealthBar();
	void SetVisualBurrowOffset(float RelativeZ);
	void BeginFleeing();
	void BeginBurrowing();
	/** Completes the full deferred corpse spawn before the MoonAnt is allowed to destroy itself. */
	bool SpawnMoonAntCorpse();
	void SetMoonAntState(EJTSMoonAntState NewState);
	void UpdateFallbackMaterial();

	UFUNCTION()
	void HandleHealthDamaged(float CurrentHealth, float MaxHealth, float Damage, AActor* DamageCauser);

	UFUNCTION()
	void HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser);

	UPROPERTY(VisibleAnywhere, Category = "Moon|MoonAnt")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSHealthComponent> HealthComponent;

	/** Standard enemy kill experience. Future bosses can use the same component with bBossReward enabled. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Progression", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UJTSExperienceRewardComponent> ExperienceRewardComponent;

	/** Assign Skeletal Mesh, MoonAnt animation, and materials on this component in BP_MoonAnt. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> MoonAntMesh;

	/** Temporary primitive shown only when no Skeletal Mesh has been assigned to MoonAntMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MoonAntFallbackMesh;

	/** Invisible query collider is the only gameplay collision for both Skeletal and fallback visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> MoonAntHitCollider;

	/** Native screen-space health bar that follows the MoonAnt visual without inheriting its scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon|MoonAnt|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> MoonAntHealthBarComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntTargetBodyLength = 28.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntScaleVariationMin = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntScaleVariationMax = 1.15f;

	/** Imported MoonAnt mesh forward-axis correction. This changes only the visual mesh, never Actor movement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Visual", meta = (AllowPrivateAccess = "true"))
	float MoonAntMeshForwardYawOffset = 0.0f;

	/** Emits one visual-transform diagnostic per MoonAnt per second while enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Moon|MoonAnt|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugMoonAntVisualTransform = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MoonAntMaxHealth = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moon|MoonAnt|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float MoonAntHealthBarVisibleDuration = 2.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_MoonAntState, Category = "Moon|MoonAnt", meta = (AllowPrivateAccess = "true"))
	EJTSMoonAntState MoonAntState = EJTSMoonAntState::Emerging;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonAntFallbackMaterial;

	TWeakObjectPtr<AJTSMoonAntNestActor> OriginNest;
	FVector GroundLocation = FVector::ZeroVector;
	FVector RoamTargetWorldLocation = FVector::ZeroVector;
	FVector FleeSourceLocation = FVector::ZeroVector;
	FVector FleeDirection = FVector::ForwardVector;
	FVector MoonAntMeshBaseRelativeLocation = FVector::ZeroVector;
	FRotator MoonAntMeshBaseRelativeRotation = FRotator::ZeroRotator;
	FVector MoonAntFallbackBaseRelativeLocation = FVector::ZeroVector;
	FVector MoonAntFallbackBaseMeshScale = FVector(0.55f, 0.34f, 0.16f);
	float MoonAntMeshUniformScale = 1.0f;
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
	float NextMoonAntVisualDebugLogTime = 0.0f;
	FTimerHandle MoonAntHealthBarHideTimerHandle;
	UPROPERTY(Replicated)
	TObjectPtr<AJTSPlanetAnchor> SurfacePlanet;

	UPROPERTY(Replicated)
	FVector SurfaceUp = FVector::UpVector;
	bool bInitialized = false;
	bool bUsingSkeletalMoonAntMesh = false;
	bool bMoonAntMeshBaseTransformCaptured = false;
	bool bMoonAntVisualTransformReady = false;
	bool bDeathSequenceStarted = false;
	bool bHasDroppedCorpse = false;
	UPROPERTY(Replicated)
	bool bUsesRealPlanetSurface = false;
};
