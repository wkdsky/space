#include "space/World/JTSMoonResourceSpawner.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "space/World/JTSMoonSurfaceGameplayData.h"
#include "space/World/JTSMoonResourceCoverage.h"
#include "space/World/JTSMoonResourcePreviewComponent.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "space/Items/JTSResourceType.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSMoonCorpseActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/World/JTSMoonSurfaceController.h"
#include "space/World/JTSMoonSurfaceGameplaySettings.h"
#include "space/World/JTSMoonAntNestActor.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"

namespace
{
	constexpr float MinimumScaleMultiplier = 0.85f;
	constexpr float MaximumScaleMultiplier = 1.15f;

	float GetSurfaceClearanceRadius(const FBox& Bounds, const float Padding)
	{
		return Bounds.IsValid
			? Bounds.GetExtent().Size() + FMath::Max(0.0f, Padding)
			: FMath::Max(0.0f, Padding);
	}
}

AJTSMoonResourceSpawner::AJTSMoonResourceSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	ResourceActorClass = AJTSMoonResourceActor::StaticClass();
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
#if WITH_EDITORONLY_DATA
	CoverageOutline = CreateEditorOnlyDefaultSubobject<UJTSMoonResourcePreviewComponent>(TEXT("CoverageOutline"));
	CoverageOutline->SetupAttachment(SceneRoot);
	CoverageOutline->bIsEditorOnly = true;
	CoverageOutline->PrimaryComponentTick.bCanEverTick = false;
	CoverageOutline->SetHiddenInGame(true);
	CoverageOutline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoverageOutline->SetGenerateOverlapEvents(false);
#endif
}

void AJTSMoonResourceSpawner::BeginPlay()
{
	Super::BeginPlay();
#if WITH_EDITORONLY_DATA
	if (IsValid(CoverageOutline))
	{
		CoverageOutline->Flush();
		CoverageOutline->SetVisibility(false, true);
		CoverageOutline->SetHiddenInGame(true);
	}
#endif

	// The active MoonSurfaceController applies settings, landmarks, and its PlanetAnchor before generation.
}

void AJTSMoonResourceSpawner::ApplyMoonSpawnSettings(const FJTSMoonResourceSpawnSettings& Settings)
{
	if (!bOverrideCoverageRadius) Radius = FMath::Max(0.0f, Settings.SpawnRadius);
	MinimumResourceSpacing = FMath::Max(0.0f, Settings.MinimumResourceSpacing);
	MediumMetalYieldUnits = FMath::Max(1, Settings.MediumMetalYieldUnits);
	MediumCrystalYieldUnits = FMath::Max(1, Settings.MediumCrystalYieldUnits);
	LargeCrystalYieldUnits = FMath::Max(1, Settings.LargeCrystalYieldUnits);
	ResourceRatios.SetEnabledKinds(Settings.ResourceKinds);
	ResourceRatios.Normalize();
	ResourceCount = GetTargetResourceCount();
	SpacecraftExclusionPadding = FMath::Max(0.0f, Settings.SpacecraftExclusionPadding);
	LandmarkExclusionPadding = FMath::Max(0.0f, Settings.LandmarkExclusionPadding);
	bUseRandomSeed = !Settings.bUseDeterministicSeed;
	RandomSeed = Settings.RandomSeed;
}

void AJTSMoonResourceSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	// Remove serialized previews from the previous spline implementation during asset migration.
	TInlineComponentArray<USplineComponent*> LegacyPreviews(this);
	for (USplineComponent* Preview : LegacyPreviews)
	{
		if (Preview->GetFName() == TEXT("CoveragePreview")) Preview->DestroyComponent();
	}
#endif
	RefreshResourceConfiguration();
}

void AJTSMoonResourceSpawner::RefreshResourceConfiguration()
{
	if (IsValid(ResourceGameplayData)) ApplyMoonSpawnSettings(ResourceGameplayData->GetMoonResourceSpawnSettings());
	else ResourceRatios.Normalize();
	ResourceDensity = FMath::IsFinite(ResourceDensity) ? FMath::Max(0.0f, ResourceDensity) : 0;
	ResourceCount = GetTargetResourceCount();
	UpdateCoveragePreview();
}

