#include "space/World/JTSMoonAntActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/UI/JTSHealthBarWidget.h"
#include "space/World/JTSMoonAntCorpsePickupActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonAntNestActor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* GetMoonAntStateDebugName(EJTSMoonAntState State)
	{
		switch (State)
		{
		case EJTSMoonAntState::Emerging:
			return TEXT("Emerging");
		case EJTSMoonAntState::Roaming:
			return TEXT("Roaming");
		case EJTSMoonAntState::ReactingToHit:
			return TEXT("ReactingToHit");
		case EJTSMoonAntState::Fleeing:
			return TEXT("Fleeing");
		case EJTSMoonAntState::Burrowing:
			return TEXT("Burrowing");
		default:
			return TEXT("Unknown");
		}
	}
}

AJTSMoonAntActor::AJTSMoonAntActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetActorEnableCollision(true);
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HealthComponent = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("HealthComponent"));

	MoonAntMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MoonAntMesh"));
	MoonAntMesh->SetupAttachment(SceneRoot);
	MoonAntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonAntMesh->SetCollisionObjectType(ECC_WorldDynamic);
	MoonAntMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MoonAntMesh->SetGenerateOverlapEvents(false);
	MoonAntMesh->SetCanEverAffectNavigation(false);

	MoonAntFallbackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoonAntFallbackMesh"));
	MoonAntFallbackMesh->SetupAttachment(SceneRoot);
	MoonAntFallbackMesh->SetRelativeScale3D(MoonAntFallbackBaseMeshScale);
	MoonAntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonAntFallbackMesh->SetCollisionObjectType(ECC_WorldDynamic);
	MoonAntFallbackMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MoonAntFallbackMesh->SetGenerateOverlapEvents(false);
	MoonAntFallbackMesh->SetCanEverAffectNavigation(false);

	MoonAntHitCollider = CreateDefaultSubobject<USphereComponent>(TEXT("MoonAntHitCollider"));
	MoonAntHitCollider->SetupAttachment(SceneRoot);
	MoonAntHitCollider->InitSphereRadius(25.0f);
	MoonAntHitCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonAntHitCollider->SetCollisionObjectType(ECC_WorldDynamic);
	MoonAntHitCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	MoonAntHitCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MoonAntHitCollider->SetGenerateOverlapEvents(false);
	MoonAntHitCollider->SetCanEverAffectNavigation(false);

	MoonAntHealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MoonAntHealthBarComponent"));
	MoonAntHealthBarComponent->SetupAttachment(SceneRoot);
	MoonAntHealthBarComponent->SetWidgetClass(UJTSHealthBarWidget::StaticClass());
	MoonAntHealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	MoonAntHealthBarComponent->SetDrawAtDesiredSize(false);
	MoonAntHealthBarComponent->SetDrawSize(FVector2D(110.0f, 14.0f));
	MoonAntHealthBarComponent->SetPivot(FVector2D(0.5f, 1.0f));
	MoonAntHealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonAntHealthBarComponent->SetGenerateOverlapEvents(false);
	MoonAntHealthBarComponent->SetCanEverAffectNavigation(false);
	MoonAntHealthBarComponent->SetAbsolute(false, false, true);
	MoonAntHealthBarComponent->SetVisibility(false);
	MoonAntHealthBarComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		MoonAntFallbackMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		MoonAntFallbackMesh->SetMaterial(0, BasicMaterialAsset.Object);
	}
}

void AJTSMoonAntActor::InitializeMoonAnt(AJTSMoonAntNestActor* InOriginNest, const FVector& InGroundLocation)
{
	if (!HasAuthority())
	{
		return;
	}
	OriginNest = InOriginNest;
	GroundLocation = InGroundLocation;
	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController) && SurfaceController->IsUsingRealPlanetSurfaceGameplay())
	{
		SurfacePlanet = SurfaceController->GetOwningPlanet();
		bUsesRealPlanetSurface = IsValid(SurfacePlanet);
		if (bUsesRealPlanetSurface)
		{
			SurfaceUp = SurfacePlanet->GetRadialUpVector(InGroundLocation);
		}
	}
	bInitialized = IsValid(InOriginNest);
}

EJTSMoonAntState AJTSMoonAntActor::GetMoonAntState() const
{
	return MoonAntState;
}

UJTSHealthComponent* AJTSMoonAntActor::GetHealthComponent() const
{
	return HealthComponent.Get();
}

bool AJTSMoonAntActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn)
		&& IsValid(HealthComponent)
		&& !HealthComponent->IsDead()
		&& MoonAntState != EJTSMoonAntState::Burrowing
		&& !IsPendingKillPending();
}

void AJTSMoonAntActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	if (!HasAuthority() || !CanReceiveMeleeHit_Implementation(AttackingPawn))
	{
		return;
	}

	// Normal attacks already use UE damage directly. Legacy interface callers still take their
	// damage value from the attacking pawn's melee/weapon component, never from this target.
	const UJTSMeleeComponent* const MeleeComponent = AttackingPawn->FindComponentByClass<UJTSMeleeComponent>();
	if (!IsValid(MeleeComponent))
	{
		return;
	}

	const float LegacyDamage = MeleeComponent->GetDamageForAttackType(AttackType);
	if (LegacyDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	UGameplayStatics::ApplyDamage(
		this,
		LegacyDamage,
		AttackingPawn->GetController(),
		AttackingPawn,
		UDamageType::StaticClass());
}

FText AJTSMoonAntActor::GetMeleeTargetDisplayName_Implementation() const
{
	return FText::FromString(TEXT("MOON ANT"));
}

