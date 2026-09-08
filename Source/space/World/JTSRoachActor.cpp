#include "space/World/JTSRoachActor.h"

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
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/UI/JTSHealthBarWidget.h"
#include "space/World/JTSAntCorpsePickupActor.h"
#include "space/World/JTSRoachNestActor.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* GetAntStateDebugName(EJTSAntState State)
	{
		switch (State)
		{
		case EJTSAntState::Emerging:
			return TEXT("Emerging");
		case EJTSAntState::Roaming:
			return TEXT("Roaming");
		case EJTSAntState::ReactingToHit:
			return TEXT("ReactingToHit");
		case EJTSAntState::Fleeing:
			return TEXT("Fleeing");
		case EJTSAntState::Burrowing:
			return TEXT("Burrowing");
		default:
			return TEXT("Unknown");
		}
	}
}

AJTSRoachActor::AJTSRoachActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetActorEnableCollision(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HealthComponent = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("HealthComponent"));

	AntMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AntMesh"));
	AntMesh->SetupAttachment(SceneRoot);
	AntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AntMesh->SetCollisionObjectType(ECC_WorldDynamic);
	AntMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	AntMesh->SetGenerateOverlapEvents(false);
	AntMesh->SetCanEverAffectNavigation(false);

	AntFallbackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AntFallbackMesh"));
	AntFallbackMesh->SetupAttachment(SceneRoot);
	AntFallbackMesh->SetRelativeScale3D(AntFallbackBaseMeshScale);
	AntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AntFallbackMesh->SetCollisionObjectType(ECC_WorldDynamic);
	AntFallbackMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	AntFallbackMesh->SetGenerateOverlapEvents(false);
	AntFallbackMesh->SetCanEverAffectNavigation(false);

	AntHitCollider = CreateDefaultSubobject<USphereComponent>(TEXT("AntHitCollider"));
	AntHitCollider->SetupAttachment(SceneRoot);
	AntHitCollider->InitSphereRadius(25.0f);
	AntHitCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AntHitCollider->SetCollisionObjectType(ECC_WorldDynamic);
	AntHitCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	AntHitCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	AntHitCollider->SetGenerateOverlapEvents(false);
	AntHitCollider->SetCanEverAffectNavigation(false);

	AntHealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("AntHealthBarComponent"));
	AntHealthBarComponent->SetupAttachment(SceneRoot);
	AntHealthBarComponent->SetWidgetClass(UJTSHealthBarWidget::StaticClass());
	AntHealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	AntHealthBarComponent->SetDrawAtDesiredSize(false);
	AntHealthBarComponent->SetDrawSize(FVector2D(110.0f, 14.0f));
	AntHealthBarComponent->SetPivot(FVector2D(0.5f, 1.0f));
	AntHealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AntHealthBarComponent->SetGenerateOverlapEvents(false);
	AntHealthBarComponent->SetCanEverAffectNavigation(false);
	AntHealthBarComponent->SetAbsolute(false, false, true);
	AntHealthBarComponent->SetVisibility(false);
	AntHealthBarComponent->SetHiddenInGame(true);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		AntFallbackMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		AntFallbackMesh->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMaterialAsset.Succeeded())
		{
			AntFallbackMesh->SetMaterial(0, BasicMaterialAsset.Object);
		}
	}
}

void AJTSRoachActor::InitializeAnt(AJTSRoachNestActor* InOriginNest, const FVector& InGroundLocation)
{
	OriginNest = InOriginNest;
	GroundLocation = InGroundLocation;
	bInitialized = IsValid(InOriginNest);
}

EJTSAntState AJTSRoachActor::GetAntState() const
{
	return AntState;
}

UJTSHealthComponent* AJTSRoachActor::GetHealthComponent() const
{
	return HealthComponent.Get();
}

bool AJTSRoachActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn)
		&& IsValid(HealthComponent)
		&& !HealthComponent->IsDead()
		&& AntState != EJTSAntState::Burrowing
		&& !IsPendingKillPending();
}

void AJTSRoachActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	if (!CanReceiveMeleeHit_Implementation(AttackingPawn))
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

FText AJTSRoachActor::GetMeleeTargetDisplayName_Implementation() const
{
	return FText::FromString(TEXT("ANT"));
}

FText AJTSRoachActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return CanReceiveMeleeHit_Implementation(AttackingPawn)
		? FText::FromString(TEXT("[LMB] ATTACK"))
		: FText::GetEmpty();
}

FVector AJTSRoachActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent(); IsValid(ActiveVisual) && ActiveVisual->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return ActiveVisual->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 14.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, GroundSupportHeight + 14.0f);
}

void AJTSRoachActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisualMode();
}

void AJTSRoachActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);
	bDeathSequenceStarted = false;
	bHasDroppedCorpse = false;

	// This repeats the construction-time decision so Blueprint component defaults are honored in every runtime spawn path.
	RefreshVisualMode();

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!bInitialized || !OriginNest.IsValid() || !IsValid(MoonGameMode) || !IsValid(HealthComponent))
	{
		Destroy();
		return;
	}

	HealthComponent->SetMaxHealth(AntMaxHealth, true);
	HealthComponent->OnDamaged.AddDynamic(this, &AJTSRoachActor::HandleHealthDamaged);
	HealthComponent->OnDeath.AddDynamic(this, &AJTSRoachActor::HandleHealthDeath);
	ConfigureAntVisuals();
	ConfigureAntHealthBar();
	HideAntHealthBar();
	PlaceOnGround(GroundLocation);
	SetAntState(EJTSAntState::Emerging);
	UpdateMoonWrappedLogicalPosition();
	UpdateAntVisualTransform();
	UpdateAntHealthBarTransform();

	const USkeletalMesh* const SkeletalMesh = IsValid(AntMesh) ? AntMesh->GetSkeletalMeshAsset() : nullptr;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Moon Ant spawned: Class=%s Mesh=%s Nest=%s"),
		*GetNameSafe(GetClass()),
		*GetNameSafe(SkeletalMesh),
		*GetNameSafe(OriginNest.Get()));
	if (GetClass() == AJTSRoachActor::StaticClass())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Moon Ant is using native fallback class; AntActorClass is not configured. Class=%s Mesh=%s Nest=%s"),
			*GetNameSafe(GetClass()),
			*GetNameSafe(SkeletalMesh),
			*GetNameSafe(OriginNest.Get()));
	}
}

void AJTSRoachActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDamaged.RemoveDynamic(this, &AJTSRoachActor::HandleHealthDamaged);
		HealthComponent->OnDeath.RemoveDynamic(this, &AJTSRoachActor::HandleHealthDeath);
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AntHealthBarHideTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AJTSRoachActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(MoonGameMode))
	{
		Destroy();
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	StateElapsed += SafeDeltaSeconds;

	switch (AntState)
	{
	case EJTSAntState::Emerging:
	{
		const float Duration = MoonGameMode->GetAntEmergingDuration();
		const float Progress = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		const float SmoothedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, Progress, 2.0f);
		SetVisualBurrowOffset(FMath::Lerp(-BurrowDepth, 0.0f, SmoothedProgress));
		if (Progress >= 1.0f)
		{
			SetVisualBurrowOffset(0.0f);
			SetMeleeHitCollisionEnabled(true);
			SurfaceElapsed = 0.0f;
			SurfaceDuration = FMath::FRandRange(
				MoonGameMode->GetAntSurfaceDurationMin(),
				MoonGameMode->GetAntSurfaceDurationMax());
			SetAntState(EJTSAntState::Roaming);
		}
		break;
	}

	case EJTSAntState::Roaming:
	{
		SurfaceElapsed += SafeDeltaSeconds;
		if (SurfaceElapsed >= SurfaceDuration)
		{
			BeginBurrowing();
			break;
		}

		RoamRetargetElapsed += SafeDeltaSeconds;
		const float HomeDistance = GetDistanceToOriginNest();
		FVector ToRoamTarget = GetShortestWrappedDeltaTo(RoamTargetLogicalPosition);
		if (HomeDistance > MoonGameMode->GetAntMaxHomeRadius())
		{
			FVector NestLocation;
			const FVector ToNest = GetOriginNestLocation(NestLocation)
				? GetShortestWrappedDeltaTo(FVector2D(NestLocation.X, NestLocation.Y))
				: FVector::ZeroVector;
			const float TargetAlignment = FVector::DotProduct(
				ToRoamTarget.GetSafeNormal2D(),
				ToNest.GetSafeNormal2D());
			if (ToRoamTarget.IsNearlyZero() || TargetAlignment < 0.25f)
			{
				ChooseRoamTarget(true);
				ToRoamTarget = GetShortestWrappedDeltaTo(RoamTargetLogicalPosition);
			}
		}

		if (ToRoamTarget.Size2D() <= 30.0f || RoamRetargetElapsed >= RoamRetargetInterval)
		{
			ChooseRoamTarget(HomeDistance > MoonGameMode->GetAntMaxHomeRadius());
			ToRoamTarget = GetShortestWrappedDeltaTo(RoamTargetLogicalPosition);
		}

		MoveAlongGround(ToRoamTarget, MoonGameMode->GetAntRoamSpeed(), SafeDeltaSeconds);
		break;
	}

	case EJTSAntState::ReactingToHit:
		if (StateElapsed >= MoonGameMode->GetAntHitReactionDuration())
		{
			BeginFleeing();
		}
		break;

	case EJTSAntState::Fleeing:
	{
		const FVector PreviousLocation = GetActorLocation();
		if (MoveAlongGround(FleeDirection, MoonGameMode->GetAntFleeSpeed(), SafeDeltaSeconds))
		{
			FleeDistanceTravelled += FVector::Dist2D(PreviousLocation, GetActorLocation());
		}
		if (StateElapsed >= FleeDuration || FleeDistanceTravelled >= MoonGameMode->GetAntMaxFleeDistance())
		{
			BeginBurrowing();
		}
		break;
	}

	case EJTSAntState::Burrowing:
	{
		const float Duration = MoonGameMode->GetAntBurrowDuration();
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

	UpdateAntVisualTransform();
	UpdateAntHealthBarTransform();
}

const AJTSMoonGameMode* AJTSRoachActor::GetMoonGameMode() const
{
	const UWorld* const World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr;
}

void AJTSRoachActor::RefreshVisualMode()
{
	bUsingSkeletalAntMesh = IsValid(AntMesh) && AntMesh->GetSkeletalMeshAsset() != nullptr;
	if (bUsingSkeletalAntMesh)
	{
		AntFallbackMesh->SetVisibility(false, true);
		AntFallbackMesh->SetHiddenInGame(true, true);
		AntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AntMesh->SetVisibility(true, true);
		AntMesh->SetHiddenInGame(false, true);
		AntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		AntMesh->SetVisibility(false, true);
		AntMesh->SetHiddenInGame(true, true);
		AntMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AntFallbackMesh->SetVisibility(true, true);
		AntFallbackMesh->SetHiddenInGame(false, true);
		AntFallbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AJTSRoachActor::ConfigureAntVisuals()
{
	RefreshVisualMode();
	if (bAntMeshBaseTransformCaptured)
	{
		return;
	}

	// The mesh is still in its Blueprint-authored, unbent transform here. Do not let a visual offset
	// affect the bounds/support calculation or the immutable base transform captured below.
	bAntVisualTransformReady = false;
	BurrowVisualOffset = 0.0f;

	const float SafeVariationMin = FMath::Max(0.1f, AntScaleVariationMin);
	const float SafeVariationMax = FMath::Max(SafeVariationMin, AntScaleVariationMax);
	const float DesiredBodyLength = FMath::Max(1.0f, AntTargetBodyLength) * FMath::FRandRange(SafeVariationMin, SafeVariationMax);

	AntMeshUniformScale = 1.0f;
	if (bUsingSkeletalAntMesh)
	{
		const auto MeshBounds = AntMesh->GetSkeletalMeshAsset()->GetBounds();
		const float MaxHorizontalBodySize = FMath::Max(
			FMath::Abs(MeshBounds.BoxExtent.X),
			FMath::Abs(MeshBounds.BoxExtent.Y)) * 2.0f;
		if (MaxHorizontalBodySize > KINDA_SMALL_NUMBER)
		{
			AntMeshUniformScale = DesiredBodyLength / MaxHorizontalBodySize;
		}
		AntMesh->SetRelativeScale3D(FVector(AntMeshUniformScale));
	}
	else
	{
		const float FallbackLengthScale = DesiredBodyLength / 100.0f;
		AntFallbackBaseMeshScale = FVector(
			FallbackLengthScale,
			FallbackLengthScale * 0.62f,
			FallbackLengthScale * 0.30f);
		AntFallbackMesh->SetRelativeScale3D(AntFallbackBaseMeshScale);
		AntFallbackMaterial = AntFallbackMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent())
	{
		ActiveVisual->UpdateBounds();
	}
	RecalculateGroundMetrics();
	CaptureBaseVisualTransforms();
	bAntVisualTransformReady = bAntMeshBaseTransformCaptured;

	if (IsValid(AntHitCollider))
	{
		AntHitCollider->SetSphereRadius(FMath::Max(12.0f, DesiredBodyLength * 0.42f));
	}
	UpdateFallbackMaterial();
}

void AJTSRoachActor::CaptureBaseVisualTransforms()
{
	if (bAntMeshBaseTransformCaptured)
	{
		return;
	}

	// This is intentionally the only base-transform capture. It happens after Blueprint defaults and
	// final visual scale are ready, but before Moon Bend or burrow offsets are ever applied.
	AntMeshBaseRelativeLocation = IsValid(AntMesh) ? AntMesh->GetRelativeLocation() : FVector::ZeroVector;
	AntMeshBaseRelativeRotation = IsValid(AntMesh) ? AntMesh->GetRelativeRotation() : FRotator::ZeroRotator;
	AntFallbackBaseRelativeLocation = IsValid(AntFallbackMesh) ? AntFallbackMesh->GetRelativeLocation() : FVector::ZeroVector;
	bAntMeshBaseTransformCaptured = true;
}

void AJTSRoachActor::RecalculateGroundMetrics()
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
	const float CenterOffsetZ = ActiveVisual->Bounds.Origin.Z - GetActorLocation().Z;
	GroundSupportHeight = FMath::Max(1.0f, PhysicalExtent.Z - CenterOffsetZ);
	BurrowDepth = FMath::Max(4.0f, PhysicalExtent.Z * 2.0f + 2.0f);
}

void AJTSRoachActor::ChooseRoamTarget(bool bForceNearNest)
{
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	FVector NestLocation;
	const UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	const bool bUseMoonWrap = IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon();
	if (!IsValid(MoonGameMode) || !GetOriginNestLocation(NestLocation))
	{
		RoamTargetLogicalPosition = bUseMoonWrap
			? MoonWrap->GetLogicalPositionFromWorld(GetActorLocation())
			: FVector2D(GetActorLocation().X, GetActorLocation().Y);
		RoamRetargetElapsed = 0.0f;
		RoamRetargetInterval = 0.5f;
		return;
	}

	const float RoamRadius = MoonGameMode->GetAntRoamRadius();
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
	const FVector2D NestLogicalPosition = bUseMoonWrap
		? MoonWrap->GetLogicalPositionFromWorld(NestLocation)
		: FVector2D(NestLocation.X, NestLocation.Y);
	RoamTargetLogicalPosition = NestLogicalPosition + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
	if (bUseMoonWrap)
	{
		RoamTargetLogicalPosition = MoonWrap->CanonicalizePosition2D(RoamTargetLogicalPosition);
	}

	RoamRetargetElapsed = 0.0f;
	RoamRetargetInterval = FMath::FRandRange(
		MoonGameMode->GetAntRoamRetargetIntervalMin(),
		MoonGameMode->GetAntRoamRetargetIntervalMax());
}

bool AJTSRoachActor::GetOriginNestLocation(FVector& OutNestLocation) const
{
	if (!OriginNest.IsValid())
	{
		return false;
	}

	OutNestLocation = OriginNest->GetActorLocation();
	return true;
}

float AJTSRoachActor::GetDistanceToOriginNest() const
{
	FVector NestLocation;
	return GetOriginNestLocation(NestLocation)
		? GetShortestWrappedDeltaTo(FVector2D(NestLocation.X, NestLocation.Y)).Size2D()
		: TNumericLimits<float>::Max();
}

FVector AJTSRoachActor::GetShortestWrappedDeltaTo(const FVector2D& TargetLogicalPosition) const
{
	const UWorld* const World = GetWorld();
	const UJTSMoonWrapSubsystem* const MoonWrap = World != nullptr ? World->GetSubsystem<UJTSMoonWrapSubsystem>() : nullptr;
	const FVector2D CurrentPosition(GetActorLocation().X, GetActorLocation().Y);
	if (IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
	{
		const FVector2D CurrentLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(GetActorLocation());
		const FVector2D Delta = MoonWrap->ShortestWrappedDelta2D(CurrentLogicalPosition, TargetLogicalPosition);
		return FVector(Delta.X, Delta.Y, 0.0f);
	}

	const FVector2D Delta = TargetLogicalPosition - CurrentPosition;
	return FVector(Delta.X, Delta.Y, 0.0f);
}

bool AJTSRoachActor::MoveAlongGround(const FVector& Direction, float Speed, float DeltaSeconds)
{
	FVector SafeDirection(Direction.X, Direction.Y, 0.0f);
	SafeDirection = SafeDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(MoonGameMode))
	{
		return false;
	}

	FVector CandidateLocation = GetActorLocation()
		+ SafeDirection * FMath::Max(0.0f, Speed) * FMath::Max(0.0f, DeltaSeconds);
	if (const UWorld* const World = GetWorld())
	{
		if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
			IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
		{
			const FVector2D CurrentPhysicalXY(GetActorLocation().X, GetActorLocation().Y);
			const FVector2D CurrentLogicalXY = MoonWrap->GetLogicalPositionFromWorld(GetActorLocation());
			const FVector2D CandidateLogicalXY = MoonWrap->CanonicalizePosition2D(
				CurrentLogicalXY + FVector2D(SafeDirection.X, SafeDirection.Y) * FMath::Max(0.0f, Speed) * FMath::Max(0.0f, DeltaSeconds));
			const FVector2D CandidatePhysicalXY = MoonWrap->GetNearestPhysicalImage(CurrentPhysicalXY, CandidateLogicalXY);
			CandidateLocation.X = CandidatePhysicalXY.X;
			CandidateLocation.Y = CandidatePhysicalXY.Y;
		}
	}

	FVector NewGroundLocation;
	if (!MoonGameMode->ResolveMoonGroundLocation(CandidateLocation, NewGroundLocation, this))
	{
		return false;
	}

	PlaceOnGround(NewGroundLocation);
	RotateTowardsDirection(SafeDirection, DeltaSeconds);
	UpdateMoonWrappedLogicalPosition();
	return true;
}

void AJTSRoachActor::RotateTowardsDirection(const FVector& Direction, float DeltaSeconds)
{
	FVector FlatDirection(Direction.X, Direction.Y, 0.0f);
	FlatDirection = FlatDirection.GetSafeNormal();
	if (FlatDirection.IsNearlyZero())
	{
		return;
	}

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(MoonGameMode))
	{
		return;
	}

	FRotator CurrentRotation = GetActorRotation();
	CurrentRotation.Pitch = 0.0f;
	CurrentRotation.Roll = 0.0f;
	FRotator DesiredRotation = FlatDirection.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;
	SetActorRotation(
		FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, FMath::Max(0.0f, DeltaSeconds), MoonGameMode->GetAntTurnSpeed()),
		ETeleportType::TeleportPhysics);
}