float AJTSMoonResourceSpawner::GetCoverageAreaSquareMeters() const
{
	const AJTSPlanetAnchor* Planet = OwningPlanet.IsValid() ? OwningPlanet.Get() : CoveragePlanet.Get();
	const double SurfaceRadius = IsValid(Planet) ? Planet->GetApproximateRadius() : 0;
	const double ArcRadius = FMath::IsFinite(Radius) ? FMath::Max(0.0, static_cast<double>(Radius)) : 0;
	// Spherical cap area. The sine form avoids cancellation for small coverage angles.
	const double Sine = SurfaceRadius > 0 ? FMath::Sin(FMath::Min(UE_DOUBLE_PI, ArcRadius / SurfaceRadius) * 0.5) : 0;
	const double AreaCm2 = SurfaceRadius > 0 ? 4 * UE_DOUBLE_PI * SurfaceRadius * SurfaceRadius * Sine * Sine
		: UE_DOUBLE_PI * ArcRadius * ArcRadius;
	return static_cast<float>(AreaCm2 / 10000.0);
}

int32 AJTSMoonResourceSpawner::GetTargetResourceCount() const
{
	const double Target = static_cast<double>(GetCoverageAreaSquareMeters()) * ResourceDensity / 100.0;
	return FMath::IsFinite(Target) ? FMath::RoundToInt(FMath::Clamp(Target, 0.0, 100000.0)) : 0;
}

void AJTSMoonResourceSpawner::UpdateCoveragePreview()
{
#if WITH_EDITOR
	if (!IsValid(CoverageOutline) || !GetWorld()) return;
	CoverageOutline->bIsEditorOnly = true;
	CoverageOutline->Flush();
	if (GetWorld()->IsGameWorld())
	{
		CoverageOutline->SetVisibility(false, true);
		CoverageOutline->SetHiddenInGame(true);
		return;
	}
	CoverageOutline->SetVisibility(true);
	const AJTSPlanetAnchor* Planet = CoveragePlanet.Get();
	const FVector Origin = GetActorLocation();
	const FVector Up = IsValid(Planet) ? Planet->GetRadialUpVector(Origin) : GetActorUpVector();
	FVector TangentX, TangentY;
	Up.FindBestAxisVectors(TangentX, TangentY);
	constexpr int32 SegmentCount = 192;
	TArray<FVector, TInlineAllocator<SegmentCount>> Points;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const double Azimuth = UE_DOUBLE_TWO_PI * Index / SegmentCount;
		const FVector Tangent = TangentX * FMath::Cos(Azimuth) + TangentY * FMath::Sin(Azimuth);
		FVector Point = Origin + Tangent * Radius;
		if (IsValid(Planet))
		{
			const double Angle = FMath::Min(UE_DOUBLE_PI, static_cast<double>(Planet->ArcDistanceToAngleRadians(Radius)));
			const FVector Direction = Up * FMath::Cos(Angle) + Tangent * FMath::Sin(Angle);
			Point = Planet->GetPlanetCenter() + Direction * Planet->GetApproximateRadius();
			FJTSPlanetSurfaceHit Hit;
			if (Planet->ProjectPointToSurface(Point, Hit)) Point = Hit.ImpactPoint + Hit.ImpactNormal * 10;
		}
		Points.Add(Point);
	}
	TArray<FBatchedLine> Lines;
	Lines.Reserve(SegmentCount);
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		Lines.Emplace(Points[Index], Points[(Index + 1) % SegmentCount], FLinearColor(0.02f, 0.5f, 1), -1, 4, SDPG_World);
	}
	CoverageOutline->DrawLines(Lines);
#endif
}

int32 AJTSMoonResourceSpawner::GetCoveragePreviewSegmentCount() const
{
#if WITH_EDITORONLY_DATA
	return IsValid(CoverageOutline) ? CoverageOutline->BatchedLines.Num() : 0;
#else
	return 0;
#endif
}

#if WITH_EDITOR
void AJTSMoonResourceSpawner::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	ResourceRatios.Normalize(Event.GetPropertyName());
	Super::PostEditChangeProperty(Event);
	RefreshResourceConfiguration();
}

void AJTSMoonResourceSpawner::PostEditUndo()
{
	Super::PostEditUndo();
	RefreshResourceConfiguration();
}
#endif