FText AJTSMoonAntActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return CanReceiveMeleeHit_Implementation(AttackingPawn)
		? FText::FromString(TEXT("[LMB] ATTACK"))
		: FText::GetEmpty();
}

FVector AJTSMoonAntActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent(); IsValid(ActiveVisual) && ActiveVisual->IsRegistered())
	{
		if (IsUsingRealPlanetSurface())
		{
			FJTSSurfaceVisualProjectionBounds VisualBounds;
			if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
				ActiveVisual,
				GetActorLocation(),
				SurfaceUp,
				VisualBounds))
			{
				return VisualBounds.HighestPoint + SurfaceUp * 14.0f;
			}
		}

		const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return ActiveVisual->Bounds.Origin + SurfaceUp * (PhysicalExtent.Z + 14.0f);
	}

	return GetActorLocation() + SurfaceUp * (GroundSupportHeight + 14.0f);
}

void AJTSMoonAntActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisualMode();
}

void AJTSMoonAntActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);
	bDeathSequenceStarted = false;
	bHasDroppedCorpse = false;

	// This repeats the construction-time decision so Blueprint component defaults are honored in every runtime spawn path.
	RefreshVisualMode();

	if (!HasAuthority())
	{
		ConfigureMoonAntVisuals();
		ConfigureMoonAntHealthBar();
		UpdateMoonAntVisualTransform();
		UpdateMoonAntHealthBarTransform();
		return;
	}

	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (!bInitialized || !OriginNest.IsValid() || MoonGameMode == nullptr || !IsValid(HealthComponent))
	{
		Destroy();
		return;
	}

	HealthComponent->SetMaxHealth(MoonAntMaxHealth, true);
	HealthComponent->OnDamaged.AddDynamic(this, &AJTSMoonAntActor::HandleHealthDamaged);
	HealthComponent->OnDeath.AddDynamic(this, &AJTSMoonAntActor::HandleHealthDeath);
	ConfigureMoonAntVisuals();
	ConfigureMoonAntHealthBar();
	HideMoonAntHealthBar();
	PlaceOnGround(GroundLocation);
	SetMoonAntState(EJTSMoonAntState::Emerging);
	UpdateMoonAntVisualTransform();
	UpdateMoonAntHealthBarTransform();

	const USkeletalMesh* const SkeletalMesh = IsValid(MoonAntMesh) ? MoonAntMesh->GetSkeletalMeshAsset() : nullptr;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonAnt spawned: Class=%s Mesh=%s Nest=%s"),
		*GetNameSafe(GetClass()),
		*GetNameSafe(SkeletalMesh),
		*GetNameSafe(OriginNest.Get()));
	if (GetClass() == AJTSMoonAntActor::StaticClass())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("MoonAnt is using native fallback class; MoonAntActorClass is not configured. Class=%s Mesh=%s Nest=%s"),
			*GetNameSafe(GetClass()),
			*GetNameSafe(SkeletalMesh),
			*GetNameSafe(OriginNest.Get()));
	}
}

void AJTSMoonAntActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDamaged.RemoveDynamic(this, &AJTSMoonAntActor::HandleHealthDamaged);
		HealthComponent->OnDeath.RemoveDynamic(this, &AJTSMoonAntActor::HandleHealthDeath);
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoonAntHealthBarHideTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AJTSMoonAntActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		UpdateMoonAntVisualTransform();
		UpdateMoonAntHealthBarTransform();
		return;
	}

	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (MoonGameMode == nullptr)
	{
		Destroy();
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	StateElapsed += SafeDeltaSeconds;

	switch (MoonAntState)
	{
	case EJTSMoonAntState::Emerging:
	{
		const float Duration = MoonGameMode->GetMoonAntEmergingDuration();
		const float Progress = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		const float SmoothedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, Progress, 2.0f);
		SetVisualBurrowOffset(FMath::Lerp(-BurrowDepth, 0.0f, SmoothedProgress));
		if (Progress >= 1.0f)
		{
			SetVisualBurrowOffset(0.0f);
			SetMeleeHitCollisionEnabled(true);
			SurfaceElapsed = 0.0f;
			SurfaceDuration = FMath::FRandRange(
				MoonGameMode->GetMoonAntSurfaceDurationMin(),
				MoonGameMode->GetMoonAntSurfaceDurationMax());
			SetMoonAntState(EJTSMoonAntState::Roaming);
		}
		break;
	}

	case EJTSMoonAntState::Roaming:
	{
		SurfaceElapsed += SafeDeltaSeconds;
		if (SurfaceElapsed >= SurfaceDuration)
		{
			BeginBurrowing();
			break;
		}

		AJTSPlanetAnchor* const Planet = GetSurfacePlanet();
		if (!IsValid(Planet))
		{
			BeginBurrowing();
			break;
		}

		RoamRetargetElapsed += SafeDeltaSeconds;
		const float HomeDistance = GetDistanceToOriginNest();
		FVector ToRoamTarget = GetSurfaceTangentTo(RoamTargetWorldLocation);
		if (HomeDistance > MoonGameMode->GetMoonAntMaxHomeRadius())
		{
			FVector NestLocation;
			const FVector ToNest = GetOriginNestLocation(NestLocation)
				? GetSurfaceTangentTo(NestLocation)
				: FVector::ZeroVector;
			const float TargetAlignment = FVector::DotProduct(
				ToRoamTarget.GetSafeNormal(),
				ToNest.GetSafeNormal());
			if (ToRoamTarget.IsNearlyZero() || TargetAlignment < 0.25f)
			{
				ChooseRoamTarget(true);
				ToRoamTarget = GetSurfaceTangentTo(RoamTargetWorldLocation);
			}
		}

		const float TargetDistance = Planet->ApproximateSurfaceArcDistance(GetActorLocation(), RoamTargetWorldLocation);
		if (TargetDistance <= 30.0f || RoamRetargetElapsed >= RoamRetargetInterval)
		{
			ChooseRoamTarget(HomeDistance > MoonGameMode->GetMoonAntMaxHomeRadius());
			ToRoamTarget = GetSurfaceTangentTo(RoamTargetWorldLocation);
		}

		MoveAlongGround(ToRoamTarget, MoonGameMode->GetMoonAntRoamSpeed(), SafeDeltaSeconds);
		break;
	}

	case EJTSMoonAntState::ReactingToHit:
		if (StateElapsed >= MoonGameMode->GetMoonAntHitReactionDuration())
		{
			BeginFleeing();
		}
		break;

	case EJTSMoonAntState::Fleeing:
	{
		const FVector PreviousLocation = GetActorLocation();
		if (MoveAlongGround(FleeDirection, MoonGameMode->GetMoonAntFleeSpeed(), SafeDeltaSeconds))
		{
			if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
			{
				FleeDistanceTravelled += Planet->ApproximateSurfaceArcDistance(PreviousLocation, GetActorLocation());
			}
		}
		if (StateElapsed >= FleeDuration || FleeDistanceTravelled >= MoonGameMode->GetMoonAntMaxFleeDistance())
		{
			BeginBurrowing();
		}
		break;
	}

	case EJTSMoonAntState::Burrowing:
	{
		const float Duration = MoonGameMode->GetMoonAntBurrowDuration();
		const float Progress = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		const float SmoothedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, Progress, 2.0f);
		SetVisualBurrowOffset(FMath::Lerp(0.0f, -BurrowDepth, SmoothedProgress));
		if (Progress >= 1.0f)
		{
			Destroy();
		}
		break;
	}

	default:
		break;
	}

	UpdateMoonAntVisualTransform();
	UpdateMoonAntHealthBarTransform();
}

