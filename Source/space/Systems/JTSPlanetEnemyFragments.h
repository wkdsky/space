#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "JTSPlanetEnemyFragments.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
class AActor;

/** Tuned on the enemy Blueprint. The Mass entity owns a copy so behavior systems never read an Actor CDO each frame. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSPlanetEnemyBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float RoamRadius = 620.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float LeashRadius = 1100.0f;
	/** Home-centred entry radius. LeashRadius is the slightly larger normal release boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float AggroRadius = 1050.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float RetaliationLeashRadius = 2500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float HomeReturnRadius = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float ReacquireCooldown = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float SightRadius = 1050.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float RoamSpeed = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float ChaseSpeed = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float Acceleration = 520.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float TurnSpeedDegrees = 420.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float HoverHeight = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "1"))
	float CollisionRadius = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0.05"))
	float ScanInterval = 0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float TargetMemorySeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0", ClampMax = "0.5"))
	float TargetLeadSeconds = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	bool bAcquireOnSpawn = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	bool bShareSettlementAlert = true;
	/** Fixed maximum duration of an outside-guard retaliation episode; later hits never extend it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float RetaliationMemorySeconds = 5.0f;
	/** Expensive steering refresh only. Position, rotation, forces and collision still advance every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering", meta = (ClampMin = "0.025"))
	float MovementInterval = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering", meta = (ClampMin = "0.05"))
	float IdleMovementInterval = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steering", meta = (ClampMin = "0.05"))
	float DistantMovementInterval = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float DistantMovementDistance = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float SeparationRadius = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float SeparationWeight = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.1"))
	float RoamRetargetSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0"))
	float AttackRange = 145.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0"))
	float AttackReach = 165.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0"))
	float AttackWindup = 0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.01"))
	float AttackCooldown = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0"))
	float AttackDamage = 0.0f;
};

/** UObject references stay isolated from the cache-friendly numeric fragments. */
USTRUCT()
struct FJTSPlanetEnemyActorFragment : public FMassFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<AJTSPlanetAnchor> Planet;
};

template<> struct TMassFragmentTraits<FJTSPlanetEnemyActorFragment>
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct FJTSPlanetEnemyMovementFragment : public FMassFragment
{
	GENERATED_BODY()
	FVector GroundLocation = FVector::ZeroVector;
	FVector SurfaceUp = FVector::UpVector;
	FVector Velocity = FVector::ZeroVector;
	FVector DesiredVelocity = FVector::ZeroVector;
	FVector GoalLocation = FVector::ZeroVector;
	float StopDistance = 0.0f;
	float NextUpdateTime = 0.0f;
	float LastUpdateTime = 0.0f;
	float RequestedInterval = 0.05f;
	float NextForceImpactTime = 0.0f;
};

USTRUCT()
struct FJTSPlanetEnemyNavigationFragment : public FMassFragment
{
	GENERATED_BODY()
	FVector HomeLocation = FVector::ZeroVector;
	FVector RoamTarget = FVector::ZeroVector;
	float NextRoamTime = 0.0f;
	FVector AvoidanceDirection = FVector::ZeroVector;
	float NextObstacleProbeTime = 0.0f;
	float RouteUntilTime = 0.0f;
	bool bReturningHome = false;
};

USTRUCT()
struct FJTSPlanetEnemyPerceptionFragment : public FMassFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<AJTSCharacter> Target;
	FVector LastKnownLocation = FVector::ZeroVector;
	float NextScanTime = 0.0f;
	float LastSensedTime = -1000.0f;
	float RetaliationUntilTime = -1000.0f;
	float NextAcquireAllowedTime = 0.0f;
	// A completed episode cannot be rejoined through a still-live colony alert after returning.
	float CompletedRetaliationUntilTime = -1000.0f;
	bool bProvokedPursuit = false;
	bool bCurrentlyVisible = false;
};

template<> struct TMassFragmentTraits<FJTSPlanetEnemyPerceptionFragment>
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct FJTSPlanetEnemyCombatFragment : public FMassFragment
{
	GENERATED_BODY()
	TWeakObjectPtr<AJTSCharacter> PendingVictim;
	float NextAttackTime = 0.0f;
	float ImpactTime = 0.0f;
	bool bImpactPending = false;
};

template<> struct TMassFragmentTraits<FJTSPlanetEnemyCombatFragment>
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

USTRUCT()
struct FJTSPlanetEnemyBehaviorFragment : public FMassFragment
{
	GENERATED_BODY()
	FJTSPlanetEnemyBehavior Values;
};
