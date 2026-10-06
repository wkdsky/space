#include "space/World/JTSPlanetEnemySettlement.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "space/Components/JTSHealthComponent.h"
#include "Math/RotationMatrix.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "space/World/JTSPlanetSurfaceGameplay.h"

AJTSPlanetEnemySettlement::AJTSPlanetEnemySettlement()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	EditorRadius = CreateDefaultSubobject<USphereComponent>(TEXT("EncounterRadius"));
	SetRootComponent(EditorRadius);
	EditorRadius->InitSphereRadius(SpawnRadius);
	EditorRadius->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorRadius->SetHiddenInGame(true);
}

void AJTSPlanetEnemySettlement::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EditorRadius->SetSphereRadius(FMath::Max(0.0f, SpawnRadius), false);
}

void AJTSPlanetEnemySettlement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();
	Super::EndPlay(EndPlayReason);
}

bool AJTSPlanetEnemySettlement::AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const
{
	return IsValid(Planet);
}

void AJTSPlanetEnemySettlement::ConfigureSpawnedEnemy_Implementation(AActor* Enemy)
{
}

int32 AJTSPlanetEnemySettlement::GetSpawnedEnemyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Enemy : SpawnedEnemies)
	{
		if (Enemy.IsValid())
		{
			const UJTSHealthComponent* Health = Enemy->FindComponentByClass<UJTSHealthComponent>();
			if (!Health || !Health->IsDead()) ++Count;
		}
	}
	return Count;
}

void AJTSPlanetEnemySettlement::Activate(AJTSPlanetAnchor* Planet, AActor* SurfaceController)
{
	if (!HasAuthority() || bActive || !IsValid(Planet) || GetWorld() == nullptr) return;
	if (EnemyClass == nullptr || !EnemyClass->ImplementsInterface(UJTSPlanetSettlementEnemy::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy settlement %s requires a class implementing JTSPlanetSettlementEnemy."), *GetName());
		return;
	}
	FJTSPlanetSurfaceHit CenterHit;
	if (!Planet->ProjectPointToSurface(GetActorLocation(), CenterHit)
		|| !AcceptSpawnPoint(CenterHit.ImpactPoint, Planet))
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy settlement %s has no accepted real surface at its marker."), *GetName());
		return;
	}
	FJTSPlanetSurfaceFrame CenterFrame;
	if (!Planet->GetSurfaceFrameAt(CenterHit.ImpactPoint, GetActorForwardVector(), CenterFrame)) return;
	bActive = true;
	ActivePlanet = Planet;
	ActiveSurfaceController = SurfaceController;
	SettlementCenter = CenterFrame.Location;
	SpawnMissingEnemies(FMath::Clamp(EnemyCount, 0, 512));
	if (bMaintainPopulation)
		GetWorld()->GetTimerManager().SetTimer(RefillTimer, this, &ThisClass::RefillPopulation,
			FMath::Max(0.25f, RefillInterval), true);
	UE_LOG(LogTemp, Log, TEXT("Enemy settlement %s spawned %d/%d actors on %s; refill=%d."),
		*GetName(), GetSpawnedEnemyCount(), EnemyCount, *Planet->GetPlanetId().ToString(), bMaintainPopulation);
}

void AJTSPlanetEnemySettlement::RefillPopulation()
{
	if (!HasAuthority() || !bActive) return;
	if (!ActivePlanet.IsValid()) { Deactivate(); return; }
	SpawnMissingEnemies(FMath::Clamp(RefillBatchSize, 1, 512));
}

void AJTSPlanetEnemySettlement::SpawnMissingEnemies(int32 SpawnBudget)
{
	AJTSPlanetAnchor* const Planet = ActivePlanet.Get();
	if (!HasAuthority() || !bActive || !IsValid(Planet) || !GetWorld()) return;
	SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<AActor>& Enemy)
	{
		const UJTSHealthComponent* Health = Enemy.IsValid() ? Enemy->FindComponentByClass<UJTSHealthComponent>() : nullptr;
		return !Enemy.IsValid() || (Health && Health->IsDead());
	});
	TArray<FVector> AcceptedLocations;
	const int32 DesiredCount = FMath::Clamp(EnemyCount, 0, 512);
	const int32 Missing = FMath::Min(SpawnBudget, DesiredCount - SpawnedEnemies.Num());
	if (Missing <= 0) return;
	for (const auto& Enemy : SpawnedEnemies) AcceptedLocations.Add(Enemy->GetActorLocation());
	TArray<FVector> PlayerLocations;
	if (MinimumPlayerDistance > 0)
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			if (It->IsPlayerControlled()) PlayerLocations.Add(It->GetActorLocation());
	const int32 MaxAttempts = FMath::Max(32, Missing * 40);
	int32 Added = 0;
	for (int32 Attempt = 0; Attempt < MaxAttempts && Added < Missing; ++Attempt)
	{
		FJTSPlanetSurfaceHit Hit;
		if (!Planet->RandomPointInSurfaceCap(Planet->GetRadialUpVector(SettlementCenter),
			FMath::Max(0.0f, SpawnRadius), Hit)
			|| Planet->ApproximateSurfaceArcDistance(SettlementCenter, Hit.ImpactPoint) > SpawnRadius
			|| !AcceptSpawnPoint(Hit.ImpactPoint, Planet)) continue;
		if (PlayerLocations.ContainsByPredicate([&](const FVector& Position)
			{ return Planet->ApproximateSurfaceArcDistance(Position, Hit.ImpactPoint) < MinimumPlayerDistance; })) continue;
		bool bTooClose = false;
		for (const FVector& Existing : AcceptedLocations)
		{
			if (Planet->ApproximateSurfaceArcDistance(Existing, Hit.ImpactPoint) < MinimumSpacing)
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose) continue;
		const FVector Direction = Planet->ProjectDirectionToSurfaceTangent(
			Hit.ImpactPoint - SettlementCenter, Hit.ImpactPoint);
		FJTSPlanetSurfaceFrame Frame;
		if (!Planet->GetSurfaceFrameAt(Hit.ImpactPoint, Direction, Frame)) continue;
		const FTransform SpawnTransform(Frame.Transform.GetRotation(), Frame.Location);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.OverrideLevel = GetLevel();
		AActor* Enemy = GetWorld()->SpawnActor<AActor>(EnemyClass, SpawnTransform, Params);
		if (!IsValid(Enemy)) continue;
		if (!IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(Enemy, Planet,
			SettlementCenter, Hit.ImpactPoint))
		{
			Enemy->Destroy();
			continue;
		}
		ConfigureSpawnedEnemy(Enemy);
		if (!Enemy->GetIsReplicated())
		{
			UE_LOG(LogTemp, Warning, TEXT("Enemy settlement %s spawned non-replicated enemy %s."),
				*GetName(), *Enemy->GetName());
		}
		if (IJTSPlanetSurfaceGameplay* Gameplay = Cast<IJTSPlanetSurfaceGameplay>(ActiveSurfaceController.Get()))
		{
			Gameplay->RegisterSurfaceRuntimeActor(Enemy);
		}
		SpawnedEnemies.Add(Enemy);
		AcceptedLocations.Add(Hit.ImpactPoint);
		++Added;
	}
}

void AJTSPlanetEnemySettlement::Deactivate()
{
	if (!HasAuthority()) return;
	bActive = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefillTimer);
	for (const TWeakObjectPtr<AActor>& Enemy : SpawnedEnemies)
	{
		if (Enemy.IsValid()) Enemy->Destroy();
	}
	SpawnedEnemies.Reset();
	ActivePlanet.Reset();
	ActiveSurfaceController.Reset();
}