const IJTSMoonSurfaceGameplaySettings* AJTSMoonAntActor::GetMoonGameMode() const
{
	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return Controller->OwnsSurfaceActor(this) ? Controller->GetMoonSettings() : nullptr;
	}

	return nullptr;
}

void AJTSMoonAntActor::RefreshVisualMode()
{
	bUsingSkeletalMoonAntMesh = IsValid(MoonAntMesh) && MoonAntMesh->GetSkeletalMeshAsset() != nullptr;
	if (bUsingSkeletalMoonAntMesh)
	{
		MoonAntFallbackMesh->SetVisibility(false, true);
		MoonAntFallbackMesh->SetHiddenInGame(true, true);
		MoonAntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MoonAntMesh->SetVisibility(true, true);
		MoonAntMesh->SetHiddenInGame(false, true);
		MoonAntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		MoonAntMesh->SetVisibility(false, true);
		MoonAntMesh->SetHiddenInGame(true, true);
		MoonAntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MoonAntFallbackMesh->SetVisibility(true, true);
		MoonAntFallbackMesh->SetHiddenInGame(false, true);
		MoonAntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AJTSMoonAntActor::ConfigureMoonAntVisuals()
{
	RefreshVisualMode();
	if (bMoonAntMeshBaseTransformCaptured)
	{
		return;
	}

	// The mesh is still in its Blueprint-authored, unbent transform here. Do not let a visual offset
	// affect the bounds/support calculation or the immutable base transform captured below.
	bMoonAntVisualTransformReady = false;
	BurrowVisualOffset = 0.0f;

	const float SafeVariationMin = FMath::Max(0.1f, MoonAntScaleVariationMin);
	const float SafeVariationMax = FMath::Max(SafeVariationMin, MoonAntScaleVariationMax);
	const float DesiredBodyLength = FMath::Max(1.0f, MoonAntTargetBodyLength) * FMath::FRandRange(SafeVariationMin, SafeVariationMax);

	MoonAntMeshUniformScale = 1.0f;
	if (bUsingSkeletalMoonAntMesh)
	{
		const auto MeshBounds = MoonAntMesh->GetSkeletalMeshAsset()->GetBounds();
		const float MaxHorizontalBodySize = FMath::Max(
			FMath::Abs(MeshBounds.BoxExtent.X),
			FMath::Abs(MeshBounds.BoxExtent.Y)) * 2.0f;
		if (MaxHorizontalBodySize > KINDA_SMALL_NUMBER)
		{
			MoonAntMeshUniformScale = DesiredBodyLength / MaxHorizontalBodySize;
		}
		MoonAntMesh->SetRelativeScale3D(FVector(MoonAntMeshUniformScale));
	}
	else
	{
		const float FallbackLengthScale = DesiredBodyLength / 100.0f;
		MoonAntFallbackBaseMeshScale = FVector(
			FallbackLengthScale,
			FallbackLengthScale * 0.62f,
			FallbackLengthScale * 0.30f);
		MoonAntFallbackMesh->SetRelativeScale3D(MoonAntFallbackBaseMeshScale);
		MoonAntFallbackMaterial = MoonAntFallbackMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent())
	{
		ActiveVisual->UpdateBounds();
	}
	RecalculateGroundMetrics();
	CaptureBaseVisualTransforms();
	bMoonAntVisualTransformReady = bMoonAntMeshBaseTransformCaptured;

	if (IsValid(MoonAntHitCollider))
	{
		MoonAntHitCollider->SetSphereRadius(FMath::Max(12.0f, DesiredBodyLength * 0.42f));
	}
	UpdateFallbackMaterial();
}

void AJTSMoonAntActor::CaptureBaseVisualTransforms()
{
	if (bMoonAntMeshBaseTransformCaptured)
	{
		return;
	}

	// This is intentionally the only base-transform capture. It happens after Blueprint defaults and
	// final visual scale are ready, but before burrow offsets are applied.
	MoonAntMeshBaseRelativeLocation = IsValid(MoonAntMesh) ? MoonAntMesh->GetRelativeLocation() : FVector::ZeroVector;
	MoonAntMeshBaseRelativeRotation = IsValid(MoonAntMesh) ? MoonAntMesh->GetRelativeRotation() : FRotator::ZeroRotator;
	MoonAntFallbackBaseRelativeLocation = IsValid(MoonAntFallbackMesh) ? MoonAntFallbackMesh->GetRelativeLocation() : FVector::ZeroVector;
	bMoonAntMeshBaseTransformCaptured = true;
}

void AJTSMoonAntActor::RecalculateGroundMetrics()
{
	GroundSupportHeight = 12.0f;
	BurrowDepth = 30.0f;

	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	if (!IsValid(ActiveVisual))
	{
		return;
	}

	ActiveVisual->UpdateBounds();
	if (!ActiveVisual->IsRegistered())
	{
		return;
	}

	const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
	if (IsUsingRealPlanetSurface())
	{
		FJTSSurfaceVisualProjectionBounds VisualBounds;
		if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
			ActiveVisual,
			GetActorLocation(),
			SurfaceUp,
			VisualBounds))
		{
			GroundSupportHeight = VisualBounds.GetRootToLowestSupport()
				+ JTSSurfacePlacementBounds::DefaultSurfaceClearance;
			// A fully burrowed visual must move its actual SurfaceUp thickness below the mesh surface.
			// This is intentionally independent of the actor root offset and any long tangent body axis.
			BurrowDepth = FMath::Max(
				4.0f,
				VisualBounds.GetSurfaceThickness() + JTSSurfacePlacementBounds::DefaultSurfaceClearance);
		}
		return;
	}

	GroundSupportHeight = FMath::Max(1.0f, PhysicalExtent.Z);
	BurrowDepth = FMath::Max(4.0f, PhysicalExtent.Z * 2.0f + 2.0f);
}

