#include "space/World/JTSMoonAntNestActor.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSNestEntranceMeshComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/World/JTSMoonAntActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSurfacePlacementBounds.h"
#include "UObject/ConstructorHelpers.h"

AJTSMoonAntNestActor::AJTSMoonAntNestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

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

	EntranceShape = CreateDefaultSubobject<UJTSNestEntranceMeshComponent>(TEXT("EntranceShape"));
	EntranceShape->SetupAttachment(NestMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		NestMesh->SetStaticMesh(SphereMeshAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterialAsset.Succeeded())
	{
		NestMesh->SetMaterial(0, BasicMaterialAsset.Object);
	}
}

void AJTSMoonAntNestActor::AdjustToGround(const FVector& GroundLocation)
{
	if (!HasAuthority())
	{
		return;
	}
	bUsesRealPlanetSurface = false;
	SurfacePlanet = nullptr;
	SurfaceUp = FVector::UpVector;
	const FVector Extent = GetVisualBoundsExtent();
	SetActorLocation(
		FVector(GroundLocation.X, GroundLocation.Y, GroundLocation.Z + Extent.Z + 2.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void AJTSMoonAntNestActor::PlaceOnPlanetSurface(
	AJTSPlanetAnchor* Planet,
	const FVector& GroundLocation,
	const FVector& PreferredForward)
{
	if (!HasAuthority())
	{
		return;
	}
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
	UPrimitiveComponent* const Visual = GetNestVisual();
	Visual->UpdateBounds();
	FJTSSurfaceVisualProjectionBounds VisualBounds;
	const float SurfaceSupport = JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
		Visual,
		GetActorLocation(),
		SurfaceUp,
		VisualBounds)
		? VisualBounds.GetRootToLowestSupport()
		: 0.0f;
	SetActorLocation(
		SurfaceFrame.Location + SurfaceUp * (SurfaceSupport + JTSSurfacePlacementBounds::DefaultSurfaceClearance),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void AJTSMoonAntNestActor::SetMoonAntNestVisualScale(float InVisualScale)
{
	NestVisualScale = FMath::Max(0.1f, InVisualScale);
	if (!IsValid(NestMesh))
	{
		return;
	}

	NestMesh->SetRelativeScale3D(BaseMoonAntNestMeshScale * NestVisualScale);
	if (NestMesh->IsRegistered())
	{
		NestMesh->UpdateBounds();
	}
}

void AJTSMoonAntNestActor::SetMoonAntActorClass(TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass)
{
	if (!HasAuthority())
	{
		return;
	}
	MoonAntActorClass = InMoonAntActorClass;
}

bool AJTSMoonAntNestActor::ActivateAuthoredPlanetNest(
	AJTSPlanetAnchor* Planet,
	TSubclassOf<AJTSMoonAntActor> InMoonAntActorClass)
{
	if (!HasAuthority() || !IsValid(Planet) || !Planet->HasGameplaySurface())
	{
		return false;
	}
	SetMoonAntActorClass(InMoonAntActorClass);
	SurfacePlanet = Planet;
	bUsesRealPlanetSurface = true;
	SurfaceUp = GetActorUpVector().GetSafeNormal();
	StartSurfaceActivity();
	ForceNetUpdate();
	return true;
}

void AJTSMoonAntNestActor::DeactivateSurfaceNest()
{
	if (!HasAuthority())
	{
		return;
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoonAntSpawnTimerHandle);
	}
	bSurfaceActivityStarted = false;
	const auto AntsToRemove = MoveTemp(ActiveMoonAnts);
	for (const TWeakObjectPtr<AJTSMoonAntActor>& Ant : AntsToRemove)
	{
		if (Ant.IsValid()) Ant->Destroy();
	}
	ActiveMoonAnts.Reset();
	bSurfaceActivityStarted = false;
}

int32 AJTSMoonAntNestActor::GetActiveMoonAntCount() const
{
	int32 Count = 0;
	for (const auto& Ant : ActiveMoonAnts)
	{
		if (Ant.IsValid() && !Ant->IsActorBeingDestroyed()
			&& IsValid(Ant->GetHealthComponent()) && !Ant->GetHealthComponent()->IsDead()) ++Count;
	}
	return Count;
}

float AJTSMoonAntNestActor::GetPopulationSpawnInterval() const
{
	const auto* Settings = GetMoonGameMode();
	if (!Settings) return 1.0f;
	const float Fullness = FMath::Clamp(static_cast<float>(GetActiveMoonAntCount())
		/ FMath::Max(1, Settings->GetMaxActiveMoonAntsPerNest()), 0.0f, 1.0f);
	return FMath::Lerp(Settings->GetMoonAntSpawnIntervalMin(), Settings->GetMoonAntSpawnIntervalMax(),
		Fullness * Fullness * Fullness);
}

void AJTSMoonAntNestActor::NotifyMoonAntEnded(AJTSMoonAntActor* Ant)
{
	if (!HasAuthority()) return;
	ActiveMoonAnts.RemoveAll([Ant](const auto& Existing) { return !Existing.IsValid() || Existing.Get() == Ant; });
	if (bMaintainPopulation && bSurfaceActivityStarted && !IsActorBeingDestroyed())
	{
		// Shorten a pending slow refill after losses; repeated notifications must not postpone it forever.
		const float Delay = GetPopulationSpawnInterval();
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(MoonAntSpawnTimerHandle);
		if (Remaining < 0 || Remaining > Delay)
			GetWorld()->GetTimerManager().SetTimer(MoonAntSpawnTimerHandle, this,
				&AJTSMoonAntNestActor::TrySpawnMoonAnt, Delay, false);
	}
}

bool AJTSMoonAntNestActor::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const
{
	return IsValid(AttackingPawn) && PunchHitsRemaining > 0 && !IsPendingKillPending();
}

void AJTSMoonAntNestActor::ReceiveMeleeHit_Implementation(APawn* AttackingPawn, EJTSMeleeAttackType AttackType)
{
	if (!HasAuthority() || !CanReceiveMeleeHit_Implementation(AttackingPawn))
	{
		return;
	}

	if (AttackType == EJTSMeleeAttackType::Knife || AttackType == EJTSMeleeAttackType::Axe
		|| AttackType == EJTSMeleeAttackType::Tool)
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
	UPrimitiveComponent* const Visual = GetNestVisual();
	if (IsValid(Visual) && Visual->IsRegistered())
	{
		if (IsUsingRealPlanetSurface())
		{
			FJTSSurfaceVisualProjectionBounds VisualBounds;
			if (JTSSurfacePlacementBounds::AccumulateVisualProjectionBounds(
				Visual,
				GetActorLocation(),
				SurfaceUp,
				VisualBounds))
			{
				return VisualBounds.HighestPoint + SurfaceUp * 24.0f;
			}
		}

		const float BoundsScale = FMath::Max(FMath::Abs(Visual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = Visual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		return Visual->Bounds.Origin + SurfaceUp * (PhysicalExtent.Z + 24.0f);
	}

	return GetActorLocation() + SurfaceUp * 55.0f;
}

void AJTSMoonAntNestActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EntranceShape->RebuildEntrance();
}

void AJTSMoonAntNestActor::BeginPlay()
{
	Super::BeginPlay();
	MoonAntNestMaterial = NestMesh != nullptr ? NestMesh->CreateAndSetMaterialInstanceDynamic(0) : nullptr;
	if (MoonAntNestMaterial != nullptr)
	{
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("Color"), NestTint);
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("BaseColor"), NestTint);
		MoonAntNestMaterial->SetVectorParameterValue(TEXT("Tint"), NestTint);
	}
	// Placed entrances may begin play before the GameMode supplies surface settings.
	// The surface controller activates them once its existing context is ready.
	StartSurfaceActivity();
}

void AJTSMoonAntNestActor::StartSurfaceActivity()
{
	if (!HasAuthority() || !HasActorBegunPlay() || bSurfaceActivityStarted)
	{
		return;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonGameMode = GetMoonGameMode();
	if (MoonGameMode == nullptr)
	{
		return;
	}
	PunchHitsRemaining = MoonGameMode->GetMoonAntNestPunchHitsToDestroy();
	bSurfaceActivityStarted = true;
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
	DeactivateSurfaceNest();
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
	if (IsValid(SurfacePlanet))
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
	UPrimitiveComponent* const Visual = GetNestVisual();
	if (IsValid(Visual) && Visual->IsRegistered())
	{
		const float BoundsScale = FMath::Max(FMath::Abs(Visual->BoundsScale), KINDA_SMALL_NUMBER);
		const FVector PhysicalExtent = Visual->Bounds.BoxExtent.GetAbs() / BoundsScale;
		if (!PhysicalExtent.IsNearlyZero())
		{
			return PhysicalExtent;
		}
	}

	return FVector(34.0f, 34.0f, 10.0f);
}

UPrimitiveComponent* AJTSMoonAntNestActor::GetNestVisual() const
{
	return IsValid(EntranceShape) && EntranceShape->bEnabled
		? static_cast<UPrimitiveComponent*>(EntranceShape.Get())
		: static_cast<UPrimitiveComponent*>(NestMesh.Get());
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
	if (!HasAuthority())
	{
		return;
	}
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
		bMaintainPopulation ? GetPopulationSpawnInterval() * FMath::FRandRange(0.9f, 1.1f)
			: FMath::FRandRange(MoonGameMode->GetMoonAntSpawnIntervalMin(), MoonGameMode->GetMoonAntSpawnIntervalMax()),
		false);
}

void AJTSMoonAntNestActor::TrySpawnMoonAnt()
{
	if (!HasAuthority())
	{
		return;
	}
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
	if (GetActiveMoonAntCount() >= MoonGameMode->GetMaxActiveMoonAntsPerNest()
		|| (!bMaintainPopulation && FMath::FRand() > MoonGameMode->GetMoonAntSpawnChance()))
	{
		ScheduleNextMoonAntSpawn();
		return;
	}

	AJTSPlanetAnchor* const Planet = GetSurfacePlanet();
	if (!IsValid(Planet))
	{
		ScheduleNextMoonAntSpawn();
		return;
	}

	FJTSPlanetSurfaceFrame NestSurfaceFrame;
	FJTSPlanetSurfaceHit SpawnSurfaceHit;
	if (Planet->GetSurfaceFrameAt(GetActorLocation(), GetActorForwardVector(), NestSurfaceFrame))
	{
		const AJTSMoonAntActor* Defaults = MoonAntActorClass
			? MoonAntActorClass->GetDefaultObject<AJTSMoonAntActor>() : GetDefault<AJTSMoonAntActor>();
		FVector SpawnDirection = FVector::ZeroVector;
		bool bFoundPoint = false;
		// Keep the selected density band while rejecting the steep wall half of a crater-foot nest.
		const float SpawnDistance = ChooseMoonAntSpawnDistance(*MoonGameMode);
		for (int32 Attempt = 0; Attempt < 16 && !bFoundPoint; ++Attempt)
		{
			const float SpawnAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
			SpawnDirection = (NestSurfaceFrame.Forward * FMath::Cos(SpawnAngle)
				+ NestSurfaceFrame.Right * FMath::Sin(SpawnAngle)).GetSafeNormal();
			bFoundPoint = Planet->ProjectPointToSurface(GetActorLocation() + SpawnDirection * SpawnDistance, SpawnSurfaceHit)
				&& Defaults->GetSurfaceSteering()->IsWalkable(Planet, SpawnSurfaceHit);
			// A walkable far endpoint behind the crater wall is not a reachable part of this colony.
			for (float Along = FMath::Min(80.0f, SpawnDistance); bFoundPoint && Along < SpawnDistance; Along += 180.0f)
			{
				FJTSPlanetSurfaceHit RouteHit;
				bFoundPoint = Planet->ProjectPointToSurface(GetActorLocation() + SpawnDirection * Along, RouteHit)
					&& Defaults->GetSurfaceSteering()->IsWalkable(Planet, RouteHit);
			}
			if (bFoundPoint)
			{
				for (const auto& Existing : ActiveMoonAnts)
				{
					if (Existing.IsValid() && FVector::DistSquared(Existing->GetActorLocation(), SpawnSurfaceHit.ImpactPoint) < FMath::Square(35.0f))
					{ bFoundPoint = false; break; }
				}
			}
		}
		if (bFoundPoint)
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
				if (IsValid(MoonAnt) && !MoonAnt->IsActorBeingDestroyed()) ActiveMoonAnts.Add(MoonAnt);
			}
		}
	}

	ScheduleNextMoonAntSpawn();
}

void AJTSMoonAntNestActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AJTSMoonAntNestActor, SurfacePlanet);
	DOREPLIFETIME(AJTSMoonAntNestActor, SurfaceUp);
	DOREPLIFETIME(AJTSMoonAntNestActor, NestVisualScale);
	DOREPLIFETIME(AJTSMoonAntNestActor, PunchHitsRemaining);
	DOREPLIFETIME(AJTSMoonAntNestActor, bUsesRealPlanetSurface);
}

void AJTSMoonAntNestActor::OnRep_NestPresentation()
{
	if (IsValid(NestMesh))
	{
		NestMesh->SetRelativeScale3D(BaseMoonAntNestMeshScale * FMath::Max(0.1f, NestVisualScale));
	}
}
