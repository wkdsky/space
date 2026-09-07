#include "space/World/JTSRoachActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/World/JTSRoachNestActor.h"
#include "UObject/ConstructorHelpers.h"

AJTSRoachActor::AJTSRoachActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetActorEnableCollision(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RoachMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoachMesh"));
	RoachMesh->SetupAttachment(SceneRoot);
	RoachMesh->SetRelativeScale3D(BaseMeshScale);
	RoachMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RoachMesh->SetCollisionObjectType(ECC_WorldDynamic);
	RoachMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RoachMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RoachMesh->SetGenerateOverlapEvents(false);
	RoachMesh->SetCanEverAffectNavigation(false);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		RoachMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		RoachMesh->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMaterialAsset.Succeeded())
		{
			RoachMesh->SetMaterial(0, BasicMaterialAsset.Object);
		}
	}
}

void AJTSRoachActor::InitializeRoach(AJTSRoachNestActor* InOriginNest, const FVector& InGroundLocation, const FVector& InCrawlDirection)
{
	OriginNest = InOriginNest;
	GroundLocation = InGroundLocation;
	CrawlDirection = FVector(InCrawlDirection.X, InCrawlDirection.Y, 0.0f).GetSafeNormal();
	if (CrawlDirection.IsNearlyZero())
	{
		CrawlDirection = FVector::ForwardVector;
	}
	bInitialized = true;
}

EJTSRoachState AJTSRoachActor::GetRoachState() const
{
	return RoachState;
}

bool AJTSRoachActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn)
		&& RoachState != EJTSRoachState::Burrowing
		&& !IsPendingKillPending();
}

void AJTSRoachActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	if (!CanReceiveMeleeHit_Implementation(AttackingPawn))
	{
		return;
	}

	if (AttackType == EJTSMeleeAttackType::Knife || AttackType == EJTSMeleeAttackType::Axe)
	{
		Destroy();
		return;
	}

	PunchHitsRemaining = FMath::Max(0, PunchHitsRemaining - 1);
	if (PunchHitsRemaining <= 0)
	{
		Destroy();
		return;
	}

	SetRoachState(EJTSRoachState::ReactingToHit);
	BeginEscape(AttackingPawn);
}

FText AJTSRoachActor::GetMeleeTargetDisplayName_Implementation() const
{
	return FText::FromString(TEXT("ROACH"));
}

FText AJTSRoachActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return CanReceiveMeleeHit_Implementation(AttackingPawn)
		? FText::FromString(TEXT("[LMB] ATTACK"))
		: FText::GetEmpty();
}

FVector AJTSRoachActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	if (IsValid(RoachMesh) && RoachMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(RoachMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = RoachMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return RoachMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 14.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 28.0f);
}