void AJTSMoonAntActor::ChooseRoamTarget(bool bForceNearNest)
{
	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	FVector NestLocation;
	AJTSPlanetAnchor* const Planet = GetSurfacePlanet();
	if (!IsValid(Planet) || MoonGameMode == nullptr || !GetOriginNestLocation(NestLocation))
	{
		RoamTargetWorldLocation = GetActorLocation();
		RoamRetargetElapsed = 0.0f;
		RoamRetargetInterval = 0.5f;
		return;
	}

	const float RoamRadius = MoonGameMode->GetMoonAntRoamRadius();
	float MinDistance = FMath::Min(40.0f, RoamRadius);
	float MaxDistance = FMath::Clamp(170.0f, MinDistance, RoamRadius);
	if (!bForceNearNest)
	{
		const float BandSelection = FMath::FRand();
		if (BandSelection >= 0.60f && BandSelection < 0.90f)
		{
			MinDistance = FMath::Min(140.0f, RoamRadius);
			MaxDistance = FMath::Clamp(300.0f, MinDistance, RoamRadius);
		}
		else if (BandSelection >= 0.90f)
		{
			MinDistance = FMath::Min(270.0f, RoamRadius);
			MaxDistance = RoamRadius;
		}
	}

	const float BaseDistance = FMath::FRandRange(MinDistance, MaxDistance);
	const float RadialJitter = (MaxDistance - MinDistance) * 0.08f;
	const float Distance = FMath::Clamp(
		BaseDistance + FMath::FRandRange(-RadialJitter, RadialJitter),
		MinDistance,
		MaxDistance);
	const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
	FJTSPlanetSurfaceFrame SurfaceFrame;
	FJTSPlanetSurfaceHit SurfaceHit;
	if (Planet->GetSurfaceFrameAt(NestLocation, FVector::ForwardVector, SurfaceFrame)
		&& Planet->ProjectPointToSurface(
			NestLocation + (SurfaceFrame.Forward * FMath::Cos(Angle)
				+ SurfaceFrame.Right * FMath::Sin(Angle)).GetSafeNormal() * Distance,
			SurfaceHit))
	{
		RoamTargetWorldLocation = SurfaceHit.ImpactPoint;
	}
	else
	{
		RoamTargetWorldLocation = NestLocation;
	}

	RoamRetargetElapsed = 0.0f;
	RoamRetargetInterval = FMath::FRandRange(
		MoonGameMode->GetMoonAntRoamRetargetIntervalMin(),
		MoonGameMode->GetMoonAntRoamRetargetIntervalMax());
}

bool AJTSMoonAntActor::GetOriginNestLocation(FVector& OutNestLocation) const
{
	if (!OriginNest.IsValid())
	{
		return false;
	}

	OutNestLocation = OriginNest->GetActorLocation();
	return true;
}

float AJTSMoonAntActor::GetDistanceToOriginNest() const
{
	FVector NestLocation;
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		return GetOriginNestLocation(NestLocation)
			? Planet->ApproximateSurfaceArcDistance(GetActorLocation(), NestLocation)
			: TNumericLimits<float>::Max();
	}

	return TNumericLimits<float>::Max();
}

