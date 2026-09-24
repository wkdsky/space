#include "space/Weapons/JTSProjectileImpactActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float TemporaryImpactLifeSeconds = 0.55f;
constexpr float ImpactFlashLifeSeconds = 0.085f;
}

AJTSProjectileImpactActor::AJTSProjectileImpactActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	InitialLifeSpan = TemporaryImpactLifeSeconds;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ImpactMark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactMark"));
	ImpactMark->SetupAttachment(SceneRoot);
	ImpactMark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactMark->SetGenerateOverlapEvents(false);
	ImpactMark->SetCanEverAffectNavigation(false);
	ImpactMark->SetCastShadow(false);
	ImpactMark->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.004f));

	ImpactFlashA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlashA"));
	ImpactFlashA->SetupAttachment(SceneRoot);
	ImpactFlashA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactFlashA->SetGenerateOverlapEvents(false);
	ImpactFlashA->SetCanEverAffectNavigation(false);
	ImpactFlashA->SetCastShadow(false);
	ImpactFlashA->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ImpactFlashA->SetRelativeScale3D(FVector(0.18f, 0.02f, 0.02f));

	ImpactFlashB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlashB"));
	ImpactFlashB->SetupAttachment(SceneRoot);
	ImpactFlashB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactFlashB->SetGenerateOverlapEvents(false);
	ImpactFlashB->SetCanEverAffectNavigation(false);
	ImpactFlashB->SetCastShadow(false);
	ImpactFlashB->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	ImpactFlashB->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	ImpactFlashB->SetRelativeScale3D(FVector(0.18f, 0.02f, 0.02f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		CubeMesh = CubeMeshAsset.Object;
		ImpactMark->SetStaticMesh(CubeMesh);
		ImpactFlashA->SetStaticMesh(CubeMesh);
		ImpactFlashB->SetStaticMesh(CubeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		ImpactMark->SetMaterial(0, BasicMaterialAsset.Object);
		ImpactFlashA->SetMaterial(0, BasicMaterialAsset.Object);
		ImpactFlashB->SetMaterial(0, BasicMaterialAsset.Object);
	}
	ImpactMark->SetHiddenInGame(false);
	ImpactFlashA->SetHiddenInGame(false);
	ImpactFlashB->SetHiddenInGame(false);
}

void AJTSProjectileImpactActor::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(
		ImpactFlashTimerHandle,
		this,
		&AJTSProjectileImpactActor::HideImpactFlash,
		ImpactFlashLifeSeconds,
		false);
}

void AJTSProjectileImpactActor::InitializeImpact(const FVector& ImpactNormal,
	UMaterialInterface* GlowMaterial, const FLinearColor& Color, bool bShowMark)
{
	FVector SafeNormal = ImpactNormal.GetSafeNormal();
	if (SafeNormal.IsNearlyZero())
	{
		SafeNormal = FVector::UpVector;
	}

	const FVector ReferenceAxis = FMath::Abs(SafeNormal.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
	const FVector Tangent = FVector::CrossProduct(ReferenceAxis, SafeNormal).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		SetActorRotation(FRotationMatrix::MakeFromXZ(Tangent, SafeNormal).Rotator());
	}
	ImpactMark->SetVisibility(bShowMark, true);
	if (bShowMark)
	{
		ImpactMarkMaterial = ImpactMark->CreateAndSetMaterialInstanceDynamic(0);
		if (IsValid(ImpactMarkMaterial))
		{
			ImpactMarkMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.025f, 0.03f));
		}
	}
	if (IsValid(GlowMaterial))
	{
		ImpactFlashA->SetMaterial(0, GlowMaterial);
		ImpactFlashB->SetMaterial(0, GlowMaterial);
	}
	ImpactFlashMaterial = ImpactFlashA->CreateAndSetMaterialInstanceDynamic(0);
	ImpactFlashB->SetMaterial(0, ImpactFlashMaterial);
	if (IsValid(ImpactFlashMaterial))
	{
		ImpactFlashMaterial->SetVectorParameterValue(TEXT("GlowColor"), Color * 6.0f);
		ImpactFlashMaterial->SetVectorParameterValue(TEXT("Color"), Color * 6.0f);
	}
	SetLifeSpan(TemporaryImpactLifeSeconds);
}

void AJTSProjectileImpactActor::HideImpactFlash()
{
	ImpactFlashA->SetVisibility(false, true);
	ImpactFlashB->SetVisibility(false, true);
}
