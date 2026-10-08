#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarProjectile.generated.h"
/** Server swept explosive orb; its small cosmetic child follows replicated motion. */
UCLASS()
class SPACE_API AJTSStellarProjectile : public AActor
{
	GENERATED_BODY()
public:
	AJTSStellarProjectile();
	void Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Destination, bool bCharged);
	FGuid GetCoreId() const { return CastBinding.CoreInstanceId; }
	FGuid GetAttachmentId() const { return CastBinding.AttachmentInstanceId; }
	void LimitRemainingLife(float Seconds);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Advance();
	void Detonate(FVector Point, FVector Normal);
	UFUNCTION() void OnRep_Visual();
	UFUNCTION(NetMulticast, Reliable) void MulticastDetonate(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, float Radius);
	UPROPERTY(ReplicatedUsing=OnRep_Visual) FJTSStellarWeaponDefinition Definition;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalEffect;
	FJTSStellarWeaponBinding CastBinding;
	FVector Destination, Velocity;
	bool bCharged = false, bDetonated = false;
	double LastStep = 0;
	FTimerHandle Timer;
};