AJTSPlanetAnchor* AJTSMoonAntActor::GetSurfacePlanet() const
{
	if (IsValid(SurfacePlanet))
	{
		return SurfacePlanet.Get();
	}

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController) && SurfaceController->IsUsingRealPlanetSurfaceGameplay())
	{
		return SurfaceController->GetOwningPlanet();
	}

	return nullptr;
}

bool AJTSMoonAntActor::IsUsingRealPlanetSurface() const
{
	return bUsesRealPlanetSurface && IsValid(GetSurfacePlanet());
}

FVector AJTSMoonAntActor::GetSurfaceTangentTo(const FVector& TargetLocation) const
{
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		return Planet->ProjectDirectionToSurfaceTangent(TargetLocation - GetActorLocation(), GetActorLocation());
	}

	return FVector::ZeroVector;
}

bool AJTSMoonAntActor::MoveAlongGround(const FVector& Direction, float Speed, float DeltaSeconds)
{
	AJTSPlanetAnchor* const Planet = GetSurfacePlanet();
	if (!IsValid(Planet))
	{
		return false;
	}

	const FVector SurfaceDirection = Planet->ProjectDirectionToSurfaceTangent(Direction, GetActorLocation());
	if (SurfaceDirection.IsNearlyZero())
	{
		return false;
	}

	FJTSPlanetSurfaceHit SurfaceHit;
	const FVector CandidateLocation = GetActorLocation()
		+ SurfaceDirection * FMath::Max(0.0f, Speed) * FMath::Max(0.0f, DeltaSeconds);
	if (!Planet->ProjectPointToSurface(CandidateLocation, SurfaceHit))
	{
		return false;
	}

	PlaceOnGround(SurfaceHit.ImpactPoint);
	RotateTowardsDirection(SurfaceDirection, DeltaSeconds);
	return true;
}

void AJTSMoonAntActor::RotateTowardsDirection(const FVector& Direction, float DeltaSeconds)
{
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		const FVector SurfaceDirection = Planet->ProjectDirectionToSurfaceTangent(Direction, GetActorLocation());
		if (SurfaceDirection.IsNearlyZero())
		{
			return;
		}

		const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
		if (MoonGameMode == nullptr)
		{
			return;
		}

		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (Planet->GetSurfaceFrameAt(GetActorLocation(), SurfaceDirection, SurfaceFrame))
		{
			SetActorRotation(
				FMath::RInterpConstantTo(
					GetActorRotation(),
					SurfaceFrame.Transform.Rotator(),
					FMath::Max(0.0f, DeltaSeconds),
					MoonGameMode->GetMoonAntTurnSpeed()),
				ETeleportType::TeleportPhysics);
		}
	}
}

void AJTSMoonAntActor::PlaceOnGround(const FVector& NewGroundLocation)
{
	GroundLocation = NewGroundLocation;
	if (AJTSPlanetAnchor* const Planet = GetSurfacePlanet())
	{
		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (Planet->GetSurfaceFrameAt(NewGroundLocation, GetActorForwardVector(), SurfaceFrame))
		{
			SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
			SetActorRotation(SurfaceFrame.Transform.Rotator(), ETeleportType::TeleportPhysics);
			RecalculateGroundMetrics();
			SetActorLocation(
				SurfaceFrame.Location + SurfaceUp * GroundSupportHeight,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			return;
		}
	}
}

UPrimitiveComponent* AJTSMoonAntActor::GetActiveVisualComponent() const
{
	return bUsingSkeletalMoonAntMesh
		? static_cast<UPrimitiveComponent*>(MoonAntMesh.Get())
		: static_cast<UPrimitiveComponent*>(MoonAntFallbackMesh.Get());
}

void AJTSMoonAntActor::SetMeleeHitCollisionEnabled(bool bEnabled)
{
	if (IsValid(MoonAntHitCollider))
	{
		MoonAntHitCollider->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AJTSMoonAntActor::ConfigureMoonAntHealthBar()
{
	if (!IsValid(MoonAntHealthBarComponent))
	{
		return;
	}

	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent(); IsValid(ActiveVisual))
	{
		MoonAntHealthBarComponent->AttachToComponent(ActiveVisual, FAttachmentTransformRules::KeepWorldTransform);
		// MoonAnt visual scale is intentionally tiny (~28 cm); keep the screen-space widget readable.
		MoonAntHealthBarComponent->SetAbsolute(false, false, true);
	}

	MoonAntHealthBarComponent->InitWidget();
	if (UJTSHealthBarWidget* const HealthBarWidget = Cast<UJTSHealthBarWidget>(MoonAntHealthBarComponent->GetUserWidgetObject()))
	{
		HealthBarWidget->SetHealth(
			IsValid(HealthComponent) ? HealthComponent->GetHealth() : 0.0f,
			IsValid(HealthComponent) ? HealthComponent->GetMaxHealth() : 0.0f);
	}
}

void AJTSMoonAntActor::UpdateMoonAntHealthBarTransform()
{
	if (!IsValid(MoonAntHealthBarComponent))
	{
		return;
	}

	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	if (!IsValid(ActiveVisual) || !ActiveVisual->IsRegistered())
	{
		return;
	}

	ActiveVisual->UpdateBounds();
	if (IsUsingRealPlanetSurface())
	{
		FJTSSurfaceVisualProjectionBounds VisualBounds;
		if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
			ActiveVisual,
			GetActorLocation(),
			SurfaceUp,
			VisualBounds))
		{
			MoonAntHealthBarComponent->SetWorldLocation(VisualBounds.HighestPoint + SurfaceUp * 12.0f);
			return;
		}
	}

	const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
	MoonAntHealthBarComponent->SetWorldLocation(ActiveVisual->Bounds.Origin + SurfaceUp * (PhysicalExtent.Z + 12.0f));
}

