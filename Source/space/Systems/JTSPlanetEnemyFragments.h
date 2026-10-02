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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0.05"))
	float ScanInterval = 0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0"))
	float TargetMemorySeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception", meta = (ClampMin = "0", ClampMax = "0.5"))
	float TargetLeadSeconds = 0.12f;
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
};

USTRUCT()
struct FJTSPlanetEnemyNavigationFragment : public FMassFragment
{
	GENERATED_BODY()
	FVector HomeLocation = FVector::ZeroVector;
	FVector RoamTarget = FVector::ZeroVector;
	float NextRoamTime = 0.0f;
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
