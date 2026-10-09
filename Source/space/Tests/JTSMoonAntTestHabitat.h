#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UnrealType.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonAntNestActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonSurfaceGameplayData.h"
#include "space/World/JTSPlanetAnchor.h"

/** Real spherical collision surface and authored ant presentation, without resource/game-flow initialization. */
struct FJTSMoonAntTestHabitat
{
	UWorld* World;
	AJTSPlanetAnchor* Planet;
	AJTSMoonSurfaceController* Controller;
	AJTSMoonAntNestActor* Nest;
	UClass* AntClass;

	explicit FJTSMoonAntTestHabitat(UWorld* InWorld) : World(InWorld)
	{
		Planet = World->SpawnActor<AJTSPlanetAnchor>();
		Planet->SetActorLocation(FVector(0, 0, -10000));
		auto* Surface = World->SpawnActor<AStaticMeshActor>();
		Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Surface->SetActorLocation(Planet->GetActorLocation());
		auto* Mesh = DuplicateObject<UStaticMesh>(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")), World);
		// Surface projection uses render triangles. Weapon LOS must use the same surface, rather than
		// the larger smooth simple sphere enclosing the low-poly ground and its tiny ants.
		Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
		Surface->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		Surface->SetActorScale3D(FVector(200));
		Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
		FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000.f);
		Controller = World->SpawnActor<AJTSMoonSurfaceController>();
		Controller->SetOwningPlanet(Planet);
		auto* Data = NewObject<UJTSMoonSurfaceGameplayData>(World);
		for (const TCHAR* Property : {TEXT("MoonAntSpawnIntervalMin"), TEXT("MoonAntSpawnIntervalMax"),
			TEXT("MoonAntSurfaceDurationMin"), TEXT("MoonAntSurfaceDurationMax")})
			FindFProperty<FFloatProperty>(Data->GetClass(), Property)->SetPropertyValue_InContainer(Data, 120.f);
		FindFProperty<FObjectProperty>(Controller->GetClass(), TEXT("MoonGameplayData"))->SetObjectPropertyValue_InContainer(Controller, Data);
		Nest = World->SpawnActor<AJTSMoonAntNestActor>(Ground(FVector::ZeroVector), FRotator::ZeroRotator);
		Controller->RegisterSurfaceRuntimeActor(Nest);
		AntClass = LoadClass<AJTSMoonAntActor>(nullptr, TEXT("/Game/Space/Blueprints/Planets/Moon/BP_MoonAnt.BP_MoonAnt_C"));
	}
	FVector Ground(const FVector& Candidate) const
	{
		FJTSPlanetSurfaceHit Hit;
		return Planet->ProjectPointToSurface(Candidate, Hit) ? Hit.ImpactPoint : Candidate;
	}
	AJTSMoonAntActor* SpawnAnt(const FVector& Candidate)
	{
		if (!AntClass) return nullptr;
		const FVector Point = Ground(Candidate);
		const FTransform Transform(FRotationMatrix::MakeFromZ(Planet->GetRadialUpVector(Point)).ToQuat(), Point);
		auto* Ant = World->SpawnActorDeferred<AJTSMoonAntActor>(AntClass, Transform, Nest, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Ant->InitializeMoonAnt(Nest, Point);
		Controller->RegisterSurfaceRuntimeActor(Ant);
		Ant->FinishSpawning(Transform);
		return Ant;
	}
};
#endif