void AJTSMoonAntActor::ShowMoonAntHealthBar()
{
	if (!IsValid(MoonAntHealthBarComponent) || !IsValid(HealthComponent) || HealthComponent->IsDead())
	{
		return;
	}

	ConfigureMoonAntHealthBar();
	UpdateMoonAntHealthBarTransform();
	MoonAntHealthBarComponent->SetVisibility(true);
	MoonAntHealthBarComponent->SetHiddenInGame(false);

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoonAntHealthBarHideTimerHandle);
		World->GetTimerManager().SetTimer(
			MoonAntHealthBarHideTimerHandle,
			this,
			&AJTSMoonAntActor::HideMoonAntHealthBar,
			FMath::Max(0.1f, MoonAntHealthBarVisibleDuration),
			false);
	}
}

void AJTSMoonAntActor::HideMoonAntHealthBar()
{
	if (IsValid(MoonAntHealthBarComponent))
	{
		MoonAntHealthBarComponent->SetVisibility(false);
		MoonAntHealthBarComponent->SetHiddenInGame(true);
	}
}

void AJTSMoonAntActor::HandleHealthDamaged(float CurrentHealth, float MaxHealth, float Damage, AActor* DamageCauser)
{
	(void)MaxHealth;
	(void)Damage;

	if (CurrentHealth <= 0.0f || !IsValid(HealthComponent) || HealthComponent->IsDead())
	{
		HideMoonAntHealthBar();
		return;
	}
	if (!HasAuthority())
	{
		ShowMoonAntHealthBar();
		return;
	}

	FleeSourceLocation = IsValid(DamageCauser)
		? DamageCauser->GetActorLocation()
		: GetActorLocation() - GetActorForwardVector() * 100.0f;
	ShowMoonAntHealthBar();
	SetMoonAntState(EJTSMoonAntState::ReactingToHit);
}

void AJTSMoonAntActor::HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser)
{
	(void)InstigatorController;
	(void)DamageCauser;

	if (bDeathSequenceStarted)
	{
		return;
	}

	bDeathSequenceStarted = true;
	HideMoonAntHealthBar();
	SetMeleeHitCollisionEnabled(false);
	SetActorTickEnabled(false);
	// Clients receive the replicated health/death state for presentation, but only the server may
	// create the corpse or destroy this replicated gameplay actor.
	if (!HasAuthority())
	{
		return;
	}

	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	const USkeletalMesh* const SkeletalMesh = IsValid(MoonAntMesh) ? MoonAntMesh->GetSkeletalMeshAsset() : nullptr;
	const UStaticMesh* const FallbackMesh = IsValid(MoonAntFallbackMesh) ? MoonAntFallbackMesh->GetStaticMesh() : nullptr;
	const UObject* const LoggedMesh = SkeletalMesh != nullptr
		? static_cast<const UObject*>(SkeletalMesh)
		: static_cast<const UObject*>(FallbackMesh);
	const FVector VisualScale = IsValid(ActiveVisual) ? ActiveVisual->GetComponentScale() : FVector::OneVector;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonAnt death: MoonAnt=%s Health=%.0f Location=(%.2f, %.2f, %.2f) Mesh=%s Scale=(%.4f, %.4f, %.4f)"),
		*GetNameSafe(this),
		IsValid(HealthComponent) ? HealthComponent->GetHealth() : 0.0f,
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z,
		*GetNameSafe(LoggedMesh),
		VisualScale.X,
		VisualScale.Y,
		VisualScale.Z);

	SpawnMoonAntCorpse();

	// SpawnMoonAntCorpse finishes the deferred actor (including its visual initialization) synchronously.
	// Only after that point is it safe to release this MoonAnt and its runtime mesh data.
	Destroy();
}