void AJTSRoachActor::PlaceOnGround(const FVector& NewGroundLocation)
{
	GroundLocation = NewGroundLocation;
	SetActorLocation(
		FVector(NewGroundLocation.X, NewGroundLocation.Y, NewGroundLocation.Z + GroundSupportHeight),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

UPrimitiveComponent* AJTSRoachActor::GetActiveVisualComponent() const
{
	return bUsingSkeletalAntMesh
		? static_cast<UPrimitiveComponent*>(AntMesh.Get())
		: static_cast<UPrimitiveComponent*>(AntFallbackMesh.Get());
}

void AJTSRoachActor::SetMeleeHitCollisionEnabled(bool bEnabled)
{
	if (IsValid(AntHitCollider))
	{
		AntHitCollider->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AJTSRoachActor::ConfigureAntHealthBar()
{
	if (!IsValid(AntHealthBarComponent))
	{
		return;
	}

	if (UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent(); IsValid(ActiveVisual))
	{
		AntHealthBarComponent->AttachToComponent(ActiveVisual, FAttachmentTransformRules::KeepWorldTransform);
		// Ant visual scale is intentionally tiny (~28 cm); keep the screen-space widget readable.
		AntHealthBarComponent->SetAbsolute(false, false, true);
	}

	AntHealthBarComponent->InitWidget();
	if (UJTSHealthBarWidget* const HealthBarWidget = Cast<UJTSHealthBarWidget>(AntHealthBarComponent->GetUserWidgetObject()))
	{
		HealthBarWidget->SetHealth(
			IsValid(HealthComponent) ? HealthComponent->GetHealth() : 0.0f,
			IsValid(HealthComponent) ? HealthComponent->GetMaxHealth() : 0.0f);
	}
}

void AJTSRoachActor::UpdateAntHealthBarTransform()
{
	if (!IsValid(AntHealthBarComponent))
	{
		return;
	}

	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	if (!IsValid(ActiveVisual) || !ActiveVisual->IsRegistered())
	{
		return;
	}

	ActiveVisual->UpdateBounds();
	const float BoundsScale = FMath::Max(FMath::Abs(ActiveVisual->BoundsScale), KINDA_SMALL_NUMBER);
	const FVector PhysicalExtent = ActiveVisual->Bounds.BoxExtent.GetAbs() / BoundsScale;
	FVector HealthBarLocation = ActiveVisual->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 12.0f);
	if (!bUsingSkeletalAntMesh)
	{
		// Fallback mesh bend is material WPO, so its component bounds stay at the physical location.
		HealthBarLocation += CurrentMoonBendWorldOffset;
	}

	AntHealthBarComponent->SetWorldLocation(HealthBarLocation);
}

void AJTSRoachActor::ShowAntHealthBar()
{
	if (!IsValid(AntHealthBarComponent) || !IsValid(HealthComponent) || HealthComponent->IsDead())
	{
		return;
	}

	ConfigureAntHealthBar();
	UpdateAntHealthBarTransform();
	AntHealthBarComponent->SetVisibility(true);
	AntHealthBarComponent->SetHiddenInGame(false);

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AntHealthBarHideTimerHandle);
		World->GetTimerManager().SetTimer(
			AntHealthBarHideTimerHandle,
			this,
			&AJTSRoachActor::HideAntHealthBar,
			FMath::Max(0.1f, AntHealthBarVisibleDuration),
			false);
	}
}

void AJTSRoachActor::HideAntHealthBar()
{
	if (IsValid(AntHealthBarComponent))
	{
		AntHealthBarComponent->SetVisibility(false);
		AntHealthBarComponent->SetHiddenInGame(true);
	}
}

void AJTSRoachActor::HandleHealthDamaged(float CurrentHealth, float MaxHealth, float Damage, AActor* DamageCauser)
{
	(void)MaxHealth;
	(void)Damage;

	if (CurrentHealth <= 0.0f || !IsValid(HealthComponent) || HealthComponent->IsDead())
	{
		HideAntHealthBar();
		return;
	}

	FleeSourceLocation = IsValid(DamageCauser)
		? DamageCauser->GetActorLocation()
		: GetActorLocation() - GetActorForwardVector() * 100.0f;
	ShowAntHealthBar();
	SetAntState(EJTSAntState::ReactingToHit);
}

void AJTSRoachActor::HandleHealthDeath(AController* InstigatorController, AActor* DamageCauser)
{
	(void)InstigatorController;
	(void)DamageCauser;

	if (bDeathSequenceStarted)
	{
		return;
	}

	bDeathSequenceStarted = true;
	HideAntHealthBar();
	SetMeleeHitCollisionEnabled(false);
	SetActorTickEnabled(false);

	UPrimitiveComponent* const ActiveVisual = GetActiveVisualComponent();
	const USkeletalMesh* const SkeletalMesh = IsValid(AntMesh) ? AntMesh->GetSkeletalMeshAsset() : nullptr;
	const UStaticMesh* const FallbackMesh = IsValid(AntFallbackMesh) ? AntFallbackMesh->GetStaticMesh() : nullptr;
	const UObject* const LoggedMesh = SkeletalMesh != nullptr
		? static_cast<const UObject*>(SkeletalMesh)
		: static_cast<const UObject*>(FallbackMesh);
	const FVector VisualScale = IsValid(ActiveVisual) ? ActiveVisual->GetComponentScale() : FVector::OneVector;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Moon Ant death: Ant=%s Health=%.0f Location=(%.2f, %.2f, %.2f) Mesh=%s Scale=(%.4f, %.4f, %.4f)"),
		*GetNameSafe(this),
		IsValid(HealthComponent) ? HealthComponent->GetHealth() : 0.0f,
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z,
		*GetNameSafe(LoggedMesh),
		VisualScale.X,
		VisualScale.Y,
		VisualScale.Z);

	if (HasAuthority())
	{
		SpawnAntCorpse();
	}

	// SpawnAntCorpse finishes the deferred actor (including its visual initialization) synchronously.
	// Only after that point is it safe to release this Ant and its runtime mesh data.
	Destroy();
}

