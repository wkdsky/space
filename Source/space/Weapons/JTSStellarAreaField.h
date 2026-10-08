#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarAreaField.generated.h"
/** Short-lived paid residual fire; no extra player tick and at most two per caster. */
UCLASS()
class SPACE_API AJTSStellarAreaField : public AActor
{
	GENERATED_BODY()
public:
	AJTSStellarAreaField();
	void Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Up, float Radius, float Duration);
	FGuid GetCoreId() const { return CoreId; }
	FGuid GetAttachmentId() const { return AttachmentId; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Pulse();
	UFUNCTION() void OnRep_Presentation();
	UPROPERTY(ReplicatedUsing=OnRep_Presentation) FJTSStellarWeaponDefinition Definition;
	UPROPERTY(ReplicatedUsing=OnRep_Presentation) FVector SurfaceNormal = FVector::UpVector;
	UPROPERTY(ReplicatedUsing=OnRep_Presentation) float FieldRadius = 300;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalEffect;
	FGuid CoreId, AttachmentId;
	float Duration = 1.5f;
	double LastPulse = 0;
	FTimerHandle Timer;
};