bool AJTSMoonAntActor::SpawnMoonAntCorpse()
{
	if (bHasDroppedCorpse || !IsValid(HealthComponent) || !HealthComponent->IsDead())
	{
		return false;
	}

	bHasDroppedCorpse = true;
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("MoonAnt Corpse spawn FAILED: MoonAnt=%s Reason=MissingWorld"), *GetNameSafe(this));
		return false;
	}

	// Read every visual input before any destruction. In particular, never use the MoonAntMesh after
	// Destroy() because BP-authored mesh, material, and scale data are no longer reliable then.
	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	TArray<UMaterialInterface*> SourceMaterials;
	if (IsValid(ActiveVisual))
	{
		const int32 MaterialCount = ActiveVisual->GetNumMaterials();
		SourceMaterials.Reserve(MaterialCount);
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			SourceMaterials.Add(ActiveVisual->GetMaterial(MaterialIndex));
		}
	}

	USkeletalMesh* const SourceSkeletalMesh = bUsingSkeletalMoonAntMesh && IsValid(MoonAntMesh)
		? MoonAntMesh->GetSkeletalMeshAsset()
		: nullptr;
	UStaticMesh* const SourceFallbackMesh = SourceSkeletalMesh == nullptr && IsValid(MoonAntFallbackMesh)
		? MoonAntFallbackMesh->GetStaticMesh()
		: nullptr;
	const FVector SourceRelativeScale = IsValid(ActiveVisual)
		? ActiveVisual->GetComponentScale()
		: FVector::OneVector;
	const FRotator SourceRelativeRotation = IsValid(ActiveVisual)
		? ActiveVisual->GetRelativeRotation()
		: FRotator::ZeroRotator;

	AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	FVector DeathGroundLocation = GroundLocation;

	if (IsValid(SurfaceController))
	{
		FVector ResolvedDeathGroundLocation;
		if (SurfaceController->OwnsSurfaceActor(this)
			&& SurfaceController->ResolveMoonGroundLocation(DeathGroundLocation, ResolvedDeathGroundLocation))
		{
			DeathGroundLocation = ResolvedDeathGroundLocation;
		}
	}

	const FTransform SpawnTransform(
		GetActorRotation(),
		DeathGroundLocation);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonAnt Corpse spawn requested: Location=(%.2f, %.2f, %.2f) Class=%s"),
		DeathGroundLocation.X,
		DeathGroundLocation.Y,
		DeathGroundLocation.Z,
		*GetNameSafe(AJTSMoonAntCorpsePickupActor::StaticClass()));
	AJTSMoonAntCorpsePickupActor* const CorpsePickup = World->SpawnActorDeferred<AJTSMoonAntCorpsePickupActor>(
		AJTSMoonAntCorpsePickupActor::StaticClass(),
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(CorpsePickup))
	{
		UE_LOG(LogTemp, Warning, TEXT("MoonAnt Corpse spawn FAILED: MoonAnt=%s Reason=DeferredSpawnReturnedNull"), *GetNameSafe(this));
		return false;
	}

	CorpsePickup->InitializeFromMoonAnt(
		SourceSkeletalMesh,
		SourceFallbackMesh,
		SourceMaterials,
		SourceRelativeScale,
		SourceRelativeRotation,
		DeathGroundLocation);
	if (IsValid(SurfaceController))
	{
		SurfaceController->RegisterSurfaceRuntimeActor(CorpsePickup);
	}
	CorpsePickup->FinishSpawning(SpawnTransform);
	if (!IsValid(CorpsePickup) || !CorpsePickup->HasVisibleCorpseVisual())
	{
		UE_LOG(LogTemp, Warning, TEXT("MoonAnt Corpse spawn FAILED: MoonAnt=%s Reason=NoVisibleCorpseVisual"), *GetNameSafe(this));
		if (IsValid(CorpsePickup))
		{
			CorpsePickup->Destroy();
		}
		return false;
	}

	const FVector CorpseScale = CorpsePickup->GetCorpseVisualScale();
	const FVector CorpseLocation = CorpsePickup->GetActorLocation();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonAnt Corpse spawned: Actor=%s Mesh=%s Location=(%.2f, %.2f, %.2f) Scale=(%.4f, %.4f, %.4f) Hidden=%s"),
		*GetNameSafe(CorpsePickup),
		*CorpsePickup->GetCorpseVisualDebugName(),
		CorpseLocation.X,
		CorpseLocation.Y,
		CorpseLocation.Z,
		CorpseScale.X,
		CorpseScale.Y,
		CorpseScale.Z,
		CorpsePickup->IsCorpseVisualHidden() ? TEXT("true") : TEXT("false"));
	return true;
}

void AJTSMoonAntActor::UpdateMoonAntVisualTransform()
{
	if (!bMoonAntVisualTransformReady)
	{
		return;
	}

	UWorld* const World = GetWorld();
	FVector FinalVisualRelativeLocation = MoonAntMeshBaseRelativeLocation
		+ FVector(0.0f, 0.0f, BurrowVisualOffset);
	if (bUsingSkeletalMoonAntMesh && IsValid(MoonAntMesh))
	{
		// Stateless reconstruction avoids accumulating burrow offsets across frames.
		MoonAntMesh->SetRelativeLocation(FinalVisualRelativeLocation);
		MoonAntMesh->SetRelativeRotation(MoonAntMeshBaseRelativeRotation + FRotator(0.0f, MoonAntMeshForwardYawOffset, 0.0f));
	}

	if (IsValid(MoonAntFallbackMesh))
	{
		MoonAntFallbackMesh->SetRelativeLocation(MoonAntFallbackBaseRelativeLocation + FVector(0.0f, 0.0f, BurrowVisualOffset));
	}

	if (bDebugMoonAntVisualTransform && World != nullptr && World->GetTimeSeconds() >= NextMoonAntVisualDebugLogTime)
	{
		NextMoonAntVisualDebugLogTime = World->GetTimeSeconds() + 1.0f;
		const float MeshRelativeZ = IsValid(MoonAntMesh) ? MoonAntMesh->GetRelativeLocation().Z : 0.0f;
		UE_LOG(
			LogTemp,
			Log,
		TEXT("MoonAnt visual: MoonAnt=%s State=%s ActorZ=%.2f MeshRelativeZ=%.2f BaseRelativeZ=%.2f BurrowOffsetZ=%.2f FinalRelativeZ=%.2f"),
			*GetName(),
			GetMoonAntStateDebugName(MoonAntState),
			GetActorLocation().Z,
			MeshRelativeZ,
			MoonAntMeshBaseRelativeLocation.Z,
			BurrowVisualOffset,
			FinalVisualRelativeLocation.Z);
	}
	else if (!bDebugMoonAntVisualTransform)
	{
		NextMoonAntVisualDebugLogTime = 0.0f;
	}
}

void AJTSMoonAntActor::SetVisualBurrowOffset(float RelativeZ)
{
	BurrowVisualOffset = RelativeZ;
}