bool AJTSRoachActor::SpawnAntCorpse()
{
	if (bHasDroppedCorpse || !IsValid(HealthComponent) || !HealthComponent->IsDead())
	{
		return false;
	}

	bHasDroppedCorpse = true;
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ant Corpse spawn FAILED: Ant=%s Reason=MissingWorld"), *GetNameSafe(this));
		return false;
	}

	// Read every visual input before any destruction. In particular, never use the AntMesh after
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

	USkeletalMesh* const SourceSkeletalMesh = bUsingSkeletalAntMesh && IsValid(AntMesh)
		? AntMesh->GetSkeletalMeshAsset()
		: nullptr;
	UStaticMesh* const SourceFallbackMesh = SourceSkeletalMesh == nullptr && IsValid(AntFallbackMesh)
		? AntFallbackMesh->GetStaticMesh()
		: nullptr;
	const FVector SourceRelativeScale = IsValid(ActiveVisual)
		? ActiveVisual->GetComponentScale()
		: FVector::OneVector;
	const FRotator SourceRelativeRotation = IsValid(ActiveVisual)
		? ActiveVisual->GetRelativeRotation()
		: FRotator::ZeroRotator;

	// GroundLocation retains the unbent support Z. Rebuild its XY from the Ant's current wrapped
	// logical position and root physical image; never derive gameplay placement from AntMesh's bent world position.
	FVector DeathGroundLocation = GroundLocation;
	if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
		IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
	{
		const FVector2D AntLogicalPosition = IsValid(MoonWrappedActorComponent)
			? MoonWrappedActorComponent->GetLogicalPosition2D()
			: MoonWrap->GetLogicalPositionFromWorld(GetActorLocation());
		const FVector2D DeathPhysicalPosition = MoonWrap->GetNearestPhysicalImage(
			FVector2D(GetActorLocation().X, GetActorLocation().Y),
			AntLogicalPosition);
		DeathGroundLocation.X = DeathPhysicalPosition.X;
		DeathGroundLocation.Y = DeathPhysicalPosition.Y;
	}

	if (const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode())
	{
		FVector ResolvedDeathGroundLocation;
		if (MoonGameMode->ResolveMoonGroundLocation(DeathGroundLocation, ResolvedDeathGroundLocation, this))
		{
			DeathGroundLocation = ResolvedDeathGroundLocation;
		}
	}

	const FTransform SpawnTransform(
		FRotator(0.0f, GetActorRotation().Yaw, 0.0f),
		DeathGroundLocation);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Ant Corpse spawn requested: Location=(%.2f, %.2f, %.2f) Class=%s"),
		DeathGroundLocation.X,
		DeathGroundLocation.Y,
		DeathGroundLocation.Z,
		*GetNameSafe(AJTSAntCorpsePickupActor::StaticClass()));
	AJTSAntCorpsePickupActor* const CorpsePickup = World->SpawnActorDeferred<AJTSAntCorpsePickupActor>(
		AJTSAntCorpsePickupActor::StaticClass(),
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(CorpsePickup))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ant Corpse spawn FAILED: Ant=%s Reason=DeferredSpawnReturnedNull"), *GetNameSafe(this));
		return false;
	}

	CorpsePickup->InitializeFromAnt(
		SourceSkeletalMesh,
		SourceFallbackMesh,
		SourceMaterials,
		SourceRelativeScale,
		SourceRelativeRotation,
		DeathGroundLocation);
	CorpsePickup->FinishSpawning(SpawnTransform);
	if (!IsValid(CorpsePickup) || !CorpsePickup->HasVisibleCorpseVisual())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ant Corpse spawn FAILED: Ant=%s Reason=NoVisibleCorpseVisual"), *GetNameSafe(this));
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
		TEXT("Ant Corpse spawned: Actor=%s Mesh=%s Location=(%.2f, %.2f, %.2f) Scale=(%.4f, %.4f, %.4f) Hidden=%s"),
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

