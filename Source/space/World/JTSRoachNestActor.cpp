#include "space/World/JTSRoachNestActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSRoachActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "UObject/ConstructorHelpers.h"

AJTSRoachNestActor::AJTSRoachNestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NestMesh"));
	NestMesh->SetupAttachment(SceneRoot);
	NestMesh->SetRelativeScale3D(BaseAntNestMeshScale);
	NestMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NestMesh->SetCollisionObjectType(ECC_WorldDynamic);
	NestMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	NestMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	NestMesh->SetGenerateOverlapEvents(false);
	NestMesh->SetCanEverAffectNavigation(false);

	MoonWrappedActorComponent = CreateDefaultSubobject<UJTSMoonWrappedActorComponent>(TEXT("MoonWrappedActorComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		NestMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FakeMoonBendMaterialAsset(TEXT("/Game/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.MI_JTSFakeMoon_Prop"));
	if (FakeMoonBendMaterialAsset.Succeeded())
	{
		NestMesh->SetMaterial(0, FakeMoonBendMaterialAsset.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BasicMaterialAsset.Succeeded())
		{
			NestMesh->SetMaterial(0, BasicMaterialAsset.Object);
		}
	}
}

void AJTSRoachNestActor::AdjustToGround(const FVector& GroundLocation)
{
	const FVector Extent = GetVisualBoundsExtent();
	SetActorLocation(
		FVector(GroundLocation.X, GroundLocation.Y, GroundLocation.Z + Extent.Z + 2.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();
}

void AJTSRoachNestActor::SetAntNestVisualScale(float InVisualScale)
{
	if (!IsValid(NestMesh))
	{
		return;
	}

	NestMesh->SetRelativeScale3D(BaseAntNestMeshScale * FMath::Max(0.1f, InVisualScale));
	if (NestMesh->IsRegistered())
	{
		NestMesh->UpdateBounds();
	}
}

void AJTSRoachNestActor::SetAntActorClass(TSubclassOf<AJTSRoachActor> InAntActorClass)
{
	AntActorClass = InAntActorClass;
}

bool AJTSRoachNestActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn) && PunchHitsRemaining > 0 && !IsPendingKillPending();
}

void AJTSRoachNestActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
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
	}
}

FText AJTSRoachNestActor::GetMeleeTargetDisplayName_Implementation() const
{
	return FText::FromString(TEXT("ANT NEST"));
}

FText AJTSRoachNestActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return CanReceiveMeleeHit_Implementation(AttackingPawn)
		? FText::FromString(TEXT("[LMB] ATTACK"))
		: FText::GetEmpty();
}

FVector AJTSRoachNestActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	if (IsValid(NestMesh) && NestMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(NestMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = NestMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return NestMesh->Bounds.Origin + FVector(0.0f, 0.0f, PhysicalExtent.Z + 24.0f);
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
}

void AJTSRoachNestActor::BeginPlay()
{
	Super::BeginPlay();

	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(MoonGameMode))
	{
		Destroy();
		return;
	}

	PunchHitsRemaining = MoonGameMode->GetAntNestPunchHitsToDestroy();
	AntNestMaterial = NestMesh != nullptr ? NestMesh->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	if (AntNestMaterial != nullptr)
	{
		const FLinearColor AntNestColor(0.18f, 0.055f, 0.025f, 1.0f);
		AntNestMaterial->SetVectorParameterValue(TEXT("Color"), AntNestColor);
		AntNestMaterial->SetVectorParameterValue(TEXT("BaseColor"), AntNestColor);
		AntNestMaterial->SetVectorParameterValue(TEXT("Tint"), AntNestColor);
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Moon Ant Nest created: Nest=%s AntClass=%s"),
		*GetNameSafe(this),
		AntActorClass != nullptr ? *GetNameSafe(AntActorClass.Get()) : TEXT("None (native fallback)"));
	ScheduleNextAntSpawn();
}

void AJTSRoachNestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AntSpawnTimerHandle);
	}

	ActiveAnts.Reset();
	Super::EndPlay(EndPlayReason);
}

const AJTSMoonGameMode* AJTSRoachNestActor::GetMoonGameMode() const
{
	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return Controller->OwnsSurfaceActor(this) ? Controller->GetMoonSettings() : nullptr;
	}

	return nullptr;
}

FVector AJTSRoachNestActor::GetVisualBoundsExtent() const
{
	if (IsValid(NestMesh) && NestMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(NestMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = NestMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		if (!PhysicalExtent.IsNearlyZero())
		{
			return PhysicalExtent;
		}
	}

	return FVector(34.0f, 34.0f, 10.0f);
}

bool AJTSRoachNestActor::ResolveAntGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const
{
	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return Controller->OwnsSurfaceActor(this)
			&& Controller->ResolveMoonGroundLocation(CandidateLocation, OutGroundLocation, this);
	}

	return false;
}

