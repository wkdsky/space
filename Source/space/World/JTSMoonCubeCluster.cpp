#include "space/World/JTSMoonCubeCluster.h"

#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "space/World/JTSMoonCubeEnemy.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSMoonCubeCluster::AJTSMoonCubeCluster()
{
	EnemyClass = AJTSMoonCubeEnemy::StaticClass();
}

bool AJTSMoonCubeCluster::AcceptSpawnPoint_Implementation(FVector GroundPoint, AJTSPlanetAnchor* Planet) const
{
	if (!IsValid(SunLight) || !IsValid(Planet) || GetWorld() == nullptr) return false;
	const FVector Sunward = -SunLight->GetActorForwardVector();
	const FVector Up = Planet->GetRadialUpVector(GroundPoint);
	if (FVector::DotProduct(Up, Sunward) < MinimumSunDot) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MoonCubeSunlight), false, this);
	for (const TWeakObjectPtr<AActor>& Enemy : SpawnedEnemies)
	{
		if (Enemy.IsValid()) Params.AddIgnoredActor(Enemy.Get());
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit,
		GroundPoint + Up * 95.0f, GroundPoint + Up * 95.0f + Sunward * 7000.0f,
		ECC_Visibility, Params);
}

void AJTSMoonCubeCluster::ConfigureSpawnedEnemy_Implementation(AActor* Enemy)
{
	if (AJTSMoonCubeEnemy* Cube = Cast<AJTSMoonCubeEnemy>(Enemy))
	{
		Cube->ConfigurePresentation(BodyMeshAsset, WeakPointMeshAsset, BodyMaterial, WeakPointMaterial);
	}
}