void AJTSRoachActor::UpdateAntVisualTransform()
{
	if (!bAntVisualTransformReady)
	{
		return;
	}

	FVector MoonBendWorldOffset = FVector::ZeroVector;
	CurrentMoonBendWorldOffset = FVector::ZeroVector;
	FVector BendOriginLocation = FVector::ZeroVector;
	float DistanceToBendOrigin = 0.0f;
	UWorld* const World = GetWorld();
	const APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (IsValid(LocalPawn))
	{
		// This is intentionally read on every visual update; only the pawn reference may be cached by
		// the engine, never its spawn-time location.
		BendOriginLocation = LocalPawn->GetActorLocation();
	}

	if (World != nullptr && IsValid(LocalPawn))
	{
		if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
			IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
		{
			// Skeletal Ant visuals require a physical nearest-image refresh before their visual-only bend.
			// The fallback material already evaluates its own WPO path, so do not alter its existing behavior.
			if (bUsingSkeletalAntMesh
				&& MoonWrappedActorComponent != nullptr
				&& MoonWrappedActorComponent->IsMoonWrappingEnabled())
			{
				UpdateMoonWrappedLogicalPosition();
				MoonWrappedActorComponent->RefreshPhysicalImage();
			}

			// AJTSMoonWorldActor writes BendOrigin from this pawn location, not from PlayerCameraManager.
			const FVector ActorPhysicalLocation = GetActorLocation();
			const FVector2D AntLogicalPosition = MoonWrap->GetLogicalPositionFromWorld(ActorPhysicalLocation);
			const FVector2D AntPhysicalImage = MoonWrap->GetNearestPhysicalImage(
				FVector2D(BendOriginLocation.X, BendOriginLocation.Y),
				AntLogicalPosition);
			const FVector BendPhysicalPosition(AntPhysicalImage.X, AntPhysicalImage.Y, ActorPhysicalLocation.Z);
			DistanceToBendOrigin = FVector2D::Distance(
				FVector2D(BendPhysicalPosition.X, BendPhysicalPosition.Y),
				FVector2D(BendOriginLocation.X, BendOriginLocation.Y));
			const FVector BendVisualPosition = MoonWrap->GetMoonVisualWorldPosition(BendPhysicalPosition, BendOriginLocation);
			MoonBendWorldOffset = BendVisualPosition - BendPhysicalPosition;
		}
	}
	CurrentMoonBendWorldOffset = MoonBendWorldOffset;

	FVector MoonBendRelativeOffset = MoonBendWorldOffset;
	if (IsValid(SceneRoot))
	{
		MoonBendRelativeOffset = SceneRoot->GetComponentTransform().InverseTransformVector(MoonBendWorldOffset);
	}

	FVector FinalVisualRelativeLocation = AntMeshBaseRelativeLocation
		+ FVector(0.0f, 0.0f, BurrowVisualOffset)
		+ MoonBendRelativeOffset;
	if (bUsingSkeletalAntMesh && IsValid(AntMesh))
	{
		// Stateless reconstruction: never add to the current relative location, because it may contain
		// a prior frame's visual bend.
		AntMesh->SetRelativeLocation(FinalVisualRelativeLocation);
		AntMesh->SetRelativeRotation(AntMeshBaseRelativeRotation + FRotator(0.0f, AntMeshForwardYawOffset, 0.0f));
	}

	if (IsValid(AntFallbackMesh))
	{
		// The fallback material evaluates JTSFakeMoon WPO itself; only rebuild its burrow contribution here.
		AntFallbackMesh->SetRelativeLocation(AntFallbackBaseRelativeLocation + FVector(0.0f, 0.0f, BurrowVisualOffset));
	}

	if (bDebugAntVisualTransform && World != nullptr && World->GetTimeSeconds() >= NextAntVisualDebugLogTime)
	{
		NextAntVisualDebugLogTime = World->GetTimeSeconds() + 1.0f;
		const float MeshRelativeZ = IsValid(AntMesh) ? AntMesh->GetRelativeLocation().Z : 0.0f;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Moon Ant visual: Ant=%s State=%s ActorZ=%.2f MeshRelativeZ=%.2f BaseRelativeZ=%.2f BurrowOffsetZ=%.2f MoonBendOffsetZ=%.2f FinalRelativeZ=%.2f DistanceToBendOrigin=%.2f BendOriginXY=(%.2f, %.2f)"),
			*GetName(),
			GetAntStateDebugName(AntState),
			GetActorLocation().Z,
			MeshRelativeZ,
			AntMeshBaseRelativeLocation.Z,
			BurrowVisualOffset,
			MoonBendRelativeOffset.Z,
			FinalVisualRelativeLocation.Z,
			DistanceToBendOrigin,
			BendOriginLocation.X,
			BendOriginLocation.Y);
	}
	else if (!bDebugAntVisualTransform)
	{
		NextAntVisualDebugLogTime = 0.0f;
	}
}

