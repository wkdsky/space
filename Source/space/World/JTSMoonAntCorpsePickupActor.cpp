#include "space/World/JTSMoonAntCorpsePickupActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FVector SanitizeVisualScale(const FVector& InScale)
	{
		const FVector AbsoluteScale(
			FMath::Abs(InScale.X),
			FMath::Abs(InScale.Y),
			FMath::Abs(InScale.Z));
		return FMath::IsFinite(AbsoluteScale.X)
			&& FMath::IsFinite(AbsoluteScale.Y)
			&& FMath::IsFinite(AbsoluteScale.Z)
			&& AbsoluteScale.X > KINDA_SMALL_NUMBER
			&& AbsoluteScale.Y > KINDA_SMALL_NUMBER
			&& AbsoluteScale.Z > KINDA_SMALL_NUMBER
			? AbsoluteScale
			: FVector::OneVector;
	}
}

AJTSMoonAntCorpsePickupActor::AJTSMoonAntCorpsePickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorTickEnabled(true);

	CorpseMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CorpseMesh"));
	CorpseMesh->SetupAttachment(GetRootComponent());
	CorpseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpseMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CorpseMesh->SetGenerateOverlapEvents(false);
	CorpseMesh->SetSimulatePhysics(false);
	CorpseMesh->SetCanEverAffectNavigation(false);
	CorpseMesh->SetCastShadow(true);
	CorpseMesh->bCastDynamicShadow = true;
	CorpseMesh->SetHiddenInGame(true);
	CorpseMesh->SetVisibility(false);

	CorpseFallbackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CorpseFallbackMesh"));
	CorpseFallbackMesh->SetupAttachment(GetRootComponent());
	CorpseFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpseFallbackMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CorpseFallbackMesh->SetGenerateOverlapEvents(false);
	CorpseFallbackMesh->SetSimulatePhysics(false);
	CorpseFallbackMesh->SetCanEverAffectNavigation(false);
	CorpseFallbackMesh->SetCastShadow(true);
	CorpseFallbackMesh->bCastDynamicShadow = true;
	CorpseFallbackMesh->SetHiddenInGame(true);
	CorpseFallbackMesh->SetVisibility(false);

	PickupInteractionCollider = CreateDefaultSubobject<USphereComponent>(TEXT("PickupInteractionCollider"));
	PickupInteractionCollider->SetupAttachment(GetRootComponent());
	PickupInteractionCollider->InitSphereRadius(28.0f);
	PickupInteractionCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupInteractionCollider->SetCollisionObjectType(ECC_WorldDynamic);
	PickupInteractionCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupInteractionCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupInteractionCollider->SetGenerateOverlapEvents(false);
	PickupInteractionCollider->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackSphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (FallbackSphereAsset.Succeeded())
	{
		DebugFallbackMeshAsset = FallbackSphereAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		DebugFallbackMaterial = BasicMaterialAsset.Object;
	}
}

void AJTSMoonAntCorpsePickupActor::InitializeFromMoonAnt(
	USkeletalMesh* SourceSkeletalMesh,
	UStaticMesh* SourceFallbackMesh,
	const TArray<UMaterialInterface*>& InSourceMaterials,
	const FVector& InSourceVisualScale,
	const FRotator& SourceVisualRotation,
	const FVector& InDeathGroundLocation)
{
	InitializeItem(EJTSWorldPickupItemType::MoonAntCorpse);
	SourceSkeletalMeshAsset = SourceSkeletalMesh;
	SourceFallbackMeshAsset = SourceFallbackMesh;
	SourceVisualScale = SanitizeVisualScale(InSourceVisualScale);
	DeathGroundLocation = InDeathGroundLocation;
	bInitializedFromMoonAnt = true;

	SourceVisualMaterials.Reset(InSourceMaterials.Num());
	for (UMaterialInterface* const SourceMaterial : InSourceMaterials)
	{
		SourceVisualMaterials.Add(SourceMaterial);
	}

	// SourceVisualRotation already contains BP_MoonAnt's imported-mesh yaw correction. Multiplying a
	// local-X roll after it keeps the MoonAnt's long axis as the side-flip axis instead of merely yawing it.
	const float SignedSideRoll = FMath::RandBool() ? CorpseSideRollDegrees : -CorpseSideRollDegrees;
	const FQuat SourceVisualBasis = SourceVisualRotation.Quaternion();
	const FQuat LocalSideFlip(FVector::ForwardVector, FMath::DegreesToRadians(SignedSideRoll));
	CorpseBaseRelativeRotation = (SourceVisualBasis * LocalSideFlip).Rotator();
	CorpseBaseRelativeLocation = FVector::ZeroVector;

	ConfigureCorpseVisual();
}

