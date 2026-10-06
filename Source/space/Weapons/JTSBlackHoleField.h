#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "JTSBlackHoleField.generated.h"

USTRUCT()
struct FJTSBlackHoleVisual
{
	GENERATED_BODY()
	UPROPERTY() TSubclassOf<AJTSStellarEffectActor> EffectClass;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	UPROPERTY() FVector_NetQuantizeNormal SurfaceUp = FVector::UpVector;
	UPROPERTY() float Radius = 0.0f;
	UPROPERTY() float CoreRadius = 65.0f;
};

/** One server-owned, stationary cast. Clients create only its Blueprint-authored cosmetic child. */
UCLASS()
class SPACE_API AJTSBlackHoleField : public AActor
{
	GENERATED_BODY()
public:
	AJTSBlackHoleField();
	/** Called before FinishSpawning; damage and upgrades are snapshotted for this cast. */
	void Initialize(const FJTSStellarWeaponDefinition& Definition, const FJTSStellarWeaponBinding& Binding, FVector SurfaceUp, FVector SurfacePoint);
	bool BelongsTo(const FJTSStellarWeaponBinding& Binding) const;
	FVector GetAttractionCenter() const { return AttractionCenter; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Pulse();
	UFUNCTION() void OnRep_Visual();
	UPROPERTY(ReplicatedUsing=OnRep_Visual) FJTSBlackHoleVisual Visual;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalEffect;
	FGuid CoreInstanceId;
	FGuid AttachmentInstanceId;
	float DamagePerSecond = 0.0f;
	float PullSpeed = 0.0f;
	float Duration = 8.0f;
	FVector AttractionCenter = FVector::ZeroVector;
	int32 MaximumTargets = 200;
	double LastPulseTime = 0.0;
	TArray<TWeakObjectPtr<AActor>> ControlledTargets;
	FTimerHandle PulseTimer;
};
