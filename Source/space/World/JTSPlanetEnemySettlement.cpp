#include "space/World/JTSPlanetEnemySettlement.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
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
		if (Enemy.IsValid()) ++Count;
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
	TArray<FVector> AcceptedLocations;
	const int32 DesiredCount = FMath::Clamp(EnemyCount, 0, 512);
	const int32 MaxAttempts = FMath::Max(32, DesiredCount * 40);
	for (int32 Attempt = 0; Attempt < MaxAttempts && SpawnedEnemies.Num() < DesiredCount; ++Attempt)
	{
		FJTSPlanetSurfaceHit Hit;
		if (!Planet->RandomPointInSurfaceCap(Planet->GetRadialUpVector(CenterHit.ImpactPoint),
			FMath::Max(0.0f, SpawnRadius), Hit)
			|| Planet->ApproximateSurfaceArcDistance(CenterHit.ImpactPoint, Hit.ImpactPoint) > SpawnRadius
			|| !AcceptSpawnPoint(Hit.ImpactPoint, Planet)) continue;
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
			Hit.ImpactPoint - CenterHit.ImpactPoint, Hit.ImpactPoint);
		FJTSPlanetSurfaceFrame Frame;
		if (!Planet->GetSurfaceFrameAt(Hit.ImpactPoint, Direction, Frame)) continue;
		const FTransform SpawnTransform(Frame.Transform.GetRotation(), Frame.Location);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.OverrideLevel = GetLevel();
		AActor* Enemy = GetWorld()->SpawnActor<AActor>(EnemyClass, SpawnTransform, Params);
		if (!IsValid(Enemy)) continue;
		if (!IJTSPlanetSettlementEnemy::Execute_InitializeForSettlement(Enemy, Planet,
			CenterFrame.Location, Hit.ImpactPoint))
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
		if (IJTSPlanetSurfaceGameplay* Gameplay = Cast<IJTSPlanetSurfaceGameplay>(SurfaceController))
		{
			Gameplay->RegisterSurfaceRuntimeActor(Enemy);
		}
		SpawnedEnemies.Add(Enemy);
		AcceptedLocations.Add(Hit.ImpactPoint);
	}
	UE_LOG(LogTemp, Log, TEXT("Enemy settlement %s spawned %d/%d actors on %s."),
		*GetName(), SpawnedEnemies.Num(), DesiredCount, *Planet->GetPlanetId().ToString());
}

void AJTSPlanetEnemySettlement::Deactivate()
{
	if (!HasAuthority()) return;
	for (const TWeakObjectPtr<AActor>& Enemy : SpawnedEnemies)
	{
		if (Enemy.IsValid()) Enemy->Destroy();
	}
	SpawnedEnemies.Reset();
	bActive = false;
}