bool AJTSMoonAntCorpsePickupActor::HasVisibleCorpseVisual() const
{
	const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual();
	if (!IsValid(ActiveVisual) || !ActiveVisual->IsVisible())
	{
		return false;
	}

	if (bUseSkeletalCorpseVisual)
	{
		return IsValid(CorpseMesh) && CorpseMesh->GetSkeletalMeshAsset() != nullptr;
	}

	return IsValid(CorpseFallbackMesh) && CorpseFallbackMesh->GetStaticMesh() != nullptr;
}

FString AJTSMoonAntCorpsePickupActor::GetCorpseVisualDebugName() const
{
	if (bUseSkeletalCorpseVisual && IsValid(CorpseMesh))
	{
		return GetNameSafe(CorpseMesh->GetSkeletalMeshAsset());
	}

	return IsValid(CorpseFallbackMesh)
		? GetNameSafe(CorpseFallbackMesh->GetStaticMesh())
		: TEXT("None");
}

FVector AJTSMoonAntCorpsePickupActor::GetCorpseVisualScale() const
{
	if (const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual())
	{
		return ActiveVisual->GetRelativeScale3D();
	}

	return FVector::OneVector;
}

bool AJTSMoonAntCorpsePickupActor::IsCorpseVisualHidden() const
{
	const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual();
	return !IsValid(ActiveVisual) || !ActiveVisual->IsVisible();
}

bool AJTSMoonAntCorpsePickupActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return bCorpseSettled && Super::CanInteract_Implementation(InteractingPawn);
}

void AJTSMoonAntCorpsePickupActor::BeginPlay()
{
	Super::BeginPlay();
	RealSurfacePlanet = GetRealSurfacePlanet();
	bUsingRealPlanetSurfaceForPop = RealSurfacePlanet.IsValid();
	if (bUsingRealPlanetSurfaceForPop)
	{
		if (UJTSMoonWrappedActorComponent* const MoonWrapped = GetMoonWrappedActorComponent(); IsValid(MoonWrapped))
		{
			MoonWrapped->Deactivate();
			MoonWrapped->SetComponentTickEnabled(false);
		}
	}

	if (!bInitializedFromMoonAnt)
	{
		DeathGroundLocation = GetActorLocation();
	}

	ConfigureCorpseVisual();
	RecalculateGroundSupport();
	ResolveInitialSettledGroundLocation();
	PlaceAtSettledGroundLocation();
	CorpsePopElapsed = 0.0f;
	const float SafeDurationMin = FMath::Max(0.05f, CorpsePopDurationMin);
	CorpsePopDuration = FMath::FRandRange(SafeDurationMin, FMath::Max(SafeDurationMin, CorpsePopDurationMax));
	bCorpseSettled = false;
	SetCorpseInteractionEnabled(false);
	UpdateCorpseVisualTransform(0.0f);
	SetActorTickEnabled(true);
}

void AJTSMoonAntCorpsePickupActor::Tick(float DeltaSeconds)
{
	// AJTSWorldPickupActor only ticks its generic ballistic drops. Corpse pop and Fake Moon visual
	// alignment are separate, visual-only work, so do not call the base implementation here.
	AActor::Tick(DeltaSeconds);

	if (!bCorpseSettled)
	{
		CorpsePopElapsed += FMath::Max(0.0f, DeltaSeconds);
		const float PopAlpha = CorpsePopDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(CorpsePopElapsed / CorpsePopDuration, 0.0f, 1.0f)
			: 1.0f;
		UpdateCorpseVisualTransform(PopAlpha);

		if (PopAlpha >= 1.0f)
		{
			ResolveFinalSettledGroundLocation();
			PlaceAtSettledGroundLocation();
			bCorpseSettled = true;
			SetCorpseInteractionEnabled(true);
			UpdateCorpseVisualTransform(1.0f);
		}
		return;
	}

	// Skeletal MoonAnt materials use the CPU Fake Moon bend, so the visual must follow the local player
	// even after the one-shot pop has settled. No gameplay position is changed here.
	UpdateCorpseVisualTransform(1.0f);
}

