#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSBossCombatComponent.generated.h"

class AJTSPlanetBoss;
class AJTSPlanetAnchor;
class APawn;
class UAnimSequence;

UENUM(BlueprintType)
enum class EJTSBossAction : uint8 { Emerge, Idle, Walk, Slam, Sweep, Charge, Leap, Stagger, Death, TurnLeft, TurnRight };

USTRUCT(BlueprintType)
struct FJTSBossAttack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float Windup = 1.1f;
	UPROPERTY(EditAnywhere) float Active = .3f;
	UPROPERTY(EditAnywhere) float Recovery = 1.3f;
	UPROPERTY(EditAnywhere) float Damage = 2;
	UPROPERTY(EditAnywhere) float Reach = 520;
	UPROPERTY(EditAnywhere) float Radius = 250;
	UPROPERTY(EditAnywhere) float ArcDegrees = 130;
	UPROPERTY(EditAnywhere) float Cooldown = 5;
	float Duration() const { return FMath::Max(.1f, Windup + Active + Recovery); }
};

/** Server owns decisions, locked attack intent, active damage windows and traversal. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSBossCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSBossCombatComponent();
	void Initialize(AJTSPlanetAnchor* InPlanet);
	void Die();
	void ReactToDamage(float Damage);
	UFUNCTION(BlueprintPure) EJTSBossAction GetAction() const { return Action; }
	UFUNCTION(BlueprintPure) float GetActionElapsed() const;
	float GetActionDuration() const;
	bool IsVulnerable() const;
	bool IsDamageable() const;
	AJTSPlanetAnchor* GetPlanet() const { return Planet; }
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") TMap<EJTSBossAction, FJTSBossAttack> Attacks;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float AggroRadius = 7500;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float LeashRadius = 6500;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float WalkSpeed = 190;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float TurnSpeedDegrees = 72;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float TurnAccelerationDegrees = 180;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float ChargeSpeed = 1150;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float LeapHeight = 420;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float AttackPause = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float EmergenceDuration = 3;
	UPROPERTY(EditDefaultsOnly, Category="Boss|Combat") float StaggerThreshold = 90;
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void OnRep_Action();
	void Update();
	void BeginAction(EJTSBossAction NewAction);
	void ChooseAttack(APawn* Pawn);
	APawn* FindTarget() const;
	float Face(const FVector& Point, float Dt);
	void Move(const FVector& Direction, float Speed, float Dt);
	void ApplyAttack(float Phase, const FVector& PreviousLocation);
	double Now() const;
	UPROPERTY(ReplicatedUsing=OnRep_Action) EJTSBossAction Action = EJTSBossAction::Emerge;
	UPROPERTY(ReplicatedUsing=OnRep_Action) double ActionStart = 0;
	UPROPERTY(Replicated) FVector AttackAim = FVector::ZeroVector;
	UPROPERTY(Replicated) TObjectPtr<AJTSPlanetAnchor> Planet;
	TWeakObjectPtr<AJTSPlanetBoss> Boss;
	TWeakObjectPtr<APawn> Target;
	FVector Home = FVector::ZeroVector;
	FVector AttackOrigin = FVector::ZeroVector;
	FVector AttackForward = FVector::ForwardVector;
	TMap<EJTSBossAction, double> ReadyAt;
	TSet<TWeakObjectPtr<APawn>> HitPlayers;
	EJTSBossAction LastAttack = EJTSBossAction::Slam;
	double LastUpdate = 0;
	double NextDecision = 0;
	double NextStagger = 0;
	float StaggerDamage = 0;
	float TurnVelocity = 0;
	int32 ComboCount = 0;
	bool bImpactApplied = false;
	FTimerHandle UpdateTimer;
};
