#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "space/Systems/JTSPlanetEnemyFragments.h"
#include "space/Systems/JTSPlanetCrowdNavigation.h"
#include "space/Systems/JTSGroundBodySeparation.h"
#include "JTSPlanetEnemySubsystem.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
struct FMassEntityManager;
struct FComponentQueryParams;

struct FJTSPlanetEnemyWorkStats
{
	float NavigationMilliseconds = 0, GatherMilliseconds = 0, SenseMilliseconds = 0;
	float SteeringMilliseconds = 0, MovementMilliseconds = 0, CombatMilliseconds = 0;
	float SeparationMilliseconds = 0, SurfaceMilliseconds = 0, TransformMilliseconds = 0;
};

/** Server-side Mass entity host. Sense, steering and combat run once per World rather than once per enemy Actor. */
UCLASS(Config = Game)
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
	/** Pool release uses UnregisterEnemy; re-acquisition registers a fresh identity. */
	void SetBodyCollisionEnabled(FMassEntityHandle Entity, bool bEnabled);
	bool GetBodyCollisionData(FMassEntityHandle Entity, FJTSPlanetEnemyBodyFragment& OutBody) const;
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
	UFUNCTION(BlueprintPure, Category = "Planet|Collision|Diagnostics")
	FJTSGroundBodySeparationStats GetBodySeparationStats() const { return BodySeparationStats; }
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Planet|Body Collision")
	FJTSGroundBodySeparationSettings BodySeparationSettings;

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
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, float DeltaSeconds,
		const FComponentQueryParams& EnvironmentParams);
	FVector ConstrainBodyMove(AActor* Actor, AJTSPlanetAnchor* Planet, const FVector& From,
		const FVector& Desired, const FQuat& Rotation, const FComponentQueryParams& Params,
		FHitResult* OutHit = nullptr) const;
	void ResolveBodyOverlap(FMassEntityManager& Manager, const TArray<FMassEntityHandle>& Entities,
		float DeltaSeconds, const FComponentQueryParams& EnvironmentParams);
	UFUNCTION()
	void HandleRegisteredEnemyDestroyed(AActor* Actor);
	void UpdateCombat(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		FJTSPlanetEnemyCombatFragment& Combat,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, const FCollisionQueryParams& SightParams);
	bool HasSightLine(const AActor* Observer, const AJTSCharacter* Player, AJTSPlanetAnchor* Planet,
		const FCollisionQueryParams& SightParams);
	FVector GetSeparation(const AActor* Actor, const AJTSPlanetAnchor* Planet,
		const FVector& Up, float Radius) const;
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
	TMap<TWeakObjectPtr<AActor>, FMassEntityHandle> RegisteredActors;
	uint64 NextBodyId = 1;
	FJTSGroundBodySeparationStats BodySeparationStats;
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
