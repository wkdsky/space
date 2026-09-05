#include "JTSTestWrappedSphereActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"

AJTSTestWrappedSphereActor::AJTSTestWrappedSphereActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMeshAsset.Object);
	}

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));
}

void AJTSTestWrappedSphereActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshMaterial();
}

void AJTSTestWrappedSphereActor::ApplyMeshMaterial()
{
	if (Mesh != nullptr)
	{
		Mesh->SetMaterial(0, MeshMaterial);
	}
}