void AJTSRoachActor::BeginPlay()
{
	Super::BeginPlay();

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!bInitialized || !IsValid(MoonGameMode))
	{
		Destroy();
		return;
	}

	PunchHitsRemaining = MoonGameMode->GetRoachPunchHitsToKill();
	if (RoachMesh != nullptr)
	{
		RoachMaterial = RoachMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	RoachMesh->UpdateBounds();
	const float SupportHeight = RoachMesh->Bounds.BoxExtent.Z;
	SetActorLocation(GroundLocation + FVector(0.0f, 0.0f, SupportHeight), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(CrawlDirection.Rotation(), ETeleportType::TeleportPhysics);
	SetRoachState(EJTSRoachState::Emerging);
	UpdateAppearanceForState(0.0f);
	UpdateMoonWrappedLogicalPosition();
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
	LifeElapsed += SafeDeltaSeconds;
	if (RoachState != EJTSRoachState::Burrowing && LifeElapsed >= MoonGameMode->GetRoachLifetime())
	{
		BeginBurrow();
	}

	switch (RoachState)
	{
	case EJTSRoachState::Emerging:
	{
		const float Duration = MoonGameMode->GetRoachEmergingDuration();
		const float Progress = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		UpdateAppearanceForState(Progress);
		if (Progress >= 1.0f)
		{
			SetRoachState(EJTSRoachState::Crawling);
		}
		break;
	}

	case EJTSRoachState::Crawling:
		AdvanceHorizontal(CrawlDirection, MoonGameMode->GetRoachCrawlSpeed(), SafeDeltaSeconds);
		break;

	case EJTSRoachState::ReactingToHit:
		if (StateElapsed >= MoonGameMode->GetRoachHitReactionDuration())
		{
			SetRoachState(EJTSRoachState::Escaping);
		}
		break;

	case EJTSRoachState::Escaping:
		AdvanceHorizontal(EscapeDirection, MoonGameMode->GetRoachEscapeSpeed(), SafeDeltaSeconds);
		if (StateElapsed >= MoonGameMode->GetRoachEscapeDuration())
		{
			BeginBurrow();
		}
		break;

	case EJTSRoachState::Burrowing:
	{
		const float Duration = MoonGameMode->GetRoachBurrowTime();
		const float Progress = Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(StateElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		UpdateAppearanceForState(1.0f - Progress);
		if (Progress >= 1.0f)
		{
			Destroy();
		}
		break;
	}

	default:
		break;
	}
}

const AJTSMoonGameMode* AJTSRoachActor::GetMoonGameMode() const
{
	const UWorld* const World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr;
}

void AJTSRoachActor::AdvanceHorizontal(const FVector& Direction, float Speed, float DeltaSeconds)
{
	FVector SafeDirection(Direction.X, Direction.Y, 0.0f);
	SafeDirection = SafeDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(SafeDirection.Rotation(), ETeleportType::TeleportPhysics);
	SetActorLocation(
		GetActorLocation() + SafeDirection * FMath::Max(0.0f, Speed) * FMath::Max(0.0f, DeltaSeconds),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();
}

void AJTSRoachActor::BeginEscape(APawn* AttackingPawn)
{
	FVector AwayDirection = IsValid(AttackingPawn)
		? GetActorLocation() - AttackingPawn->GetActorLocation()
		: CrawlDirection;
	AwayDirection.Z = 0.0f;
	AwayDirection = AwayDirection.GetSafeNormal();
	if (AwayDirection.IsNearlyZero())
	{
		AwayDirection = CrawlDirection.IsNearlyZero() ? FVector::ForwardVector : CrawlDirection;
	}

	const float EscapeAngleRadians = FMath::DegreesToRadians(FMath::FRandRange(-18.0f, 18.0f));
	EscapeDirection = FQuat(FVector::UpVector, EscapeAngleRadians).RotateVector(AwayDirection).GetSafeNormal();
}

void AJTSRoachActor::BeginBurrow()
{
	if (RoachState != EJTSRoachState::Burrowing)
	{
		SetRoachState(EJTSRoachState::Burrowing);
	}
}

void AJTSRoachActor::SetRoachState(EJTSRoachState NewState)
{
	RoachState = NewState;
	StateElapsed = 0.0f;
	UpdateAppearanceForState(NewState == EJTSRoachState::Emerging ? 0.0f : 1.0f);
}

void AJTSRoachActor::UpdateAppearanceForState(float StateProgress)
{
	if (!IsValid(RoachMesh))
	{
		return;
	}

	const float VisibleScale = FMath::Clamp(StateProgress, 0.04f, 1.0f);
	RoachMesh->SetRelativeScale3D(FVector(BaseMeshScale.X, BaseMeshScale.Y, BaseMeshScale.Z * VisibleScale));
	if (RoachMaterial != nullptr)
	{
		const FLinearColor RoachColor = RoachState == EJTSRoachState::ReactingToHit
			? FLinearColor(0.95f, 0.42f, 0.12f, 1.0f)
			: FLinearColor(0.22f, 0.075f, 0.025f, 1.0f);
		RoachMaterial->SetVectorParameterValue(TEXT("Color"), RoachColor);
		RoachMaterial->SetVectorParameterValue(TEXT("BaseColor"), RoachColor);
		RoachMaterial->SetVectorParameterValue(TEXT("Tint"), RoachColor);
	}
}

void AJTSRoachActor::UpdateMoonWrappedLogicalPosition()
{
	if (MoonWrappedActorComponent != nullptr && MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}