void AJTSMoonResourceSpawner::SetLandmarkExclusions(
	AJTSSpacecraftActor* InSpacecraft,
	const TArray<TWeakObjectPtr<AJTSMoonCorpseActor>>& InCorpseLandmarks,
	const TArray<TWeakObjectPtr<AJTSMoonAntNestActor>>& InMoonAntNestLandmarks)
{
	SpacecraftLandmark = InSpacecraft;
	CorpseLandmarks = InCorpseLandmarks;
	MoonAntNestLandmarks = InMoonAntNestLandmarks;
}

void AJTSMoonResourceSpawner::SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet)
{
	OwningPlanet = InOwningPlanet;
	ResourceCount = GetTargetResourceCount();
}

AJTSPlanetAnchor* AJTSMoonResourceSpawner::GetOwningPlanet() const
{
	return OwningPlanet.Get();
}

void AJTSMoonResourceSpawner::SetSurfaceGameplayController(AJTSMoonSurfaceController* InSurfaceGameplayController)
{
	SurfaceGameplayController = InSurfaceGameplayController;
}

int32 AJTSMoonResourceSpawner::GenerateResources()
{
	if (!HasAuthority() || !GetWorld()) return 0;
	TArray<AJTSMoonResourceSpawner*> Spawners;
	for (TActorIterator<AJTSMoonResourceSpawner> It(GetWorld()); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && It->GetOwningPlanet() == GetOwningPlanet()
			&& It->GetLevel() == GetLevel() && It->SurfaceGameplayController == SurfaceGameplayController)
		{
			Spawners.Add(*It);
		}
	}
	GenerateResourceGroup(Spawners);
	int32 Count = 0;
	for (const auto& Resource : GeneratedResources) if (IsValid(Resource)) ++Count;
	for (const auto& Pickup : GeneratedPickups) if (Pickup.IsValid()) ++Count;
	return Count;
}

int32 AJTSMoonResourceSpawner::GenerateResourceGroup(const TArray<AJTSMoonResourceSpawner*>& Spawners)
{
	TArray<AJTSMoonResourceSpawner*> Ordered;
	for (AJTSMoonResourceSpawner* Spawner : Spawners)
	{
		if (IsValid(Spawner) && Spawner->HasAuthority() && IsValid(Spawner->GetOwningPlanet())) Ordered.AddUnique(Spawner);
	}
	if (Ordered.IsEmpty()) return 0;
	Ordered.Sort([](const AJTSMoonResourceSpawner& A, const AJTSMoonResourceSpawner& B) { return A.GetPathName() < B.GetPathName(); });
	const AJTSPlanetAnchor* Planet = Ordered[0]->GetOwningPlanet();
	Ordered.RemoveAll([Planet](const AJTSMoonResourceSpawner* Spawner) { return Spawner->GetOwningPlanet() != Planet; });
	uint32 TieSeed = 0;
	bool bRandomTieSeed = false;
	for (AJTSMoonResourceSpawner* Spawner : Ordered)
	{
		Spawner->RefreshResourceConfiguration();
		Spawner->ClearGeneratedResources();
		TieSeed = HashCombineFast(TieSeed, GetTypeHash(Spawner->RandomSeed));
		bRandomTieSeed |= Spawner->bUseRandomSeed;
	}
	if (bRandomTieSeed) TieSeed ^= static_cast<uint32>(FPlatformTime::Cycles64());
	FJTSMoonResourceCoverage Coverage(Planet, Ordered, static_cast<int32>(TieSeed));
	int32 Total = 0;
	for (AJTSMoonResourceSpawner* Spawner : Ordered) Total += Spawner->GenerateResourcesInCoverage(Coverage);
	return Total;
}