void AJTSRoachActor::SetVisualBurrowOffset(float RelativeZ)
{
	BurrowVisualOffset = RelativeZ;
}

void AJTSRoachActor::BeginFleeing()
{
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(MoonGameMode))
	{
		BeginBurrowing();
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
	FleeDuration = FMath::FRandRange(MoonGameMode->GetAntFleeDurationMin(), MoonGameMode->GetAntFleeDurationMax());
	FleeDistanceTravelled = 0.0f;
	SetAntState(EJTSAntState::Fleeing);
}

void AJTSRoachActor::BeginBurrowing()
{
	if (AntState != EJTSAntState::Burrowing)
	{
		SetAntState(EJTSAntState::Burrowing);
	}
}

void AJTSRoachActor::SetAntState(EJTSAntState NewState)
{
	AntState = NewState;
	StateElapsed = 0.0f;

	switch (NewState)
	{
	case EJTSAntState::Emerging:
		SetMeleeHitCollisionEnabled(false);
		SetVisualBurrowOffset(-BurrowDepth);
		break;

	case EJTSAntState::Roaming:
		SetMeleeHitCollisionEnabled(true);
		SetVisualBurrowOffset(0.0f);
		ChooseRoamTarget();
		break;

	case EJTSAntState::ReactingToHit:
		SetMeleeHitCollisionEnabled(true);
		SetVisualBurrowOffset(0.0f);
		break;

	case EJTSAntState::Fleeing:
		SetMeleeHitCollisionEnabled(true);
		break;

	case EJTSAntState::Burrowing:
		SetMeleeHitCollisionEnabled(false);
		SetVisualBurrowOffset(0.0f);
		break;

	default:
		break;
	}

	UpdateFallbackMaterial();
}

void AJTSRoachActor::UpdateFallbackMaterial()
{
	if (AntFallbackMaterial != nullptr)
	{
		const FLinearColor AntColor = AntState == EJTSAntState::ReactingToHit
			? FLinearColor(0.95f, 0.42f, 0.12f, 1.0f)
			: FLinearColor(0.22f, 0.075f, 0.025f, 1.0f);
		AntFallbackMaterial->SetVectorParameterValue(TEXT("Color"), AntColor);
		AntFallbackMaterial->SetVectorParameterValue(TEXT("BaseColor"), AntColor);
		AntFallbackMaterial->SetVectorParameterValue(TEXT("Tint"), AntColor);
	}
}

void AJTSRoachActor::UpdateMoonWrappedLogicalPosition()
{
	if (MoonWrappedActorComponent != nullptr && MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}