FVector AJTSMoonAntCorpsePickupActor::GetInteractionTargetWorldLocation() const
{
	if (const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual(); IsValid(ActiveVisual) && ActiveVisual->IsRegistered())
	{
		return ActiveVisual->Bounds.Origin;
	}

	return Super::GetInteractionTargetWorldLocation();
}

FVector AJTSMoonAntCorpsePickupActor::GetInteractionAnchorWorldLocation() const
{
	if (const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual(); IsValid(ActiveVisual) && ActiveVisual->IsRegistered())
	{
		if (bUsingRealPlanetSurfaceForPop)
		{
			const FVector CorpseSurfaceUp = GetCorpseSurfaceUp(ActiveVisual->Bounds.Origin);
			FJTSSurfaceVisualProjectionBounds VisualBounds;
			if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
				ActiveVisual,
				GetActorLocation(),
				CorpseSurfaceUp,
				VisualBounds))
			{
				return VisualBounds.HighestPoint + CorpseSurfaceUp * 18.0f;
			}
		}

		const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return ActiveVisual->Bounds.Origin + GetCorpseSurfaceUp(ActiveVisual->Bounds.Origin) * (PhysicalExtent.Z + 18.0f);
	}

	return Super::GetInteractionAnchorWorldLocation();
}

FVector AJTSMoonAntCorpsePickupActor::GetVisualBoundsExtent() const
{
	if (const UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual(); IsValid(ActiveVisual) && ActiveVisual->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		if (!PhysicalExtent.IsNearlyZero())
		{
			return PhysicalExtent;
		}
	}

	return Super::GetVisualBoundsExtent();
}

void AJTSMoonAntCorpsePickupActor::AdjustToGround(const FVector& GroundHitLocation)
{
	// This override is retained for shared pickup helpers. A corpse is always placed from its own
	// unbent, side-flipped bounds so generic pickup support calculations cannot pollute its ground state.
	SettledGroundLocation = GroundHitLocation;
	PlaceAtSettledGroundLocation();
	UpdateCorpseVisualTransform(bCorpseSettled ? 1.0f : 0.0f);
}

UMaterialInterface* AJTSMoonAntCorpsePickupActor::GetMoonBendMaterialForPickup() const
{
	// Do not replace copied MoonAnt materials. Skeletal and debug fallback visuals receive an equivalent
	// CPU bend, while the MoonAnt's original static fallback retains its existing WPO material.
	return nullptr;
}

UPrimitiveComponent* AJTSMoonAntCorpsePickupActor::GetActiveCorpseVisual() const
{
	return bUseSkeletalCorpseVisual
		? static_cast<UPrimitiveComponent*>(CorpseMesh.Get())
		: static_cast<UPrimitiveComponent*>(CorpseFallbackMesh.Get());
}

