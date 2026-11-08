#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "JTSMirrorDefenseComponent.generated.h"

class APawn;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture;
class AJTSPlanetAnchor;

USTRUCT(BlueprintType)
struct FJTSMirrorLegChain
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Upper;
	UPROPERTY(EditAnywhere) FName Lower;
	UPROPERTY(EditAnywhere) FName Foot;
};

/** Optional, non-damaging nest defence. The server owns activation and the mirror aim. */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSMirrorDefenseComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSMirrorDefenseComponent();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mirror") bool bEnabled = false;
	UPROPERTY(EditDefaultsOnly, Category="Mirror", meta=(ClampMin="100")) float GuardRadius = 240;
	UPROPERTY(EditDefaultsOnly, Category="Mirror", meta=(ClampMin="60")) float GuardSpacing = 130;
	UPROPERTY(EditDefaultsOnly, Category="Mirror", meta=(ClampMin="100")) float GlareRange = 2200;
	UPROPERTY(EditDefaultsOnly, Category="Mirror", meta=(ClampMin="1", ClampMax="89")) float GlareHalfAngle = 28;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Timing", meta=(ClampMin="0.1")) float RetractSeconds = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Timing", meta=(ClampMin="0.1")) float SurfacePauseSeconds = 2.5f;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose", meta=(ClampMin="1")) float StrideLength = 40;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose", meta=(ClampMin="0")) float StepHeight = 5;
	UPROPERTY(EditDefaultsOnly, Category="Mirror", meta=(ClampMin="10", ClampMax="89")) float MaximumLeanDegrees = 78;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") FName BodyBone;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") TArray<FJTSMirrorLegChain> Legs;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") float BodyLift = 2;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") float LegLengthScale = 1.22f;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") FVector MirrorOffset = FVector(0,0,16);
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") float MirrorHeightAboveBody = 8.5f;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Pose") FVector ShellHalfExtent = FVector(24,19,4);
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Screen") TObjectPtr<UMaterialInterface> GlareMaterial;
	UPROPERTY(EditDefaultsOnly, Category="Mirror|Reflection") TArray<int32> ReflectiveMaterialSlots;
	UFUNCTION(BlueprintPure, Category="Mirror") bool IsReflecting() const { return bReflecting; }
	UFUNCTION(BlueprintPure, Category="Mirror") FVector GetMirrorLocation() const;
	UFUNCTION(BlueprintPure, Category="Mirror") FVector GetMirrorNormal() const;
	UFUNCTION(BlueprintCallable, Category="Mirror") void AimAt(APawn* Target);
	UFUNCTION(BlueprintCallable, Category="Mirror") void StopReflection();
	void ApplyPose(USkeletalMeshComponent* Mesh);
	void SetNearbyReflection(UTexture* Texture, bool bVisible);
	UFUNCTION(BlueprintPure, Category="Mirror") bool CanGlareAt(class APlayerController* Viewer) const;
	static FQuat CalculateLean(const FVector& LocalDirection, float MaximumDegrees);
	bool FindGuardStation(AJTSPlanetAnchor* Planet, const FVector& Origin, const FVector& Forward, int32 Slot, FVector& OutStation);
	bool FindSeparationGoal(AJTSPlanetAnchor* Planet, FVector& OutGoal) const;
	void ReleaseGuardStation();
	void RejectGuardStation();
	bool HasClearGlarePath(const FVector& Eye, const AActor* ViewerPawn) const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	UPROPERTY(Replicated) bool bReflecting = false;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal AimDirection = FVector::UpVector;
	FQuat PresentationLean = FQuat::Identity;
	float ReflectionBlend = 0;
	TWeakObjectPtr<USkeletalMeshComponent> CreatureMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ReflectiveMaterials;
	FVector PoseLift = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	FVector GaitDirection = FVector::ForwardVector;
	float GaitPhase = 0;
	float GaitBlend = 0;
	TWeakObjectPtr<USkeletalMesh> GaitSkeleton;
	TArray<FVector> RestFeet;
	TArray<FTransform> LastLivingPose;
	double DeathBlendStart = -1;
	FVector GuardStation = FVector::ZeroVector;
	FVector GuardOrigin = FVector::ZeroVector;
	FVector RejectedStation = FVector::ZeroVector;
	bool bHasGuardStation = false;
	bool bHasRejectedStation = false;
};

/** Publishes the optional procedural pose before sockets and rendering consume it. */
UCLASS()
class SPACE_API UJTSReflectorSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
public:
	virtual void FinalizeBoneTransform() override;
};
