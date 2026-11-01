#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "JTSPlanetSurfaceAttachment.generated.h"
class AJTSPlanetAnchor;
class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UBoxComponent;
class UJTSHealthComponent;

/** Independent presentation data: no changes to the source mesh or the planet asset. */
USTRUCT()
struct FJTSPlanetAttachmentAppearance
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CutVolume;
	UPROPERTY() TObjectPtr<AJTSPlanetAnchor> Planet;
	UPROPERTY() FTransform Transform;
	UPROPERTY() bool bConform = false;
	UPROPERTY() bool bCastShadow = false;
	UPROPERTY() bool bDestructible = false;
	UPROPERTY() float SurfaceOffset = 1;
};

UCLASS()
class SPACE_API AJTSPlanetSurfaceAttachment : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()
public:
	AJTSPlanetSurfaceAttachment();
	void Initialize(UStaticMesh* Mesh, AJTSPlanetAnchor* Planet, const FTransform& Transform,
		bool bConform, bool bCastShadow, float SurfaceOffset, bool bDestructible = false, UStaticMesh* CutVolume = nullptr);
	UFUNCTION(BlueprintPure, Category="Planet|Attachment") bool IsRefugeUsable() const;
	virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* Pawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SolidMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> SurfacePatch;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> DamageTarget;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UJTSHealthComponent> Health;
	UPROPERTY(ReplicatedUsing=ApplyAppearance) FJTSPlanetAttachmentAppearance Appearance;
	bool bApplyingAppearance = false;
	void OnAttachmentTransformUpdated(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport);
	void UpdateCutVisibility();
	UFUNCTION() void ApplyAppearance();
	UFUNCTION() void OnAttachmentDepleted(AController* InstigatorController, AActor* Causer);
};