void AJTSMoonAntCorpsePickupActor::ConfigureCorpseVisual()
{
	if (UStaticMeshComponent* const BasePickupMesh = GetPickupMeshComponent())
	{
		BasePickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BasePickupMesh->SetHiddenInGame(true, true);
		BasePickupMesh->SetVisibility(false, true);
	}

	if (!IsValid(CorpseMesh) || !IsValid(CorpseFallbackMesh))
	{
		return;
	}

	CorpseMesh->SetSkeletalMesh(SourceSkeletalMeshAsset);
	bUseSkeletalCorpseVisual = SourceSkeletalMeshAsset != nullptr && CorpseMesh->GetSkeletalMeshAsset() != nullptr;
	bUseDebugFallbackVisual = !bUseSkeletalCorpseVisual && SourceFallbackMeshAsset == nullptr;
	bVisualUsesMaterialMoonBend = !bUseSkeletalCorpseVisual && !bUseDebugFallbackVisual;

	CorpseMesh->SetRelativeLocation(CorpseBaseRelativeLocation);
	CorpseMesh->SetRelativeRotation(CorpseBaseRelativeRotation);
	CorpseMesh->SetRelativeScale3D(SourceVisualScale);
	CorpseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpseMesh->SetSimulatePhysics(false);
	CorpseMesh->bPauseAnims = true;
	CorpseMesh->Stop();
	CorpseMesh->SetComponentTickEnabled(false);
	CorpseMesh->SetCastShadow(true);
	CorpseMesh->SetVisibility(bUseSkeletalCorpseVisual, true);
	CorpseMesh->SetHiddenInGame(!bUseSkeletalCorpseVisual, true);

	UStaticMesh* const FallbackAsset = bUseDebugFallbackVisual
		? DebugFallbackMeshAsset.Get()
		: SourceFallbackMeshAsset.Get();
	CorpseFallbackMesh->SetStaticMesh(FallbackAsset);
	CorpseFallbackMesh->SetRelativeLocation(CorpseBaseRelativeLocation);
	CorpseFallbackMesh->SetRelativeRotation(CorpseBaseRelativeRotation);
	CorpseFallbackMesh->SetRelativeScale3D(bUseDebugFallbackVisual ? FVector(0.28f, 0.18f, 0.13f) : SourceVisualScale);
	CorpseFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpseFallbackMesh->SetSimulatePhysics(false);
	CorpseFallbackMesh->SetCastShadow(true);
	const bool bHasFallbackVisual = !bUseSkeletalCorpseVisual && FallbackAsset != nullptr;
	CorpseFallbackMesh->SetVisibility(bHasFallbackVisual, true);
	CorpseFallbackMesh->SetHiddenInGame(!bHasFallbackVisual, true);

	UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual();
	if (!IsValid(ActiveVisual))
	{
		return;
	}

	if (!bUseDebugFallbackVisual)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SourceVisualMaterials.Num(); ++MaterialIndex)
		{
			if (UMaterialInterface* const SourceMaterial = SourceVisualMaterials[MaterialIndex])
			{
				ActiveVisual->SetMaterial(MaterialIndex, SourceMaterial);
			}
		}
	}
	else if (DebugFallbackMaterial != nullptr)
	{
		CorpseFallbackMesh->SetMaterial(0, DebugFallbackMaterial);
		if (UMaterialInstanceDynamic* const DebugMaterial = CorpseFallbackMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			const FLinearColor DebugColor(0.95f, 0.16f, 0.03f, 1.0f);
			DebugMaterial->SetVectorParameterValue(TEXT("Color"), DebugColor);
			DebugMaterial->SetVectorParameterValue(TEXT("BaseColor"), DebugColor);
			DebugMaterial->SetVectorParameterValue(TEXT("Tint"), DebugColor);
		}
	}

	ActiveVisual->UpdateBounds();
}

void AJTSMoonAntCorpsePickupActor::RecalculateGroundSupport()
{
	GroundSupportHeight = 1.0f;
	UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual();
	if (!IsValid(ActiveVisual) || !ActiveVisual->IsRegistered())
	{
		return;
	}

	// This executes before any pop or CPU bend location is applied. The side-flip rotation and the final
	// copied scale are already set, which makes these bounds the correct support shape for a lying MoonAnt.
	ActiveVisual->SetRelativeLocation(CorpseBaseRelativeLocation);
	ActiveVisual->SetRelativeRotation(CorpseBaseRelativeRotation);
	ActiveVisual->UpdateBounds();
	const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
	if (bUsingRealPlanetSurfaceForPop)
	{
		FJTSSurfaceVisualProjectionBounds VisualBounds;
		const FVector CorpseSurfaceUp = GetCorpseSurfaceUp(GetActorLocation());
		if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
			ActiveVisual,
			GetActorLocation(),
			CorpseSurfaceUp,
			VisualBounds))
		{
			GroundSupportHeight = VisualBounds.GetRootToLowestSupport()
				+ JTSSurfacePlacementBounds::DefaultSurfaceClearance;
		}
		UpdateInteractionCollider();
		return;
	}

	const float CenterOffsetZ = ActiveVisual->Bounds.Origin.Z - GetActorLocation().Z;
	GroundSupportHeight = FMath::Max(1.0f, PhysicalExtent.Z - CenterOffsetZ + 1.0f);
	UpdateInteractionCollider();
}

