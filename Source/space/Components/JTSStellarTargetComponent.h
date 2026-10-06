#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSStellarTargetComponent.generated.h"

class APawn;
UENUM(BlueprintType)
enum class EJTSStellarTargetTier : uint8 { Normal, Elite, Boss };

struct FJTSStellarDamageSource
{
	TWeakObjectPtr<APawn> Pawn;
	float FireDPS = 0;
	float LightDPS = 0;
	double FireEnd = 0;
	double LightEnd = 0;
};
struct FJTSStellarForceSource
{
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<AActor> Field;
	FVector Center = FVector::ZeroVector;
	float Strength = 0;
	float Radius = 0;
	float SofteningRadius = 0;
	float EntryDepth = 30;
	float ExitOffset = 12;
	bool bRepulsion = false;
	bool bRepulsionEngaged = false;
	double End = 0;
};

/** Opt-in hostile target: capped status sources, execution resistance and gravity-relative control. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarTargetComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarTargetComponent();
	UFUNCTION(BlueprintPure) bool IsAliveTarget() const;
	UFUNCTION(BlueprintPure) EJTSStellarTargetTier GetTier() const { return Tier; }
	void ApplyFire(APawn* Source, float Heat, float BurnDPS, int32 SpreadTargets);
	void ApplyLightBurn(APawn* Source, float DPS, float Duration, int32 SpreadTargets, float SpreadScale);
	void RemoveForce(APawn* Source);
	/** Each persistent field has its own force key, even when cast by the same player. */
	void ApplyAttraction(APawn* Source, AActor* Field, const FVector& Center, float Acceleration,
		float Radius, float SofteningRadius, float Duration);
	/** A moving, finite-radius spring barrier; stiffness is acceleration per centimetre of penetration. */
	void ApplyRepulsion(APawn* Source, float Stiffness, float Radius, float Duration,
		float EntryDepth = 30.0f, float ExitOffset = 12.0f);
	void RemoveFieldForce(AActor* Field);
	bool HasActiveFieldForces() const;
	/** A target born inside the caster's capsule must be able to sweep out during repulsion. */
	void GetRepulsionCasters(TArray<AActor*, TInlineAllocator<4>>& Out) const;
	FVector GetFieldAcceleration(const FVector& Position, const FVector& SurfaceUp, const FVector& Velocity = FVector::ZeroVector);
	/** Small-step force integration. The visual core is non-solid; crowd contacts are provided by Mass. */
	FVector IntegrateFieldMotion(const FVector& Position, const FVector& SurfaceUp,
		const FVector& DesiredVelocity, float SteeringAcceleration, const FVector& ContactAcceleration,
		float DeltaSeconds, FVector& Velocity, float& ImpactSpeed);
	static void QueryTargets(UWorld* World, const FVector& Center, float Radius, int32 Limit, TArray<AActor*>& Out);
	static bool HasLineOfSight(UWorld* World, const FVector& From, AActor* Target, AActor* Source);
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	void StatusPulse();
	void WakeStatusTimer();
	void ExecuteIgnition(APawn* Source);
	bool IsActiveForce(const FJTSStellarForceSource& Source) const;
	UPROPERTY(EditDefaultsOnly, Category="Stellar|Physics", meta=(ClampMin="0")) float FieldDrag = 1.2f;
	UPROPERTY(EditDefaultsOnly, Category="Stellar|Physics", meta=(ClampMin="100")) float MaximumFieldSpeed = 1800.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) EJTSStellarTargetTier Tier = EJTSStellarTargetTier::Normal;
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1")) float IgnitionThreshold = 100.0f;
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0")) float ResistantIgnitionDamage = 180.0f;
	UPROPERTY(Replicated, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) bool bIgnited = false;
	UPROPERTY(Replicated, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) bool bLightBurning = false;
	UPROPERTY(Replicated, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) float PosturePressure = 0.0f;
	float HeatAmount = 0.0f;
	double LastHeatTime = 0;
	double IgnitionEnd = 0;
	TWeakObjectPtr<APawn> Igniter;
	TArray<FJTSStellarDamageSource> DamageSources;
	TArray<FJTSStellarForceSource> Forces;
	FTimerHandle StatusTimer;
};
