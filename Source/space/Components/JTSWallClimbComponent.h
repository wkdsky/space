#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSWallClimbComponent.generated.h"

class UPrimitiveComponent;

enum class EJTSClimbMotion : uint8
{
	Step,
	Leap,
	Descend
};

/** One shared timing curve for the authoritative capsule and its local body pose. */
struct SPACE_API FJTSClimbMotionSample
{
	float Travel = 0.0f;
	float Load = 0.0f;
	float Reach = 0.0f;
	float Catch = 0.0f;
	float Settle = 0.0f;

	static FJTSClimbMotionSample Evaluate(float Phase, EJTSClimbMotion Motion);
};

/** Free-hand climbing on real collision surfaces, relative to the current gravity direction. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSWallClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSWallClimbComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsClimbing() const { return bClimbing; }
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsLeaping() const { return bLeaping; }
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsMantling() const { return bClimbing && bMantling; }
	UFUNCTION(BlueprintPure, Category = "Climb")
	float GetStepAlpha() const;
	UFUNCTION(BlueprintPure, Category = "Climb")
	bool IsLeadHandLeft() const { return bLeadHandLeft; }
	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetSurfaceNormal() const { return SurfaceNormal; }
	UFUNCTION(BlueprintPure, Category = "Climb")
	FVector GetStepDirection() const { return StepDirection; }
	float GetStepStartWorldTime() const { return StepStartWorldTime; }
	FVector GetStepStart() const { return StepStart; }
	FVector GetStepTarget() const { return StepTarget; }
	/** Cosmetic hand/foot contact query on the same surface used for gameplay. */
	bool FindPoseContact(const FVector& DesiredWorld, FVector& OutPoint, FVector& OutNormal) const;

	/** Owner-only intent; the server validates geometry, stamina, and collision. */
	void ToggleAttach();
	/** Explicit W+Space grab at the foot of a climbable surface. */
	bool TryAutoAttach();
	/** Arm a server-validated grip for this jump's approach or impact. */
	void ArmJumpGrab();
	/** Called by CharacterMovement when an airborne capsule hits an unwalkable surface. */
	void TryJumpImpactGrip(const FHitResult& Impact);
	/** Server collision callback for jump landings on slopes UE would otherwise walk on. */
	void TryJumpLandingGrip(const FHitResult& LandingHit);
	void SubmitClimbIntent(const FVector& WishDirection);
	void SubmitClimbLeap(const FVector& WishDirection);
	/** Release the wall immediately and resume planet gravity. */
	void SubmitClimbDrop();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FVector GetGravityUp() const;
	bool IsValidSurfaceNormal(const FVector& Normal) const;
	bool TraceClimbSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool ProbeClimbSurface(FHitResult& OutHit) const;
	bool CanClimbNow() const;
	void BeginClimb(const FHitResult& Hit);
	void EndClimb(bool bWalkOff = false, const TCHAR* Reason = TEXT("Manual"));
	void StartStep(const FVector& WishDirection, bool bLeap);
	void AdvanceStep(float DeltaTime);
	void AdvanceMantle(float DeltaTime);
	void FaceSurface();
	void LockBodyFacingToSurface();
	void RestoreBodyFacingSettings();
	bool TryTopOut(const FVector& WishDirection);
	bool TryBottomOut();

	UFUNCTION(Server, Reliable)
	void ServerToggleAttach();
	UFUNCTION(Server, Unreliable)
	void ServerTryAutoAttach();
	UFUNCTION(Server, Reliable)
	void ServerArmJumpGrab();
	UFUNCTION(Server, Unreliable)
	void ServerSubmitClimbIntent(FVector_NetQuantizeNormal WishDirection);
	UFUNCTION(Server, Reliable)
	void ServerSubmitClimbLeap(FVector_NetQuantizeNormal WishDirection);
	UFUNCTION(Server, Reliable)
	void ServerSubmitClimbDrop();
	UFUNCTION()
	void OnRep_Climbing();

	UPROPERTY(ReplicatedUsing = OnRep_Climbing)
	bool bClimbing = false;
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal SurfaceNormal = FVector::ForwardVector;
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal StepDirection = FVector::UpVector;
	UPROPERTY(Replicated)
	bool bLeadHandLeft = true;
	UPROPERTY(Replicated)
	bool bLeaping = false;
	UPROPERTY(Replicated)
	bool bMantling = false;
	UPROPERTY(Replicated)
	float StepStartWorldTime = -1.0f;
	UPROPERTY(Replicated)
	float ActiveStepDuration = 0.0f;

	float StepElapsed = 0.0f;
	bool bStepActive = false;
	bool bHasPendingLeap = false;
	FVector PendingLeapDirection = FVector::ZeroVector;
	UPROPERTY(Replicated)
	FVector StepStart = FVector::ZeroVector;
	UPROPERTY(Replicated)
	FVector StepTarget = FVector::ZeroVector;
	FVector StepTargetNormal = FVector::ForwardVector;
	FVector MantleStart = FVector::ZeroVector;
	FVector MantleLift = FVector::ZeroVector;
	FVector MantleLanding = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> StepTargetSurfaceComponent;
	FVector GripLocation = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> GripSurfaceComponent;
	float MissingSurfaceSeconds = 0.0f;
	float NextAttachRequestSeconds = -1.0f;
	bool bJumpGrabArmed = false;
	float SavedGravityScale = 1.0f;
	bool bSavedGravityScale = false;
	bool bSavedFacingSettings = false;
	bool bSavedControllerPitch = false;
	bool bSavedControllerYaw = false;
	bool bSavedControllerRoll = false;
	bool bSavedOrientToMovement = false;
	bool bSavedControllerDesiredRotation = false;

	UPROPERTY(EditDefaultsOnly, Category = "Climb|Surface", meta = (ClampMin = "0", ClampMax = "89"))
	float MinimumSlopeDegrees = 40.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Surface", meta = (ClampMin = "90", ClampMax = "179"))
	float MaximumSlopeDegrees = 95.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Surface", meta = (ClampMin = "20"))
	float ProbeDistance = 140.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Surface", meta = (ClampMin = "0.1"))
	float ContactGraceSeconds = 0.65f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Surface", meta = (ClampMin = "0"))
	float JumpGrabSurfaceGapCm = 28.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "20"))
	float StepDistance = 70.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "0.1"))
	float StepSeconds = 0.40f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "20"))
	float LeapDistance = 150.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "0.1"))
	float LeapSeconds = 0.50f;
	/** S lowers through longer, quicker holds while remaining attached. */
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "20"))
	float DescendDistance = 100.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "0.1"))
	float DescendSeconds = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Movement", meta = (ClampMin = "0.1"))
	float MantleSeconds = 0.72f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Stamina", meta = (ClampMin = "0"))
	float ClingDrainPerSecond = 4.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Stamina", meta = (ClampMin = "0"))
	float StepCost = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Climb|Stamina", meta = (ClampMin = "0"))
	float LeapCost = 9.0f;
};
