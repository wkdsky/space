#include "space/World/JTSMoonAntNestActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "space/Components/JTSMoonWrappedActorComponent.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "UObject/ConstructorHelpers.h"

AJTSMoonAntNestActor::AJTSMoonAntNestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NestMesh"));
	NestMesh->SetupAttachment(SceneRoot);
	NestMesh->SetRelativeScale3D(BaseMoonAntNestMeshScale);
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

void AJTSMoonAntNestActor::AdjustToGround(const FVector& GroundLocation)
{
	bUsesRealPlanetSurface = false;
	SurfacePlanet.Reset();
	SurfaceUp = FVector::UpVector;
	const FVector Extent = GetVisualBoundsExtent();
	SetActorLocation(
		FVector(GroundLocation.X, GroundLocation.Y, GroundLocation.Z + Extent.Z + 2.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	UpdateMoonWrappedLogicalPosition();
}

void AJTSMoonAntNestActor::PlaceOnPlanetSurface(
	AJTSPlanetAnchor* Planet,
	const FVector& GroundLocation,
	const FVector& PreferredForward)
{
	if (!IsValid(Planet) || !IsValid(NestMesh))
	{
		AdjustToGround(GroundLocation);
		return;
	}

	FJTSPlanetSurfaceFrame SurfaceFrame;
	if (!Planet->GetSurfaceFrameAt(GroundLocation, PreferredForward, SurfaceFrame))
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSMoonAntNestActor %s could not resolve a real Moon surface frame."), *GetName());
		return;
	}

	bUsesRealPlanetSurface = true;
	SurfacePlanet = Planet;
	SurfaceUp = SurfaceFrame.Up.GetSafeNormal();
	SetActorRotation(SurfaceFrame.Transform.Rotator(), ETeleportType::TeleportPhysics);
	NestMesh->UpdateBounds();
	const float SurfaceSupport = GetVisualBoundsExtent().Size() + 2.0f;
	SetActorLocation(
		SurfaceFrame.Location + SurfaceUp * SurfaceSupport,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (MoonWrappedActorComponent != nullptr)
	{
		MoonWrappedActorComponent->Deactivate();
		MoonWrappedActorComponent->SetComponentTickEnabled(false);
	}
}

void AJTSMoonAntNestActor::SetMoonAntNestVisualScale(float InVisualScale)
{
	if (!IsValid(NestMesh))
	{
		return;
	}

	NestMesh->SetRelativeScale3D(BaseMoonAntNestMeshScale * FMath::Max(0.1f, InVisualScale));
	if (NestMesh->IsRegistered())
	{
		NestMesh->UpdateBounds();
	}
}

void AJTSMoonAntNestActor::SetMoonAntActorClass(TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass)
{
	MoonAntActorClass = InMoonAntActorClass;
}

bool AJTSMoonAntNestActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn) && PunchHitsRemaining > 0 && !IsPendingKillPending();
}

void AJTSMoonAntNestActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
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

FText AJTSMoonAntNestActor::GetMeleeTargetDisplayName_Implementation() const
{
	return FText::FromString(TEXT("MOON ANT NEST"));
}

FText AJTSMoonAntNestActor::GetMeleeTargetPrompt_Implementation(APawn* AttackingPawn) const
{
	return CanReceiveMeleeHit_Implementation(AttackingPawn)
		? FText::FromString(TEXT("[LMB] ATTACK"))
		: FText::GetEmpty();
}

FVector AJTSMoonAntNestActor::GetMeleeTargetAnchorWorldLocation_Implementation() const
{
	if (IsValid(NestMesh) && NestMesh->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(NestMesh->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = NestMesh->Bounds.BoxExtent.GetAbs() / BoundsScale;
		const float SurfaceExtent = IsUsingRealPlanetSurface() ? PhysicalExtent.Size() : PhysicalExtent.Z;
		return NestMesh->Bounds.Origin + SurfaceUp * (SurfaceExtent + 24.0f);
	}

	return GetActorLocation() + SurfaceUp * 55.0f;
}

void AJTSMoonAntNestActor::BeginPlay()
{
	Super::BeginPlay();

	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (MoonGameMode == nullptr)
	{
		Destroy();
		return;
	}

	PunchHitsRemaining = MoonGameMode->GetMoonAntNestPunchHitsToDestroy();
	if (IsUsingRealPlanetSurface() && MoonWrappedActorComponent != nullptr)
	{
		MoonWrappedActorComponent->Deactivate();
		MoonWrappedActorComponent->SetComponentTickEnabled(false);
	}
	MoonAntNestMaterial = NestMesh != nullptr ? NestMesh->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	if (MoonAntNestMaterial != nullptr)
	{
		const FLinearColor MoonAntNestColor(0.18f, 0.055f, 0.025f, 1.0f);
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("Color"), MoonAntNestColor);
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("BaseColor"), MoonAntNestColor);
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("Tint"), MoonAntNestColor);
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonAnt Nest created: Nest=%s MoonAntClass=%s"),
		*GetNameSafe(this),
		MoonAntActorClass != nullptr ? *GetNameSafe(MoonAntActorClass.Get()) : TEXT("None (native fallback)"));
	ScheduleNextMoonAntSpawn();
}

void AJTSMoonAntNestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoonAntSpawnTimerHandle);
	}

	ActiveMoonAnts.Reset();
	Super::EndPlay(EndPlayReason);
}

const IJTSMoonSurfaceGameplaySettings* AJTSMoonAntNestActor::GetMoonGameMode() const
{
	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return Controller->OwnsSurfaceActor(this) ? Controller->GetMoonSettings() : nullptr;
	}

	return nullptr;
}

AJTSPlanetAnchor* AJTSMoonAntNestActor::GetSurfacePlanet() const
{
	if (SurfacePlanet.IsValid())
	{
		return SurfacePlanet.Get();
	}

	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(Controller) && Controller->IsUsingRealPlanetSurfaceGameplay())
	{
		return Controller->GetOwningPlanet();
	}

	return nullptr;
}

bool AJTSMoonAntNestActor::IsUsingRealPlanetSurface() const
{
	return bUsesRealPlanetSurface && IsValid(GetSurfacePlanet());
}

FVector AJTSMoonAntNestActor::GetVisualBoundsExtent() const
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

bool AJTSMoonAntNestActor::ResolveMoonAntGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const
{
	if (const AJTSMoonSurfaceController* const Controller = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
	{
		return Controller->OwnsSurfaceActor(this)
			&& Controller->ResolveMoonGroundLocation(CandidateLocation, OutGroundLocation, this);
	}

	return false;
}

float AJTSMoonAntNestActor::ChooseMoonAntSpawnDistance(const IJTSMoonSurfaceGameplaySettings& MoonGameMode) const
{
	const float NearWeight = MoonGameMode.GetMoonAntSpawnNearWeight();
	const float MidWeight = MoonGameMode.GetMoonAntSpawnMidWeight();
	const float FarWeight = MoonGameMode.GetMoonAntSpawnFarWeight();
	const float TotalWeight = NearWeight + MidWeight + FarWeight;
	const float Selection = TotalWeight > KINDA_SMALL_NUMBER ? FMath::FRandRange(0.0f, TotalWeight) : 0.0f;
	float MinDistance = MoonGameMode.GetMoonAntSpawnNearDistanceMin();
	float MaxDistance = MoonGameMode.GetMoonAntSpawnNearDistanceMax();
	if (TotalWeight > KINDA_SMALL_NUMBER && Selection >= NearWeight)
	{
		if (Selection < NearWeight + MidWeight)
		{
			MinDistance = MoonGameMode.GetMoonAntSpawnMidDistanceMin();
			MaxDistance = MoonGameMode.GetMoonAntSpawnMidDistanceMax();
		}
		else
		{
			MinDistance = MoonGameMode.GetMoonAntSpawnFarDistanceMin();
			MaxDistance = MoonGameMode.GetMoonAntSpawnFarDistanceMax();
		}
	}

	const float BaseDistance = FMath::FRandRange(MinDistance, MaxDistance);
	const float RadialJitter = (MaxDistance - MinDistance) * 0.08f;
	return FMath::Clamp(BaseDistance + FMath::FRandRange(-RadialJitter, RadialJitter), MinDistance, MaxDistance);
}

void AJTSMoonAntNestActor::ScheduleNextMoonAntSpawn()
{
	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (!IsValid(World) || MoonGameMode == nullptr || IsPendingKillPending())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MoonAntSpawnTimerHandle,
		this,
		&AJTSMoonAntNestActor::TrySpawnMoonAnt,
		FMath::FRandRange(MoonGameMode->GetMoonAntSpawnIntervalMin(), MoonGameMode->GetMoonAntSpawnIntervalMax()),
		false);
}

