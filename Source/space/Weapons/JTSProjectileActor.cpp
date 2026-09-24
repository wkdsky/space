#include "space/Weapons/JTSProjectileActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

AJTSProjectileActor::AJTSProjectileActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HaloMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HaloMesh"));
	HaloMesh->SetupAttachment(SceneRoot);
	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(SceneRoot);
	for (UStaticMeshComponent* Mesh : { HaloMesh.Get(), CoreMesh.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		HaloMesh->SetStaticMesh(Cylinder.Object);
		CoreMesh->SetStaticMesh(Cylinder.Object);
	}
}

void AJTSProjectileActor::InitializeTracer(const FVector& Start, const FVector& End,
	UMaterialInterface* GlowMaterial, const FLinearColor& Color)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < 1.0f)
	{
		Destroy();
		return;
	}

	TraceStart = Start;
	TraceDirection = Delta / Length;
	TraceDistance = Length;
	TravelledDistance = 0.0f;
	SetActorLocationAndRotation(Start, FRotationMatrix::MakeFromZ(TraceDirection).Rotator());
	CoreMesh->SetRelativeScale3D(FVector(0.008f, 0.008f, 0.01f));
	HaloMesh->SetRelativeScale3D(FVector(0.028f, 0.028f, 0.01f));

	for (const TPair<UStaticMeshComponent*, float>& Layer : {
		TPair<UStaticMeshComponent*, float>(HaloMesh.Get(), 1.4f),
		TPair<UStaticMeshComponent*, float>(CoreMesh.Get(), 8.0f) })
	{
		if (IsValid(GlowMaterial))
		{
			Layer.Key->SetMaterial(0, GlowMaterial);
		}
		if (UMaterialInstanceDynamic* Dynamic = Layer.Key->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("GlowColor"), Color * Layer.Value);
			Dynamic->SetVectorParameterValue(TEXT("Color"), Color * Layer.Value);
		}
	}
	SetActorTickEnabled(true);
	SetLifeSpan(FMath::Min(0.35f, Length / FMath::Max(1000.0f, TravelSpeed) + 0.04f));
}

void AJTSProjectileActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TravelledDistance = FMath::Min(TraceDistance,
		TravelledDistance + FMath::Max(0.0f, DeltaSeconds) * FMath::Max(1000.0f, TravelSpeed));
	const float VisibleLength = FMath::Min(TravelledDistance, FMath::Max(20.0f, StreakLength));
	SetActorLocation(TraceStart + TraceDirection * (TravelledDistance - VisibleLength * 0.5f));
	// Engine cylinder diameter and height are 100 cm. Keep the core thin enough to read as a bullet.
	CoreMesh->SetRelativeScale3D(FVector(0.008f, 0.008f, VisibleLength / 100.0f));
	HaloMesh->SetRelativeScale3D(FVector(0.028f, 0.028f, VisibleLength / 100.0f));
	if (TravelledDistance >= TraceDistance)
	{
		SetActorTickEnabled(false);
		SetLifeSpan(0.025f);
	}
}