float AJTSRoachNestActor::ChooseAntSpawnDistance(const AJTSMoonGameMode& MoonGameMode) const
{
	const float NearWeight = MoonGameMode.GetAntSpawnNearWeight();
	const float MidWeight = MoonGameMode.GetAntSpawnMidWeight();
	const float FarWeight = MoonGameMode.GetAntSpawnFarWeight();
	const float TotalWeight = NearWeight + MidWeight + FarWeight;
	const float Selection = TotalWeight > KINDA_SMALL_NUMBER ? FMath::FRandRange(0.0f, TotalWeight) : 0.0f;
	float MinDistance = MoonGameMode.GetAntSpawnNearDistanceMin();
	float MaxDistance = MoonGameMode.GetAntSpawnNearDistanceMax();
	if (TotalWeight > KINDA_SMALL_NUMBER && Selection >= NearWeight)
	{
		if (Selection < NearWeight + MidWeight)
		{
			MinDistance = MoonGameMode.GetAntSpawnMidDistanceMin();
			MaxDistance = MoonGameMode.GetAntSpawnMidDistanceMax();
		}
		else
		{
			MinDistance = MoonGameMode.GetAntSpawnFarDistanceMin();
			MaxDistance = MoonGameMode.GetAntSpawnFarDistanceMax();
		}
	}

	const float BaseDistance = FMath::FRandRange(MinDistance, MaxDistance);
	const float RadialJitter = (MaxDistance - MinDistance) * 0.08f;
	return FMath::Clamp(BaseDistance + FMath::FRandRange(-RadialJitter, RadialJitter), MinDistance, MaxDistance);
}

void AJTSRoachNestActor::ScheduleNextAntSpawn()
{
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(World) || !IsValid(MoonGameMode) || IsPendingKillPending())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AntSpawnTimerHandle,
		this,
		&AJTSRoachNestActor::TrySpawnAnt,
		FMath::FRandRange(MoonGameMode->GetAntSpawnIntervalMin(), MoonGameMode->GetAntSpawnIntervalMax()),
		false);
}

void AJTSRoachNestActor::TrySpawnAnt()
{
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	UWorld* const World = GetWorld();
	if (!IsValid(World) || !IsValid(MoonGameMode) || IsPendingKillPending())
	{
		return;
	}

	ActiveAnts.RemoveAll([](const TWeakObjectPtr<AJTSRoachActor>& Ant)
	{
		return !Ant.IsValid();
	});
	if (ActiveAnts.Num() >= MoonGameMode->GetMaxActiveAntsPerNest()
		|| FMath::FRand() > MoonGameMode->GetAntSpawnChance())
	{
		ScheduleNextAntSpawn();
		return;
	}

	const float SpawnAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const FVector SpawnOffset(FMath::Cos(SpawnAngle), FMath::Sin(SpawnAngle), 0.0f);
	const float SpawnDistance = ChooseAntSpawnDistance(*MoonGameMode);
	FVector CandidateLocation = GetActorLocation() + SpawnOffset * SpawnDistance;
	if (const UJTSMoonWrapSubsystem* const MoonWrap = World->GetSubsystem<UJTSMoonWrapSubsystem>();
		IsValid(MoonWrap) && MoonWrap->IsConfiguredForMoon())
	{
		const FVector NestLocation = GetActorLocation();
		const FVector2D NestPhysicalXY(NestLocation.X, NestLocation.Y);
		const FVector2D NestLogicalXY = MoonWrap->GetLogicalPositionFromWorld(NestLocation);
		const FVector2D CandidateLogicalXY = MoonWrap->CanonicalizePosition2D(
			NestLogicalXY + FVector2D(SpawnOffset.X, SpawnOffset.Y) * SpawnDistance);
		const FVector2D CandidatePhysicalXY = MoonWrap->GetNearestPhysicalImage(NestPhysicalXY, CandidateLogicalXY);
		CandidateLocation.X = CandidatePhysicalXY.X;
		CandidateLocation.Y = CandidatePhysicalXY.Y;
	}
	FVector GroundLocation;
	if (ResolveAntGroundLocation(CandidateLocation, GroundLocation))
	{
		const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), GroundLocation);
		TSubclassOf<AJTSRoachActor> SpawnClass = AntActorClass;
		if (SpawnClass == nullptr)
		{
			SpawnClass = AJTSRoachActor::StaticClass();
		}
		AJTSRoachActor* const Ant = World->SpawnActorDeferred<AJTSRoachActor>(
			SpawnClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(Ant))
		{
			Ant->InitializeAnt(this, GroundLocation);
			Ant->FinishSpawning(SpawnTransform);
			ActiveAnts.Add(Ant);
		}
	}

	ScheduleNextAntSpawn();
}

void AJTSRoachNestActor::UpdateMoonWrappedLogicalPosition()
{
	if (MoonWrappedActorComponent != nullptr && MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}
