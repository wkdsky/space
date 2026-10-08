#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "space/Items/JTSStellarLoadoutTypes.h"
#include "JTSStellarAbilityComponent.generated.h"
class AJTSStellarDrone;
class AJTSStellarAreaField;
class AJTSStellarProjectile;
/** Executes the nine non-legacy abilities after the weapon component validates owner intent. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarAbilityComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarAbilityComponent();
	void InputChanged(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, FVector Direction);
	bool ExecutePulse(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, FVector Direction, float Delta);
	void CancelCast();
	void StopPresentation();
	void ValidatePersistentEffects();
	void TrackArea(AJTSStellarAreaField* Area);
	void RefundShieldAbsorption(FGuid CastId, float Absorbed);
	UFUNCTION(BlueprintPure) bool IsEngineeringMode() const { return bEngineering; }
	UFUNCTION(BlueprintPure) float GetLockProgress() const { return LockProgress; }
	UFUNCTION(BlueprintPure) int32 GetRobotCount() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	bool Pay(float Cost);
	void LaunchExplosion(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Charged);
	void Slash(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Circular);
	void GrantTeamShield(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding);
	void Heal(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, bool Self, float Delta);
	void AreaChannel(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, bool Primary, bool Secondary, float Delta);
	void RayAttack(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction);
	void MakeRobot(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction);
	bool Disassemble(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Direction, float Delta);
	void ForEachAlly(FVector Center, float Radius, TFunctionRef<void(APawn*)> Apply);
	UFUNCTION(NetMulticast, Unreliable) void MulticastArea(FName Core, FName Attachment, FVector_NetQuantize Center, FVector_NetQuantizeNormal Up, float Radius, bool Shield, bool Burst, float SweepAngle = 360);
	UFUNCTION(NetMulticast, Unreliable) void MulticastLink(FName Core, FName Attachment, FVector_NetQuantize Start, FVector_NetQuantize End, float Width, bool FromCore, bool Impact);
	UPROPERTY(Replicated) bool bEngineering = false;
	UPROPERTY(Replicated) float LockProgress = 0;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalChannelArea;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalChannelLink;
	TMap<FGuid, float> ShieldRefunds;
	TMap<EJTSStellarWeaponMode, double> NextPrimary, NextSecondary;
	TArray<TWeakObjectPtr<AJTSStellarDrone>> Robots;
	TArray<TWeakObjectPtr<AJTSStellarAreaField>> Areas;
	TArray<TWeakObjectPtr<AJTSStellarProjectile>> Projectiles;
	TWeakObjectPtr<AActor> LockTarget;
	FGuid ChargingCore, ChargingAttachment;
	double ChargeStart = -1, NextParry = 0, RefundWindow = 0;
	float LockSeconds = 0, AreaSeconds = 0, RefundInWindow = 0;
	bool bHeldSecondary = false;
	FTimerHandle MaintenanceTimer;
};