void AJTSMoonAntCorpsePickupActor::ResolveInitialSettledGroundLocation()
{
	SettledGroundLocation = DeathGroundLocation;
	DeathLogicalPosition = FVector2D(DeathGroundLocation.X, DeathGroundLocation.Y);
	SettledLogicalPosition = DeathLogicalPosition;
	bUsingMoonWrapForPop = false;
	if (AJTSPlanetAnchor* const Planet = bUsingRealPlanetSurfaceForPop ? GetRealSurfacePlanet() : nullptr)
	{
		FJTSPlanetSurfaceFrame SurfaceFrame;
		FJTSPlanetSurfaceHit SurfaceHit;
		const float SafeMinDistance = FMath::Max(0.0f, CorpsePopHorizontalDistanceMin);
		const float SafeMaxDistance = FMath::Max(SafeMinDistance, CorpsePopHorizontalDistanceMax);
		const float PopAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
		if (Planet->GetSurfaceFrameAt(DeathGroundLocation, GetActorForwardVector(), SurfaceFrame)
			&& Planet->ProjectPointToSurface(
				DeathGroundLocation + (SurfaceFrame.Forward * FMath::Cos(PopAngle)
					+ SurfaceFrame.Right * FMath::Sin(PopAngle)).GetSafeNormal()
					* FMath::FRandRange(SafeMinDistance, SafeMaxDistance),
				SurfaceHit))
		{
			SettledGroundLocation = SurfaceHit.ImpactPoint;
		}
		return;
	}

	UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	if (IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
	{
		bUsingMoonWrapForPop = true;
		DeathLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(DeathGroundLocation);
	}

	const float SafeMinDistance = FMath::Max(0.0f, CorpsePopHorizontalDistanceMin);
	const float SafeMaxDistance = FMath::Max(SafeMinDistance, CorpsePopHorizontalDistanceMax);
	const float PopAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const FVector2D PopDelta(FMath::Cos(PopAngle), FMath::Sin(PopAngle));
	const float PopDistance = FMath::FRandRange(SafeMinDistance, SafeMaxDistance);

	FVector CandidateGroundLocation = DeathGroundLocation;
	if (bUsingMoonWrapForPop)
	{
		SettledLogicalPosition = MoonWrap->CanonicalizePosition2D(DeathLogicalPosition + PopDelta * PopDistance);
		const FVector2D CandidatePhysicalPosition = MoonWrap->GetNearestPhysicalImage(
			FVector2D(DeathGroundLocation.X, DeathGroundLocation.Y),
			SettledLogicalPosition);
		CandidateGroundLocation.X = CandidatePhysicalPosition.X;
		CandidateGroundLocation.Y = CandidatePhysicalPosition.Y;
	}
	else
	{
		CandidateGroundLocation.X += PopDelta.X * PopDistance;
		CandidateGroundLocation.Y += PopDelta.Y * PopDistance;
		SettledLogicalPosition = FVector2D(CandidateGroundLocation.X, CandidateGroundLocation.Y);
	}

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		FVector ResolvedGroundLocation;
		if (SurfaceController->OwnsSurfaceActor(this)
			&& SurfaceController->ResolveMoonGroundLocation(CandidateGroundLocation, ResolvedGroundLocation, this))
		{
			SettledGroundLocation = ResolvedGroundLocation;
			return;
		}
	}

	SettledGroundLocation = CandidateGroundLocation;
}

void AJTSMoonAntCorpsePickupActor::ResolveFinalSettledGroundLocation()
{
	if (AJTSPlanetAnchor* const Planet = bUsingRealPlanetSurfaceForPop ? GetRealSurfacePlanet() : nullptr)
	{
		FJTSPlanetSurfaceHit SurfaceHit;
		if (Planet->ProjectPointToSurface(SettledGroundLocation, SurfaceHit))
		{
			SettledGroundLocation = SurfaceHit.ImpactPoint;
		}
		return;
	}

	UWorld* const World = GetWorld();
	const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	if (World == nullptr || !IsValid(SurfaceController) || !SurfaceController->OwnsSurfaceActor(this))
	{
		return;
	}

	FVector CandidateGroundLocation = SettledGroundLocation;
	if (bUsingMoonWrapForPop)
	{
		if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>(); IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
		{
			const FVector2D CandidatePhysicalPosition = MoonWrap->GetNearestPhysicalImage(
				FVector2D(GetActorLocation().X, GetActorLocation().Y),
				SettledLogicalPosition);
			CandidateGroundLocation.X = CandidatePhysicalPosition.X;
			CandidateGroundLocation.Y = CandidatePhysicalPosition.Y;
		}
	}

	FVector ResolvedGroundLocation;
	if (SurfaceController->ResolveMoonGroundLocation(CandidateGroundLocation, ResolvedGroundLocation, this))
	{
		SettledGroundLocation = ResolvedGroundLocation;
	}
}

