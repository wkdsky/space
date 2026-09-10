#include "space/World/JTSMoonCorpseActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ConfigureCorpsePrimitive(UStaticMeshComponent* Component)
	{
		if (Component == nullptr)
		{
			return;
		}

		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}

	void ApplyColor(UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		if (!IsValid(Component))
		{
			return;
		}

		if (UMaterialInstanceDynamic* const Material = Component->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Material->SetVectorParameterValue(TEXT("Tint"), Color);
		}
	}
}

AJTSMoonCorpseActor::AJTSMoonCorpseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SkullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkullMesh"));
	SkullMesh->SetupAttachment(SceneRoot);
	SkullMesh->SetRelativeLocation(FVector(0.0f, 70.0f, 16.0f));
	SkullMesh->SetRelativeScale3D(FVector(0.24f, 0.22f, 0.22f));
	ConfigureCorpsePrimitive(SkullMesh);

	TorsoClothingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorsoClothingMesh"));
	TorsoClothingMesh->SetupAttachment(SceneRoot);
	TorsoClothingMesh->SetRelativeLocation(FVector(0.0f, 8.0f, 14.0f));
	TorsoClothingMesh->SetRelativeScale3D(FVector(0.30f, 0.68f, 0.14f));
	ConfigureCorpsePrimitive(TorsoClothingMesh);

	HipClothingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HipClothingMesh"));
	HipClothingMesh->SetupAttachment(SceneRoot);
	HipClothingMesh->SetRelativeLocation(FVector(0.0f, -54.0f, 12.0f));
	HipClothingMesh->SetRelativeScale3D(FVector(0.27f, 0.26f, 0.12f));
	ConfigureCorpsePrimitive(HipClothingMesh);

	LeftArmBoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmBoneMesh"));
	LeftArmBoneMesh->SetupAttachment(SceneRoot);
	LeftArmBoneMesh->SetRelativeLocation(FVector(-33.0f, 18.0f, 10.0f));
	LeftArmBoneMesh->SetRelativeRotation(FRotator(0.0f, -24.0f, 0.0f));
	LeftArmBoneMesh->SetRelativeScale3D(FVector(0.075f, 0.48f, 0.065f));
	ConfigureCorpsePrimitive(LeftArmBoneMesh);

	RightArmBoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmBoneMesh"));
	RightArmBoneMesh->SetupAttachment(SceneRoot);
	RightArmBoneMesh->SetRelativeLocation(FVector(33.0f, 18.0f, 10.0f));
	RightArmBoneMesh->SetRelativeRotation(FRotator(0.0f, 24.0f, 0.0f));
	RightArmBoneMesh->SetRelativeScale3D(FVector(0.075f, 0.48f, 0.065f));
	ConfigureCorpsePrimitive(RightArmBoneMesh);

	LeftLegBoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLegBoneMesh"));
	LeftLegBoneMesh->SetupAttachment(SceneRoot);
	LeftLegBoneMesh->SetRelativeLocation(FVector(-14.0f, -105.0f, 9.0f));
	LeftLegBoneMesh->SetRelativeRotation(FRotator(0.0f, -7.0f, 0.0f));
	LeftLegBoneMesh->SetRelativeScale3D(FVector(0.085f, 0.55f, 0.065f));
	ConfigureCorpsePrimitive(LeftLegBoneMesh);

	RightLegBoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLegBoneMesh"));
	RightLegBoneMesh->SetupAttachment(SceneRoot);
	RightLegBoneMesh->SetRelativeLocation(FVector(14.0f, -105.0f, 9.0f));
	RightLegBoneMesh->SetRelativeRotation(FRotator(0.0f, 8.0f, 0.0f));
	RightLegBoneMesh->SetRelativeScale3D(FVector(0.085f, 0.55f, 0.065f));
	ConfigureCorpsePrimitive(RightLegBoneMesh);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (SphereMeshAsset.Succeeded())
	{
		SkullMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	if (CubeMeshAsset.Succeeded())
	{
		TorsoClothingMesh->SetStaticMesh(CubeMeshAsset.Object);
		HipClothingMesh->SetStaticMesh(CubeMeshAsset.Object);
		LeftArmBoneMesh->SetStaticMesh(CubeMeshAsset.Object);
		RightArmBoneMesh->SetStaticMesh(CubeMeshAsset.Object);
		LeftLegBoneMesh->SetStaticMesh(CubeMeshAsset.Object);
		RightLegBoneMesh->SetStaticMesh(CubeMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		BasicPrototypeMaterial = BasicMaterialAsset.Object;
	}

	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		for (UStaticMeshComponent* Component : {
			SkullMesh.Get(), TorsoClothingMesh.Get(), HipClothingMesh.Get(), LeftArmBoneMesh.Get(),
			RightArmBoneMesh.Get(), LeftLegBoneMesh.Get(), RightLegBoneMesh.Get()})
		{
			if (IsValid(Component))
			{
				Component->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
			}
		}
	}
	else if (BasicPrototypeMaterial != nullptr)
	{
		for (UStaticMeshComponent* Component : {
			SkullMesh.Get(), TorsoClothingMesh.Get(), HipClothingMesh.Get(), LeftArmBoneMesh.Get(),
			RightArmBoneMesh.Get(), LeftLegBoneMesh.Get(), RightLegBoneMesh.Get()})
		{
			if (IsValid(Component))
			{
				Component->SetMaterial(0, BasicPrototypeMaterial);
			}
		}
	}
}

