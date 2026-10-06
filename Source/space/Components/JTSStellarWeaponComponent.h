#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarWeaponComponent.generated.h"

class UJTSStellarLoadoutComponent;
class AJTSStellarEffectActor;
class AJTSBlackHoleField;

/** Other players need only the equipped combination, not the owner's private inventory. */
USTRUCT()
struct FJTSStellarEquippedVisual
{
	GENERATED_BODY()
	UPROPERTY() FName CoreId;
	UPROPERTY() FName AttachmentId;
};

/** Server-authoritative channels and casts; clients submit intent and a bounded view direction only. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarWeaponComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarWeaponComponent();
	UFUNCTION(BlueprintPure) bool HasActiveWeapon() const;
	const FJTSStellarWeaponDefinition* GetEquippedWeaponDefinition() const;
	FLinearColor GetEquippedCoreColor() const;
	UFUNCTION(BlueprintPure) bool IsAiming() const;
	UFUNCTION(BlueprintPure) bool IsCasting() const;
	UJTSStellarLoadoutComponent* GetLoadout() const;
	UFUNCTION(BlueprintCallable, Category="Stellar|Input") void SetPrimary(bool bHeld);
	UFUNCTION(BlueprintCallable, Category="Stellar|Input") void SetSecondary(bool bHeld);
	void CycleWeapon();
	void ReturnToNormalWeapon();
	void StopChannels();
	void RefreshEquipmentBinding();
	/** Server-side diagnostic count; field Actors replicate independently of private equipment state. */
	int32 GetActiveBlackHoleCount() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar") TSoftObjectPtr<UJTSStellarWeaponCatalog> WeaponCatalog;
	UFUNCTION(Server, Reliable) void ServerSetInput(bool bPrimary, bool bSecondary, FGuid CoreId, FGuid AttachmentId,
		FVector_NetQuantizeNormal Direction, FVector_NetQuantize CastViewOrigin);
	UFUNCTION(Server, Unreliable) void ServerUpdateAim(FVector_NetQuantizeNormal Direction);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	bool CanUseWeapon() const;
	/** Compute each cycle from current server state, including consecutive client requests. */
	UFUNCTION(Server, Reliable) void ServerCycleWeapon();
	UFUNCTION(Server, Reliable) void ServerReturnToNormalWeapon();
	const FJTSStellarWeaponDefinition* ResolveDefinition(FJTSStellarWeaponBinding& Binding) const;
	void SubmitLocalInput();
	void LocalHeartbeat();
	void ServerPulse();
	void StopServerChannels();
	void JetPulse(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, float Delta);
	void FocusShot(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding);
	bool TryCastBlackHole(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector CastViewOrigin);
	void RepulsionPulse(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding);
	void GetLocalViewRay(FVector& Origin, FVector& Direction) const;
	void RemoveForces();
	UFUNCTION() void HandleLoadoutChanged();
	UFUNCTION() void OnRep_Equipped();
	UFUNCTION(NetMulticast, Unreliable) void MulticastEffect(FName Core, FName Attachment,
		FVector_NetQuantize Start, FVector_NetQuantize End, float Radius, bool bPrimary, bool bSecondary,
		bool bFromMuzzle = true, bool bImpact = false);
	UFUNCTION(NetMulticast, Reliable) void MulticastStopEffect();
	UFUNCTION(NetMulticast, Reliable) void MulticastCastGesture();
	UPROPERTY(Transient) TObjectPtr<UJTSStellarWeaponCatalog> Catalog;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalEffect;
	UPROPERTY(ReplicatedUsing=OnRep_Equipped) FJTSStellarEquippedVisual EquippedVisual;
	UPROPERTY(Replicated) bool bReplicatedCasting = false;
	double CastGestureUntil = 0;
	TWeakObjectPtr<UJTSStellarLoadoutComponent> BoundLoadout;
	TArray<TWeakObjectPtr<AActor>> ControlledTargets;
	TMap<FGuid, double> NextRepulsion;
	TMap<FGuid, double> NextBlackHoleCast;
	TArray<TWeakObjectPtr<AJTSBlackHoleField>> BlackHoleFields;
	FJTSStellarWeaponBinding ChannelBinding;
	FVector AimDirection = FVector::ForwardVector;
	bool bLocalPrimary = false;
	bool bLocalSecondary = false;
	bool bServerPrimary = false;
	bool bServerSecondary = false;
	double LastHeartbeat = 0;
	double LastPulseTime = 0;
	double NextFocusShot = 0;
	float AreaAccumulator = 0;
	FTimerHandle PulseTimer;
	FTimerHandle HeartbeatTimer;
};