void AJTSMoonAntNestActor::TrySpawnMoonAnt()
{
	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	UWorld* const World = GetWorld();
	if (!IsValid(World) || MoonGameMode == nullptr || IsPendingKillPending())
	{
		return;
	}

	ActiveMoonAnts.RemoveAll([](const TWeakObjectPtr<AJTSMoonAntActor>& MoonAnt)
	{
		return !MoonAnt.IsValid();
	});
	if (ActiveMoonAnts.Num() >= MoonGameMode->GetMaxActiveMoonAntsPerNest()
		|| FMath::FRand() > MoonGameMode->GetMoonAntSpawnChance())
	{
		ScheduleNextMoonAntSpawn();
		return;
	}

	const float SpawnAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
	const float SpawnDistance = ChooseMoonAntSpawnDistance(*MoonGameMode);
	if (AJTSPlanetAnchor* const Planet = IsUsingRealPlanetSurface() ? GetSurfacePlanet() : nullptr)
	{
		FJTSPlanetSurfaceFrame NestSurfaceFrame;
		FJTSPlanetSurfaceHit SpawnSurfaceHit;
		if (Planet->GetSurfaceFrameAt(GetActorLocation(), GetActorForwardVector(), NestSurfaceFrame))
		{
			const FVector SpawnDirection = (
				NestSurfaceFrame.Forward * FMath::Cos(SpawnAngle)
				+ NestSurfaceFrame.Right * FMath::Sin(SpawnAngle)).GetSafeNormal();
			const FVector CandidateLocation = GetActorLocation() + SpawnDirection * SpawnDistance;
			if (Planet->ProjectPointToSurface(CandidateLocation, SpawnSurfaceHit))
			{
				const FVector SpawnForward = FQuat(
					SpawnSurfaceHit.ImpactNormal,
					FMath::FRandRange(0.0f, UE_TWO_PI)).RotateVector(SpawnDirection);
				const FTransform SpawnTransform(
					FRotationMatrix::MakeFromXZ(SpawnForward, SpawnSurfaceHit.ImpactNormal).ToQuat(),
					SpawnSurfaceHit.ImpactPoint);
				TSubclassOf<AJTSMoonAntActor> SpawnClass = MoonAntActorClass;
				if (SpawnClass == nullptr)
				{
					SpawnClass = AJTSMoonAntActor::StaticClass();
				}
				AJTSMoonAntActor* const MoonAnt = World->SpawnActorDeferred<AJTSMoonAntActor>(
					SpawnClass,
					SpawnTransform,
					this,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (IsValid(MoonAnt))
				{
					MoonAnt->InitializeMoonAnt(this, SpawnSurfaceHit.ImpactPoint);
					if (AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
					{
						SurfaceController->RegisterSurfaceRuntimeActor(MoonAnt);
					}
					MoonAnt->FinishSpawning(SpawnTransform);
					ActiveMoonAnts.Add(MoonAnt);
				}
			}
		}

		ScheduleNextMoonAntSpawn();
		return;
	}

	const FVector SpawnOffset(FMath::Cos(SpawnAngle), FMath::Sin(SpawnAngle), 0.0f);
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
	if (ResolveMoonAntGroundLocation(CandidateLocation, GroundLocation))
	{
		const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), GroundLocation);
		TSubclassOf<AJTSMoonAntActor> SpawnClass = MoonAntActorClass;
		if (SpawnClass == nullptr)
		{
			SpawnClass = AJTSMoonAntActor::StaticClass();
		}
		AJTSMoonAntActor* const MoonAnt = World->SpawnActorDeferred<AJTSMoonAntActor>(
			SpawnClass,
			SpawnTransform,
			this,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(MoonAnt))
		{
			MoonAnt->InitializeMoonAnt(this, GroundLocation);
			if (AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this))
			{
				SurfaceController->RegisterSurfaceRuntimeActor(MoonAnt);
			}
			MoonAnt->FinishSpawning(SpawnTransform);
			ActiveMoonAnts.Add(MoonAnt);
		}
	}

	ScheduleNextMoonAntSpawn();
}

void AJTSMoonAntNestActor::UpdateMoonWrappedLogicalPosition()
{
	if (!IsUsingRealPlanetSurface()
		&& MoonWrappedActorComponent != nullptr
		&& MoonWrappedActorComponent->IsMoonWrappingEnabled())
	{
		MoonWrappedActorComponent->SetLogicalPositionFromWorld();
	}
}
