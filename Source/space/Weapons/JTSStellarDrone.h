#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarDrone.generated.h"
class UJTSHealthComponent;
class USphereComponent;
/** A bounded server-driven hovering defender. It never blocks a player's movement. */
UCLASS()
class SPACE_API AJTSStellarDrone : public AActor
{
	GENERATED_BODY()
public:
	AJTSStellarDrone();
	void Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding);
	void Detonate();
	void LimitRemainingLife(float Seconds);
	FGuid GetCoreId() const { return CoreId; }
	FGuid GetAttachmentId() const { return AttachmentId; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void Think();
	UFUNCTION() void OnRep_Visual();
	UFUNCTION() void HandleDeath(AController* Controller, AActor* Causer);
	UFUNCTION(NetMulticast, Unreliable) void MulticastShot(FVector_NetQuantize End);
	UFUNCTION(NetMulticast, Reliable) void MulticastBurst();
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UJTSHealthComponent> Health;
	UPROPERTY(ReplicatedUsing=OnRep_Visual) FJTSStellarWeaponDefinition Definition;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalEffect;
	FGuid CoreId, AttachmentId;
	float Damage = 0, BlastDamage = 0, BlastRadius = 200, Duration = 20;
	double NextShot = 0;
	FTimerHandle Timer;
};