int32 AJTSMoonResourceSpawner::GenerateResourcesInCoverage(FJTSMoonResourceCoverage& Coverage)
{
	UWorld* const World = GetWorld();
	if (!HasAuthority() || World == nullptr)
	{
		return 0;
	}

	AJTSPlanetAnchor* const Planet = OwningPlanet.Get();
	if (!IsValid(Planet))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon resource generation requires an active Moon PlanetAnchor."));
		return 0;
	}

	const int32 SafeResourceCount = FMath::RoundToInt(FMath::Clamp(
		static_cast<double>(Coverage.GetOwnedAreaSquareMeters(this)) * ResourceDensity / 100.0, 0.0, 100000.0));
	ResourceCount = SafeResourceCount;
	if (SafeResourceCount <= 0)
	{
		return 0;
	}

	const float TotalResourceWeight = ResourceRatios.GetTotalPercentage();
	if (TotalResourceWeight <= 0) return 0;

	uint64 SeedEntropy = FPlatformTime::Cycles64() ^ static_cast<uint64>(GetUniqueID());
	const int32 Seed = bUseRandomSeed
		? static_cast<int32>(SeedEntropy ^ (SeedEntropy >> 32))
		: RandomSeed;
	FRandomStream RandomStream(Seed);

	const float SafeRadius = FMath::Max(0.0f, Radius);
	const float SafeMinimumSpacing = FMath::Max(0.0f, MinimumResourceSpacing);
	const FVector Origin = GetActorLocation();
	const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
	const IJTSMoonSurfaceGameplaySettings* MoonSettings = IsValid(SurfaceController)
		? SurfaceController->GetMoonSettings()
		: nullptr;
	const int32 LargeRockYieldUnits = MoonSettings != nullptr ? MoonSettings->GetLargeRockTotalYieldUnits() : 6;
	const int32 OreDepositYieldUnits = MoonSettings != nullptr ? MoonSettings->GetOreDepositTotalYieldUnits() : 6;
	int32 SpawnedCount = 0;
	int32 RejectedLandmarkCount = 0;
	int32 RejectedSpacingCount = 0;
	int32 CandidateAttemptCount = 0;
	const int32 MaxCandidateAttempts = FMath::Max(64, GetTargetResourceCount() * 32);

	const auto FindRealSurfaceCandidate = [Planet, Origin, SafeRadius, &RandomStream](FVector& OutCandidateLocation)
	{
		if (!IsValid(Planet))
		{
			return false;
		}

		const FVector CapCenterDirection = Planet->GetRadialUpVector(Origin);
		const float MaximumAngle = FMath::Min(UE_PI, Planet->ArcDistanceToAngleRadians(SafeRadius));
		const float CosineOfAngle = FMath::Lerp(1.0f, FMath::Cos(MaximumAngle), RandomStream.FRand());
		const float SineOfAngle = FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square(CosineOfAngle)));
		const float Azimuth = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		FVector TangentX;
		FVector TangentY;
		CapCenterDirection.FindBestAxisVectors(TangentX, TangentY);
		const FVector CandidateDirection = (
			CapCenterDirection * CosineOfAngle
			+ (TangentX * FMath::Cos(Azimuth) + TangentY * FMath::Sin(Azimuth)) * SineOfAngle).GetSafeNormal();

		FJTSPlanetSurfaceHit SurfaceHit;
		if (!Planet->ProjectPointToSurface(Planet->GetPlanetCenter() + CandidateDirection * Planet->GetApproximateRadius(), SurfaceHit))
		{
			return false;
		}

		OutCandidateLocation = SurfaceHit.ImpactPoint;
		return true;
	};

	while (SpawnedCount < SafeResourceCount && CandidateAttemptCount < MaxCandidateAttempts)
	{
		++CandidateAttemptCount;
		FVector CandidateLocation;
		if (!FindRealSurfaceCandidate(CandidateLocation))
		{
			continue;
		}
		if (Coverage.ResolveOwner(CandidateLocation) != this) continue;
		if (IsCandidateExcludedByLandmarks(CandidateLocation))
		{
			++RejectedLandmarkCount;
			continue;
		}

		FVector GroundLocation;
		if (!ResolveGroundLocation(CandidateLocation, GroundLocation))
		{
			continue;
		}
		if (Coverage.ResolveOwner(GroundLocation) != this) continue;

		if (!Coverage.CanPlace(GroundLocation, SafeMinimumSpacing))
		{
			++RejectedSpacingCount;
			continue;
		}

		const float ResourceRoll = RandomStream.FRand() * TotalResourceWeight;
		float Threshold = 0;
		EJTSMoonResourceKind Kind = EJTSMoonResourceKind::SmallRock;
		for (int32 Index = 0; Index < static_cast<int32>(EJTSMoonResourceKind::Count); ++Index)
		{
			const auto CandidateKind = static_cast<EJTSMoonResourceKind>(Index);
			const float Percentage = ResourceRatios.GetPercentage(CandidateKind);
			if (Percentage <= 0) continue;
			Kind = CandidateKind;
			Threshold += Percentage;
			if (ResourceRoll < Threshold) break;
		}
		const bool bMiningNode = Kind != EJTSMoonResourceKind::SmallRock;
		const bool bLarge = Kind == EJTSMoonResourceKind::LargeRock || Kind == EJTSMoonResourceKind::LargeMetalRock
			|| Kind == EJTSMoonResourceKind::LargeCrystalRock;
		const FVector BaseResourceScale(bMiningNode ? (bLarge ? 2.0f : 1.0f) : 0.5f);
		const int32 TotalYieldUnits = bLarge ? LargeRockYieldUnits : 2;
		const int32 MetalYieldUnits = Kind == EJTSMoonResourceKind::MediumMetalRock ? MediumMetalYieldUnits
			: Kind == EJTSMoonResourceKind::LargeMetalRock ? OreDepositYieldUnits
			: Kind == EJTSMoonResourceKind::MediumCrystalRock ? MediumCrystalYieldUnits
			: Kind == EJTSMoonResourceKind::LargeCrystalRock ? LargeCrystalYieldUnits : 0;
		const EJTSResourceType ResourceType = EJTSResourceType::Rock;
		EJTSMoonResourceNodeSize NodeSize = EJTSMoonResourceNodeSize::MediumRock;
		switch (Kind)
		{
		case EJTSMoonResourceKind::LargeRock: NodeSize = EJTSMoonResourceNodeSize::LargeRock; break;
		case EJTSMoonResourceKind::MediumMetalRock: NodeSize = EJTSMoonResourceNodeSize::MediumMetalRock; break;
		case EJTSMoonResourceKind::LargeMetalRock: NodeSize = EJTSMoonResourceNodeSize::LargeMetalRock; break;
		case EJTSMoonResourceKind::MediumCrystalRock: NodeSize = EJTSMoonResourceNodeSize::MediumCrystalRock; break;
		case EJTSMoonResourceKind::LargeCrystalRock: NodeSize = EJTSMoonResourceNodeSize::LargeCrystalRock; break;
		default: break;
		}

		const FVector ResourceScale(
			BaseResourceScale.X * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Y * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Z * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier));
		FRotator ResourceRotation(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (Planet->GetSurfaceFrameAt(GroundLocation, FVector::ForwardVector, SurfaceFrame))
		{
			const float YawRadians = FMath::DegreesToRadians(ResourceRotation.Yaw);
			const FVector SurfaceForward = FQuat(SurfaceFrame.Up, YawRadians).RotateVector(SurfaceFrame.Forward);
			ResourceRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceFrame.Up).Rotator();
		}
		bool bSpawnedResourceAtCandidate = false;
		if (bMiningNode)
		{
			if (AJTSMoonResourceActor* const Resource = SpawnMiningNode(
				ResourceType,
				TotalYieldUnits,
				NodeSize,
				MetalYieldUnits,
				RandomStream.RandRange(0, 255),
				ResourceScale,
				ResourceRotation,
				GroundLocation))
			{
				++SpawnedCount;
				bSpawnedResourceAtCandidate = true;
			}
		}
		else
		{
			// Small rocks are intentionally loose world pickups: no mining tool is required.
			if (SpawnInitialPickup(GroundLocation) != nullptr)
			{
				++SpawnedCount;
				bSpawnedResourceAtCandidate = true;
			}
		}

		if (bSpawnedResourceAtCandidate)
		{
			Coverage.RecordPlacement(GroundLocation, SafeMinimumSpacing);
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Spawn: Total=%d RejectedLandmarks=%d RejectedSpacing=%d Seed=%d"),
		SpawnedCount,
		RejectedLandmarkCount,
		RejectedSpacingCount,
		Seed);

	return SpawnedCount;
}