void AJTSMoonAntActor::BeginFleeing()
{
	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (MoonGameMode == nullptr)
	{
		BeginBurrowing();
		return;
	}

	if (AJTSPlanetAnchor* const Planet = IsUsingRealPlanetSurface() ? GetSurfacePlanet() : nullptr)
	{
		FVector AwayDirection = Planet->ProjectDirectionToSurfaceTangent(
			GetActorLocation() - FleeSourceLocation,
			GetActorLocation());
		if (AwayDirection.IsNearlyZero())
		{
			AwayDirection = Planet->ProjectDirectionToSurfaceTangent(GetActorForwardVector(), GetActorLocation());
		}
		if (AwayDirection.IsNearlyZero())
		{
			FJTSPlanetSurfaceFrame SurfaceFrame;
			if (Planet->GetSurfaceFrameAt(GetActorLocation(), FVector::ForwardVector, SurfaceFrame))
			{
				AwayDirection = SurfaceFrame.Forward;
			}
		}

		const float SignedJitter = (FMath::RandBool() ? 1.0f : -1.0f)
			* FMath::FRandRange(15.0f, 30.0f);
		FleeDirection = FQuat(
			Planet->GetRadialUpVector(GetActorLocation()),
			FMath::DegreesToRadians(SignedJitter)).RotateVector(AwayDirection).GetSafeNormal();
		FleeDuration = FMath::FRandRange(MoonGameMode->GetMoonAntFleeDurationMin(), MoonGameMode->GetMoonAntFleeDurationMax());
		FleeDistanceTravelled = 0.0f;
		SetMoonAntState(EJTSMoonAntState::Fleeing);
		return;
	}

	FVector AwayDirection = GetActorLocation() - FleeSourceLocation;
	AwayDirection.Z = 0.0f;
	if (AwayDirection.IsNearlyZero())
	{
		AwayDirection = GetActorForwardVector();
		AwayDirection.Z = 0.0f;
	}
	if (AwayDirection.IsNearlyZero())
	{
		const float RandomAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
		AwayDirection = FVector(FMath::Cos(RandomAngle), FMath::Sin(RandomAngle), 0.0f);
	}

	const float JitterMagnitude = FMath::FRandRange(15.0f, 30.0f);
	const float SignedJitter = FMath::RandBool() ? JitterMagnitude : -JitterMagnitude;
	FleeDirection = FRotator(0.0f, SignedJitter, 0.0f).RotateVector(AwayDirection.GetSafeNormal()).GetSafeNormal();
	FleeDuration = FMath::FRandRange(MoonGameMode->GetMoonAntFleeDurationMin(), MoonGameMode->GetMoonAntFleeDurationMax());
	FleeDistanceTravelled = 0.0f;
	SetMoonAntState(EJTSMoonAntState::Fleeing);
}

void AJTSMoonAntActor::BeginBurrowing()
{
	if (MoonAntState != EJTSMoonAntState::Burrowing)
	{
		SetMoonAntState(EJTSMoonAntState::Burrowing);
	}
}

void AJTSMoonAntActor::SetMoonAntState(EJTSMoonAntState NewState)
{
	MoonAntState = NewState;
	StateElapsed = 0.0f;

	switch (NewState)
	{
	case EJTSMoonAntState::Emerging:
		SetMeleeHitCollisionEnabled(false);
		SetVisualBurrowOffset(-BurrowDepth);
		break;

	case EJTSMoonAntState::Roaming:
		SetMeleeHitCollisionEnabled(true);
		SetVisualBurrowOffset(0.0f);
		ChooseRoamTarget();
		break;

	case EJTSMoonAntState::ReactingToHit:
		SetMeleeHitCollisionEnabled(true);
		SetVisualBurrowOffset(0.0f);
		break;

	case EJTSMoonAntState::Fleeing:
		SetMeleeHitCollisionEnabled(true);
		break;

	case EJTSMoonAntState::Burrowing:
		SetMeleeHitCollisionEnabled(false);
		SetVisualBurrowOffset(0.0f);
		break;

	default:
		break;
	}

	UpdateFallbackMaterial();
}

void AJTSMoonAntActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJTSMoonAntActor, MoonAntState);
	DOREPLIFETIME(AJTSMoonAntActor, SurfacePlanet);
	DOREPLIFETIME(AJTSMoonAntActor, SurfaceUp);
	DOREPLIFETIME(AJTSMoonAntActor, bUsesRealPlanetSurface);
}

void AJTSMoonAntActor::OnRep_MoonAntState()
{
	const bool bCanBeHit = MoonAntState != EJTSMoonAntState::Emerging
		&& MoonAntState != EJTSMoonAntState::Burrowing
		&& (!IsValid(HealthComponent) || !HealthComponent->IsDead());
	SetMeleeHitCollisionEnabled(bCanBeHit);
	UpdateFallbackMaterial();
}

void AJTSMoonAntActor::UpdateFallbackMaterial()
{
	if (MoonAntFallbackMaterial != nullptr)
	{
		const FLinearColor MoonAntColor = MoonAntState == EJTSMoonAntState::ReactingToHit
			? FLinearColor(0.95f, 0.42f, 0.12f, 1.0f)
			: FLinearColor(0.22f, 0.075f, 0.025f, 1.0f);
		MoonAntFallbackMaterial->SetVectorParameterValue(TEXT("Color"), MoonAntColor);
		MoonAntFallbackMaterial->SetVectorParameterValue(TEXT("BaseColor"), MoonAntColor);
		MoonAntFallbackMaterial->SetVectorParameterValue(TEXT("Tint"), MoonAntColor);
	}
}
