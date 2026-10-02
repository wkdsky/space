#pragma once

#include "CoreMinimal.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "space/Systems/JTSPlanetEnemyFragments.h"
#include "JTSPlanetEnemySubsystem.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
struct FMassEntityManager;

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

private:
	void ScanForTargets(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyNavigationFragment& Navigation,
		FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds,
		const TArray<AJTSCharacter*>& Players, const TArray<AActor*>& EnemyActors);
	void UpdateMovement(const FJTSPlanetEnemyActorFragment& Binding,
		FJTSPlanetEnemyMovementFragment& Movement,
		FJTSPlanetEnemyNavigationFragment& Navigation,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, float DeltaSeconds);
	void UpdateCombat(const FJTSPlanetEnemyActorFragment& Binding,
		const FJTSPlanetEnemyPerceptionFragment& Perception,
		FJTSPlanetEnemyCombatFragment& Combat,
		const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds, const TArray<AActor*>& EnemyActors);
	bool HasSightLine(const AActor* Observer, const AJTSCharacter* Player, AJTSPlanetAnchor* Planet,
		const TArray<AActor*>& EnemyActors) const;
	static bool IsEligiblePlayer(const AJTSCharacter* Player, const AJTSPlanetAnchor* Planet);
	void ChooseRoamTarget(FJTSPlanetEnemyNavigationFragment& Navigation,
		AJTSPlanetAnchor* Planet, const FJTSPlanetEnemyBehavior& Behavior, float TimeSeconds);

	FMassArchetypeHandle EnemyArchetype;
	TArray<FMassEntityHandle> ActiveEntities;
};