void AJTSMoonResourceSpawner::ClearGeneratedResources()
{
	for (TObjectPtr<AJTSMoonResourceActor>& Resource : GeneratedResources)
	{
		if (IsValid(Resource))
		{
			Resource->Destroy();
		}
	}
	GeneratedResources.Reset();

	for (TWeakObjectPtr<AJTSWorldPickupActor>& Pickup : GeneratedPickups)
	{
		if (Pickup.IsValid())
		{
			Pickup->Destroy();
		}
	}
	GeneratedPickups.Reset();
}

AJTSMoonResourceActor* AJTSMoonResourceSpawner::SpawnMiningNode(
	EJTSResourceType ResourceType,
	int32 TotalYieldUnits,
	EJTSMoonResourceNodeSize NodeSize,
	int32 MetalYieldUnits,
	int32 VisualVariantIndex,
	const FVector& ResourceScale,
	const FRotator& ResourceRotation,
	const FVector& GroundLocation)
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSMoonResourceActor> SpawnClass = ResourceActorClass;
	if (SpawnClass == nullptr)
	{
		SpawnClass = AJTSMoonResourceActor::StaticClass();
	}

	const FVector SafeScale(
		FMath::Max(0.01f, ResourceScale.X),
		FMath::Max(0.01f, ResourceScale.Y),
		FMath::Max(0.01f, ResourceScale.Z));
	const FTransform SpawnTransform(ResourceRotation, GroundLocation, SafeScale);
	AJTSMoonResourceActor* const Resource = World->SpawnActorDeferred<AJTSMoonResourceActor>(
		SpawnClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Resource))
	{
		return nullptr;
	}

	Resource->InitializeMiningNode(ResourceType, TotalYieldUnits, NodeSize, MetalYieldUnits, VisualVariantIndex);
	Resource->FinishSpawning(SpawnTransform);
	if (AJTSPlanetAnchor* const Planet = OwningPlanet.Get())
	{
		Resource->PlaceOnPlanetSurface(Planet, GroundLocation, ResourceRotation.Vector());
	}
	if (AJTSMoonSurfaceController* const Controller = SurfaceGameplayController.Get())
	{
		Controller->RegisterSurfaceRuntimeActor(Resource);
	}
	GeneratedResources.Add(Resource);
	return Resource;
}

