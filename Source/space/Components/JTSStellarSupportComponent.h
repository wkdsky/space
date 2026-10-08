#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSStellarSupportComponent.generated.h"

class APawn;
class AJTSStellarEffectActor;
/** Server-owned support limits shared by all casters; never revives a dead pawn. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarSupportComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarSupportComponent();
	void HealPulse(float HealthFraction, float StaminaFraction, float OverflowFraction, float EmergencyFraction, float Delta, TSubclassOf<AJTSStellarEffectActor> EffectClass = nullptr);
	void GrantShield(bool bDark, float Fraction, float Duration, APawn* Source, FGuid CastId = FGuid(), TSubclassOf<AJTSStellarEffectActor> EffectClass = nullptr);
	void BeginParry(float Duration, FVector Facing);
	float AbsorbDamage(float Damage, AActor* Causer);
	UFUNCTION(BlueprintPure) float GetShield() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void UpdatePresentation();
	UPROPERTY(ReplicatedUsing=UpdatePresentation) float LightShield = 0;
	UPROPERTY(ReplicatedUsing=UpdatePresentation) float DarkShield = 0;
	UPROPERTY(ReplicatedUsing=UpdatePresentation) float WaterShield = 0;
	UPROPERTY(Replicated) double LightEnd = 0;
	UPROPERTY(Replicated) double DarkEnd = 0;
	UPROPERTY(Replicated) double WaterEnd = 0;
	double ParryEnd = 0, EmergencyAfter = 0;
	double HealWindowStart = -1;
	float HealedInWindow = 0, DarkRefund = 0;
	FVector ParryFacing = FVector::ForwardVector;
	TWeakObjectPtr<APawn> DarkCaster;
	FGuid DarkCastId;
	UPROPERTY(ReplicatedUsing=UpdatePresentation) TSubclassOf<AJTSStellarEffectActor> ShieldEffectClass;
	UPROPERTY(Transient) TObjectPtr<AJTSStellarEffectActor> LocalShield;
	FTimerHandle PresentationTimer;
};
