#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSMiningStrikeComponent.generated.h"

class UJTSItemDefinition;

/** Shared tool trajectory, server contact and hand target. Never depends on a rendered socket. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSMiningStrikeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSMiningStrikeComponent();
	const UJTSItemDefinition* GetToolDefinition() const;
	UFUNCTION(BlueprintPure, Category="Mining") bool IsDirectedToolEquipped() const;
	bool FindWorkContact(AActor* Target, FHitResult& OutHit) const;
	void BeginSwing(AActor* WorkTarget = nullptr);
	void ResolveContact();
	/** The reticle and the server use the same collision query. */
	bool AcquireContact(FHitResult& OutHit) const;
	void CancelSwing();
	bool GetGripPose(FVector& OutGrip, FQuat& OutRotation, FVector& OutTip, float& OutLoad) const;
	FTransform GetModelGripTransform() const;
	UFUNCTION(BlueprintPure, Category="Mining") float GetImpactFeedback() const;
	UFUNCTION(BlueprintPure, Category="Mining") int32 GetConfirmedImpactCount() const { return ConfirmedImpactCount; }
	UFUNCTION(BlueprintPure, Category="Mining") FVector GetLastImpactLocation() const { return LastImpactLocation; }
	UFUNCTION(NetMulticast, Reliable) void MulticastBeginSwing(AActor* Target, FVector_NetQuantize Contact, FVector_NetQuantizeNormal Normal, bool bTargeted);
	UFUNCTION(Client, Unreliable) void ClientConfirmImpact();
	UFUNCTION(NetMulticast, Unreliable) void MulticastToolContact(FVector_NetQuantize Point);
	/** Rig measurements and body clearance, configured on the player Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose") FVector ElbowReferenceOffset = FVector(1,19,26);
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose", meta=(ClampMin="1")) float ForearmReach = 29.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose", meta=(ClampMin="1")) float UpperArmReach = 30.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose", meta=(ClampMin="0")) float TorsoClearance = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose") FVector IdleGripOffset = FVector(34,24,32);
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose", meta=(ClampMin="5",ClampMax="40")) float ReboundDegrees = 24.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Pose") float TorsoLean = .10f;
	/** Small camera cone compensates the third-person shoulder offset at arm's length. */
	UPROPERTY(EditDefaultsOnly, Category="Mining|Aim", meta=(ClampMin="0",ClampMax="15")) float AimAssistDegrees = 8.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Aim", meta=(ClampMin="0",ClampMax="25")) float ContactSweepRadius = 12.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Aim", meta=(ClampMin="0.01",ClampMax="0.2")) float ContactWindowSeconds = .10f;
	/** Small creatures below a shoulder camera still need an intentional forward, close-range strike. */
	UPROPERTY(EditDefaultsOnly, Category="Mining|Aim", meta=(ClampMin="0")) float CloseCombatAssistDistance = 110.f;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Aim", meta=(ClampMin="0",ClampMax="80")) float CloseCombatAssistHalfAngle = 55.f;
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	bool GetContactPose(const FVector& Point, FVector& Elbow, FVector& Grip, FQuat& Rotation, bool bClampReach) const;
	void GetViewRay(FVector& Origin, FVector& Direction) const;
	bool IsReachable(const FVector& Contact) const;
	FHitResult PendingContact;
	TWeakObjectPtr<AActor> PresentationTarget;
	FVector TargetLocalContact = FVector::ZeroVector;
	FVector GetPresentationContact() const;
	void KeepToolOutsideBody(FVector& Grip, const FQuat& Rotation) const;
	TWeakObjectPtr<AActor> ExplicitWorkTarget;
	double SwingStart = -100;
	double LastContactSample = -100;
	double ReturnStart = -100;
	double LastImpact = -100;
	FVector ContactPoint = FVector::ZeroVector;
	FVector ContactNormal = FVector::UpVector;
	FVector SwingForward = FVector::ForwardVector;
	FVector SwingUp = FVector::UpVector;
	bool bSwingActive = false;
	bool bContactResolved = true;
	bool bHasContact = false;
	bool bFirstStroke = true;
	UPROPERTY(VisibleInstanceOnly, Category="Mining") int32 ConfirmedImpactCount = 0;
	UPROPERTY(VisibleInstanceOnly, Category="Mining") FVector LastImpactLocation = FVector::ZeroVector;
};
