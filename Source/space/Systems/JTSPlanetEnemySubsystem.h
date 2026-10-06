#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "space/Systems/JTSPlanetEnemyFragments.h"
#include "space/Systems/JTSPlanetCrowdNavigation.h"
#include "JTSPlanetEnemySubsystem.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
struct FMassEntityManager;

struct FJTSPlanetEnemyWorkStats
{
	float NavigationMilliseconds = 0, GatherMilliseconds = 0, SenseMilliseconds = 0;
	float SteeringMilliseconds = 0, MovementMilliseconds = 0, CombatMilliseconds = 0;
	float SeparationMilliseconds = 0, SurfaceMilliseconds = 0, TransformMilliseconds = 0;
};

/** Server-side Mass entity host. Sense, steering and combat run once per World rather than once per enemy Actor. */
UCLASS()
class SPACE_API UJTSPlanetEnemySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	FMassEntityHandle RegisterEnemy(AActor* Actor, AJTSPlanetAnchor* Planet,
		const FVector& HomeLocation, const FVector& GroundLocation, const FJTSPlanetEnemyBehavior& Behavior);
	void UnregisterEnemy(FMassEntityHandle Entity);
	void NotifyDamaged(FMassEntityHandle Entity, AJTSCharacter* Attacker);
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetTrackedTargetCount() const;
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetLastScanCount() const { return LastScanCount; }
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetLastMovementCount() const { return LastMovementCount; }
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetLastSteeringCount() const { return LastSteeringCount; }
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetLastSightQueryCount() const { return LastSightQueryCount; }
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	int32 GetLastNavigationQueryCount() const { return CrowdNavigation.GetLastPhysicsQueryCount(); }
	UFUNCTION(BlueprintPure, Category = "Planet|AI|Diagnostics")
	float GetLastTickMilliseconds() const { return LastTickMilliseconds; }
	const FJTSPlanetEnemyWorkStats& GetLastWorkStats() const { return LastWorkStats; }

private:
	void ScanForTargets(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyNavigationFragment& Navigation,
		FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
		const TArray<AJTSCharacter*>& Players, const FCollisionQueryParams& SightParams);
	void UpdateSteering(const FJTSPlanetEnemyActorFragment& Binding,
		FJTSPlanetEnemyMovementFragment& Movement,
		FJTSPlanetEnemyNavigationFragment& Navigation,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
		const FCollisionQueryParams& ObstacleParams);
	void UpdateMovement(const FJTSPlanetEnemyActorFragment& Binding,
		FJTSPlanetEnemyMovementFragment& Movement,
		const FJTSPlanetEnemyNavigationFragment& Navigation,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, float DeltaSeconds);
	void UpdateCombat(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		FJTSPlanetEnemyCombatFragment& Combat,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, const FCollisionQueryParams& SightParams);
	bool HasSightLine(const AActor* Observer, const AJTSCharacter* Player, AJTSPlanetAnchor* Planet,
		const FCollisionQueryParams& SightParams);
	FVector GetSeparation(const AActor* Actor, const AJTSPlanetAnchor* Planet,
		const FVector& Up, float Radius) const;
	FVector GetContactAcceleration(const AActor* Actor, const AJTSPlanetAnchor* Planet,
		const FVector& Up, float Radius, const FVector& Velocity) const;
	static bool IsEligiblePlayer(const AJTSCharacter* Player, const AJTSPlanetAnchor* Planet);
	void ChooseRoamTarget(FJTSPlanetEnemyNavigationFragment& Navigation,
		AJTSPlanetAnchor* Planet, const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds);
	struct FSettlementAlert
	{
		TWeakObjectPtr<AJTSPlanetAnchor> Planet;
		TWeakObjectPtr<AJTSCharacter> Target;
		FVector Home, Location;
		float Until = 0;
		float ReportedTime = 0;
		bool bProvoked = false;
	};
	float ShareAlert(AJTSPlanetAnchor* Planet, const FVector& Home, AJTSCharacter* Target, float Now, float Until, bool bProvoked);
	void ReceiveAlert(AJTSPlanetAnchor* Planet, const FJTSPlanetEnemyNavigationFragment& Navigation,
		FJTSPlanetEnemyPerceptionFragment& Perception, const FJTSPlanetEnemyBehavior& Behavior, float Now);

	FMassArchetypeHandle EnemyArchetype;
	TArray<FMassEntityHandle> ActiveEntities;
	FJTSPlanetCrowdNavigation CrowdNavigation;
	TArray<FSettlementAlert> SettlementAlerts;
	struct FCrowdSample { AActor* Actor; AJTSPlanetAnchor* Planet; FVector Position; FVector Velocity; float Radius; };
	TMap<FIntVector, TArray<FCrowdSample>> SpatialBuckets;
	int32 ScanCursor = 0;
	int32 MovementCursor = 0;
	int32 LastScanCount = 0;
	int32 LastMovementCount = 0;
	int32 LastSteeringCount = 0;
	int32 LastSightQueryCount = 0;
	int32 ObstacleQueriesRemaining = 0;
	float LastTickMilliseconds = 0;
	FJTSPlanetEnemyWorkStats LastWorkStats;
};