AJTSWorldPickupActor* AJTSMoonResourceSpawner::SpawnInitialPickup(const FVector& GroundLocation)
{
	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnInitialGroundedPickup(
		GetWorld(),
		EJTSWorldPickupItemType::Rock,
		GroundLocation,
		this);
	if (IsValid(Pickup))
	{
		if (AJTSMoonSurfaceController* const Controller = SurfaceGameplayController.Get())
		{
			Controller->RegisterSurfaceRuntimeActor(Pickup);
		}
		GeneratedPickups.Add(Pickup);
	}
	return Pickup;
}

bool AJTSMoonResourceSpawner::ResolveGroundLocation(const FVector& CandidateLocation, FVector& OutGroundLocation) const
{
	if (AJTSPlanetAnchor* const Planet = OwningPlanet.Get())
	{
		const FVector RadialUp = Planet->GetRadialUpVector(CandidateLocation);
		FJTSPlanetSurfaceHit SurfaceHit;
		const float ProbeDistance = FMath::Max(0.0f, GroundTraceStartHeight) + FMath::Max(0.0f, GroundTraceDistance);
		if (Planet->ProbeSurfaceAlongGravity(
			CandidateLocation + RadialUp * FMath::Max(0.0f, GroundTraceStartHeight),
			ProbeDistance,
			SurfaceHit)
			|| Planet->ProjectPointToSurface(CandidateLocation, SurfaceHit))
		{
			OutGroundLocation = SurfaceHit.ImpactPoint;
			return true;
		}
		return false;
	}
	return false;
}

bool AJTSMoonResourceSpawner::IsCandidateExcludedByLandmarks(const FVector& CandidateLocation) const
{
	if (AJTSPlanetAnchor* const Planet = OwningPlanet.Get())
	{
		auto IsWithinSurfaceClearance = [Planet, &CandidateLocation](const AActor* Landmark, const float Padding)
		{
			if (!IsValid(Landmark))
			{
				return false;
			}

			return Planet->ApproximateSurfaceArcDistance(CandidateLocation, Landmark->GetActorLocation())
				<= GetSurfaceClearanceRadius(Landmark->GetComponentsBoundingBox(true), Padding);
		};

		if (IsWithinSurfaceClearance(SpacecraftLandmark.Get(), SpacecraftExclusionPadding))
		{
			return true;
		}
		for (const TWeakObjectPtr<AJTSMoonCorpseActor>& Corpse : CorpseLandmarks)
		{
			if (IsWithinSurfaceClearance(Corpse.Get(), LandmarkExclusionPadding))
			{
				return true;
			}
		}
		for (const TWeakObjectPtr<AJTSMoonAntNestActor>& Nest : MoonAntNestLandmarks)
		{
			if (IsWithinSurfaceClearance(Nest.Get(), LandmarkExclusionPadding))
			{
				return true;
			}
		}
		return false;
	}
	return false;
}

bool AJTSMoonResourceSpawner::IsUsingRealPlanetSurface() const
{
	return OwningPlanet.IsValid();
}