void AJTSMoonAntCorpsePickupActor::PlaceAtSettledGroundLocation()
{
	if (AJTSPlanetAnchor* const Planet = bUsingRealPlanetSurfaceForPop ? GetRealSurfacePlanet() : nullptr)
	{
		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (Planet->GetSurfaceFrameAt(SettledGroundLocation, GetActorForwardVector(), SurfaceFrame))
		{
			PlaceOnPlanetSurface(Planet, SettledGroundLocation, SurfaceFrame.Forward);
			// PlaceOnPlanetSurface aligns the shared pickup root. Recalculate after that final rotation so
			// this lying copied visual, including its local offset and side flip, supplies the real support.
			RecalculateGroundSupport();
			SetActorLocation(
				SurfaceFrame.Location + SurfaceFrame.Up * GroundSupportHeight,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		return;
	}

	SetActorLocation(
		FVector(
			SettledGroundLocation.X,
			SettledGroundLocation.Y,
			SettledGroundLocation.Z + GroundSupportHeight),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (UJTSMoonWrappedActorComponent* const MoonWrapped = GetMoonWrappedActorComponent(); IsValid(MoonWrapped))
	{
		if (bUsingMoonWrapForPop)
		{
			MoonWrapped->SetLogicalPosition2D(SettledLogicalPosition);
			MoonWrapped->RefreshPhysicalImage();
		}
		else
		{
			UpdateMoonWrappedLogicalPosition();
		}
	}
}

void AJTSMoonAntCorpsePickupActor::UpdateCorpseVisualTransform(float PopAlpha)
{
	UPrimitiveComponent* const ActiveVisual = GetActiveCorpseVisual();
	if (!IsValid(ActiveVisual))
	{
		return;
	}

	const float ClampedAlpha = FMath::Clamp(PopAlpha, 0.0f, 1.0f);
	if (AJTSPlanetAnchor* const Planet = bUsingRealPlanetSurfaceForPop ? GetRealSurfacePlanet() : nullptr)
	{
		FJTSPlanetSurfaceHit SurfaceHit;
		const FVector InterpolatedSurfacePoint = FMath::Lerp(
			DeathGroundLocation,
			SettledGroundLocation,
			ClampedAlpha);
		const FVector SurfaceLocation = Planet->ProjectPointToSurface(InterpolatedSurfacePoint, SurfaceHit)
			? SurfaceHit.ImpactPoint
			: SettledGroundLocation;
		const FVector CorpseSurfaceUp = GetCorpseSurfaceUp(SurfaceLocation);
		const float ArcOffset = 4.0f * FMath::Max(0.0f, CorpsePopHeight)
			* ClampedAlpha * (1.0f - ClampedAlpha);
		const FVector VisualWorldLocation = SurfaceLocation + CorpseSurfaceUp * (GroundSupportHeight + ArcOffset);
		FVector VisualRelativeWorldOffset = VisualWorldLocation - GetActorLocation();
		if (const USceneComponent* const RootSceneComponent = GetRootComponent(); IsValid(RootSceneComponent))
		{
			VisualRelativeWorldOffset = RootSceneComponent->GetComponentTransform().InverseTransformVector(VisualRelativeWorldOffset);
		}

		ActiveVisual->SetRelativeLocation(CorpseBaseRelativeLocation + VisualRelativeWorldOffset);
		ActiveVisual->SetRelativeRotation(CorpseBaseRelativeRotation);
		return;
	}

	if (UJTSMoonWrappedActorComponent* const MoonWrapped = GetMoonWrappedActorComponent(); IsValid(MoonWrapped) && MoonWrapped->IsMoonWrappingEnabled())
	{
		MoonWrapped->RefreshPhysicalImage();
	}
	const FVector RootLocation = GetActorLocation();
	FVector VisualPhysicalLocation = RootLocation + GetPopVisualWorldOffset(ClampedAlpha);
	FVector MoonBendWorldOffset = FVector::ZeroVector;

	UWorld* const World = GetWorld();
	const APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (World != nullptr)
	{
		const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
		if (bUsingMoonWrapForPop && IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
		{
			const FVector2D StartDelta = MoonWrap->ShortestWrappedDelta2D(SettledLogicalPosition, DeathLogicalPosition);
			const FVector2D VisualLogicalPosition = MoonWrap->CanonicalizePosition2D(
				SettledLogicalPosition + StartDelta * (1.0f - ClampedAlpha));
			const FVector2D ImageAnchor = IsValid(LocalPawn)
				? FVector2D(LocalPawn->GetActorLocation().X, LocalPawn->GetActorLocation().Y)
				: FVector2D(RootLocation.X, RootLocation.Y);
			const FVector2D VisualPhysicalPosition = MoonWrap->GetNearestPhysicalImage(ImageAnchor, VisualLogicalPosition);
			VisualPhysicalLocation.X = VisualPhysicalPosition.X;
			VisualPhysicalLocation.Y = VisualPhysicalPosition.Y;
		}

		if (!bVisualUsesMaterialMoonBend && IsValid(LocalPawn) && IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
		{
			MoonBendWorldOffset = MoonWrap->GetMoonVisualWorldPosition(VisualPhysicalLocation, LocalPawn->GetActorLocation())
				- VisualPhysicalLocation;
		}
	}

	FVector VisualRelativeWorldOffset = VisualPhysicalLocation - RootLocation + MoonBendWorldOffset;
	if (const USceneComponent* const RootSceneComponent = GetRootComponent(); IsValid(RootSceneComponent))
	{
		VisualRelativeWorldOffset = RootSceneComponent->GetComponentTransform().InverseTransformVector(VisualRelativeWorldOffset);
	}

	ActiveVisual->SetRelativeLocation(CorpseBaseRelativeLocation + VisualRelativeWorldOffset);
	ActiveVisual->SetRelativeRotation(CorpseBaseRelativeRotation);
}

FVector AJTSMoonAntCorpsePickupActor::GetPopVisualWorldOffset(float PopAlpha) const
{
	const float ClampedAlpha = FMath::Clamp(PopAlpha, 0.0f, 1.0f);
	FVector PopOffset = FVector::ZeroVector;
	if (bUsingMoonWrapForPop)
	{
		if (const UWorld* const World = GetWorld())
		{
			if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>(); IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
			{
				const FVector2D StartDelta = MoonWrap->ShortestWrappedDelta2D(SettledLogicalPosition, DeathLogicalPosition);
				PopOffset.X = StartDelta.X * (1.0f - ClampedAlpha);
				PopOffset.Y = StartDelta.Y * (1.0f - ClampedAlpha);
			}
		}
	}
	else
	{
		PopOffset.X = (DeathGroundLocation.X - SettledGroundLocation.X) * (1.0f - ClampedAlpha);
		PopOffset.Y = (DeathGroundLocation.Y - SettledGroundLocation.Y) * (1.0f - ClampedAlpha);
	}

	const float LinearGroundOffset = (DeathGroundLocation.Z - SettledGroundLocation.Z) * (1.0f - ClampedAlpha);
	const float ArcOffset = 4.0f * FMath::Max(0.0f, CorpsePopHeight) * ClampedAlpha * (1.0f - ClampedAlpha);
	PopOffset.Z = LinearGroundOffset + ArcOffset;
	return PopOffset;
}

AJTSPlanetAnchor* AJTSMoonAntCorpsePickupActor::GetRealSurfacePlanet() const
{
	if (RealSurfacePlanet.IsValid())
	{
		return RealSurfacePlanet.Get();
	}

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController)
		&& SurfaceController->OwnsSurfaceActor(this)
		&& SurfaceController->IsUsingRealPlanetSurfaceGameplay())
	{
		return SurfaceController->GetOwningPlanet();
	}

	return nullptr;
}

FVector AJTSMoonAntCorpsePickupActor::GetCorpseSurfaceUp(const FVector& SurfaceLocation) const
{
	if (AJTSPlanetAnchor* const Planet = GetRealSurfacePlanet())
	{
		FVector SurfaceNormal;
		if (Planet->GetSurfaceNormalAt(SurfaceLocation, SurfaceNormal) && !SurfaceNormal.IsNearlyZero())
		{
			return SurfaceNormal.GetSafeNormal();
		}

		return Planet->GetRadialUpVector(SurfaceLocation);
	}

	return FVector::UpVector;
}

void AJTSMoonAntCorpsePickupActor::UpdateInteractionCollider()
{
	if (!IsValid(PickupInteractionCollider))
	{
		return;
	}

	const FVector BoundsExtent = GetVisualBoundsExtent();
	const float InteractionRadius = FMath::Max(18.0f, FVector2D(BoundsExtent.X, BoundsExtent.Y).Size() + 10.0f);
	PickupInteractionCollider->SetSphereRadius(InteractionRadius);
}

void AJTSMoonAntCorpsePickupActor::SetCorpseInteractionEnabled(bool bEnabled)
{
	if (IsValid(PickupInteractionCollider))
	{
		PickupInteractionCollider->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}
