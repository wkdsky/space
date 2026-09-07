#include "space/World/JTSRoachNestActor.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSRoachActor.h"
#include "UObject/ConstructorHelpers.h"

AJTSRoachNestActor::AJTSRoachNestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NestMesh"));
	NestMesh->SetupAttachment(SceneRoot);
	NestMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 0.20f));
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
	return FText::FromString(TEXT("ROACH NEST"));
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

	PunchHitsRemaining = MoonGameMode->GetRoachNestPunchHitsToDestroy();
	NestMaterial = NestMesh != nullptr ? NestMesh->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	if (NestMaterial != nullptr)
	{
		const FLinearColor NestColor(0.18f, 0.055f, 0.025f, 1.0f);
		NestMaterial->SetVectorParameterValue(TEXT("Color"), NestColor);
		NestMaterial->SetVectorParameterValue(TEXT("BaseColor"), NestColor);
		NestMaterial->SetVectorParameterValue(TEXT("Tint"), NestColor);
	}
	ScheduleNextRoachSpawn();
}

void AJTSRoachNestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoachSpawnTimerHandle);
	}

	ActiveRoach.Reset();
	Super::EndPlay(EndPlayReason);
}

const AJTSMoonGameMode* AJTSRoachNestActor::GetMoonGameMode() const
{
	const UWorld* const World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<AJTSMoonGameMode>() : nullptr;
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

bool AJTSRoachNestActor::ResolveRoachGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const
{
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(World) || !IsValid(MoonGameMode))
	{
		return false;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JTSRoachNestGroundTrace), false, this);
	TraceParams.AddIgnoredActor(this);
	if (APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		TraceParams.AddIgnoredActor(LocalPawn);
	}
	for (TActorIterator<AJTSCharacter> PlayerIt(World); PlayerIt; ++PlayerIt)
	{
		if (IsValid(*PlayerIt))
		{
			TraceParams.AddIgnoredActor(*PlayerIt);
		}
	}
	for (TActorIterator<AJTSSpacecraftActor> ShipIt(World); ShipIt; ++ShipIt)
	{
		if (IsValid(*ShipIt))
		{
			TraceParams.AddIgnoredActor(*ShipIt);
		}
	}
	for (TActorIterator<AJTSRoachNestActor> NestIt(World); NestIt; ++NestIt)
	{
		if (IsValid(*NestIt))
		{
			TraceParams.AddIgnoredActor(*NestIt);
		}
	}
	for (TActorIterator<AJTSRoachActor> RoachIt(World); RoachIt; ++RoachIt)
	{
		if (IsValid(*RoachIt))
		{
			TraceParams.AddIgnoredActor(*RoachIt);
		}
	}

	const float StartHeight = MoonGameMode->GetRoachGroundTraceStartHeight();
	const float TraceDistance = MoonGameMode->GetRoachGroundTraceDistance();
	FHitResult GroundHit;
	return TraceDistance > 0.0f
		&& World->LineTraceSingleByChannel(
			GroundHit,
			CandidateLocation + FVector(0.0f, 0.0f, StartHeight),
			CandidateLocation + FVector(0.0f, 0.0f, StartHeight - TraceDistance),
			ECC_Visibility,
			TraceParams)
		&& GroundHit.bBlockingHit
		&& (OutGroundLocation = GroundHit.ImpactPoint, true);
}

void AJTSRoachNestActor::ScheduleNextRoachSpawn()
{
	UWorld* const World = GetWorld();
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(World) || !IsValid(MoonGameMode) || IsPendingKillPending())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RoachSpawnTimerHandle,
		this,
		&AJTSRoachNestActor::TrySpawnRoach,
		FMath::FRandRange(MoonGameMode->GetRoachSpawnIntervalMin(), MoonGameMode->GetRoachSpawnIntervalMax()),
		false);
}

void AJTSRoachNestActor::TrySpawnRoach()
{
	const AJTSMoonGameMode* const MoonGameMode = GetMoonGameMode();
	UWorld* const World = GetWorld();
	if (!IsValid(World) || !IsValid(MoonGameMode) || IsPendingKillPending())
	{
		return;
	}

	if (ActiveRoach.IsValid() || FMath::FRand() > MoonGameMode->GetRoachSpawnChance())
	{
		ScheduleNextRoachSpawn();
		return;
	}

	const float SpawnAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const FVector SpawnDirection(FMath::Cos(SpawnAngle), FMath::Sin(SpawnAngle), 0.0f);
	const FVector CandidateLocation = GetActorLocation()
		+ SpawnDirection * FMath::FRandRange(MoonGameMode->GetRoachSpawnOffset() * 0.45f, MoonGameMode->GetRoachSpawnOffset());
	FVector GroundLocation;
	if (ResolveRoachGroundLocation(CandidateLocation, GroundLocation))
	{
		const FTransform SpawnTransform(SpawnDirection.Rotation(), GroundLocation);
		AJTSRoachActor* const Roach = World->SpawnActorDeferred<AJTSRoachActor>(
			AJTSRoachActor::StaticClass(),
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(Roach))
		{
			Roach->InitializeRoach(this, GroundLocation, SpawnDirection);
			Roach->FinishSpawning(SpawnTransform);
			ActiveRoach = Roach;
		}
	}

	ScheduleNextRoachSpawn();
}

void AJTSRoachNestActor::UpdateMoonWrappedLogicalPosition()
{
	if (MoonWrappedActorComponent != nullptr && MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}