void AJTSMoonCorpseActor::AdjustToGround(const FVector& GroundLocation)
{
	if (bUsesRealPlanetSurfacePlacement)
	{
		return;
	}

	const FBox Bounds = GetComponentsBoundingBox(true);
	if (Bounds.IsValid)
	{
		FVector AdjustedLocation = GetActorLocation();
		AdjustedLocation.Z += GroundLocation.Z - Bounds.Min.Z + 2.0f;
		SetActorLocation(AdjustedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		UpdateMoonWrappedLogicalPosition();
	}
}

bool AJTSMoonCorpseActor::SnapToPlanetSurfaceAnchor(AJTSPlanetSurfaceAnchor* SurfaceAnchor)
{
	if (!IsValid(SurfaceAnchor))
	{
		return false;
	}

	FTransform SurfaceTransform;
	if (!SurfaceAnchor->GetSurfaceTransform(SurfaceTransform))
	{
		return false;
	}

	const FVector SurfaceUp = SurfaceTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero())
	{
		return false;
	}

	DisableLegacyMoonPresentation();
	SetActorLocationAndRotation(
		SurfaceTransform.GetLocation() + SurfaceUp * FMath::Max(0.0f, PlanetSurfaceClearance),
		SurfaceTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ApplyPrototypeMaterials();
	return true;
}

bool AJTSMoonCorpseActor::IsUsingRealPlanetSurfacePlacement() const
{
	return bUsesRealPlanetSurfacePlacement;
}

void AJTSMoonCorpseActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyPrototypeMaterials();
}

void AJTSMoonCorpseActor::ApplyPrototypeMaterials()
{
	const FLinearColor BoneColor(0.70f, 0.64f, 0.48f, 1.0f);
	const FLinearColor TornFabricColor(0.13f, 0.18f, 0.24f, 1.0f);
	const FLinearColor TornFabricAccent(0.24f, 0.12f, 0.09f, 1.0f);
	ApplyColor(SkullMesh, BoneColor);
	ApplyColor(LeftArmBoneMesh, BoneColor);
	ApplyColor(RightArmBoneMesh, BoneColor);
	ApplyColor(LeftLegBoneMesh, BoneColor);
	ApplyColor(RightLegBoneMesh, BoneColor);
	ApplyColor(TorsoClothingMesh, TornFabricColor);
	ApplyColor(HipClothingMesh, TornFabricAccent);
}

void AJTSMoonCorpseActor::UpdateMoonWrappedLogicalPosition()
{
	if (!bUsesRealPlanetSurfacePlacement
		&& MoonWrappedActorComponent != nullptr
		&& MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}

void AJTSMoonCorpseActor::DisableLegacyMoonPresentation()
{
	bUsesRealPlanetSurfacePlacement = true;
	if (MoonWrappedActorComponent != nullptr)
	{
		MoonWrappedActorComponent->Deactivate();
		MoonWrappedActorComponent->SetComponentTickEnabled(false);
	}

	if (BasicPrototypeMaterial == nullptr)
	{
		return;
	}

	for (UStaticMeshComponent* Component : {
		SkullMesh.Get(), TorsoClothingMesh.Get(), HipClothingMesh.Get(), LeftArmBoneMesh.Get(),
		RightArmBoneMesh.Get(), LeftLegBoneMesh.Get(), RightLegBoneMesh.Get()})
	{
		if (IsValid(Component))
		{
			Component->SetMaterial(0, BasicPrototypeMaterial);
		}
	}
}
