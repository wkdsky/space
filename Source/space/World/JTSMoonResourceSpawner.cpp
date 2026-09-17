#include "space/World/JTSMoonResourceSpawner.h"

#include "Engine/World.h"
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
}

void AJTSMoonResourceSpawner::BeginPlay()
{
	Super::BeginPlay();

	// The active MoonSurfaceController applies settings, landmarks, and its PlanetAnchor before generation.
}

void AJTSMoonResourceSpawner::ApplyMoonSpawnSettings(const FJTSMoonResourceSpawnSettings& Settings)
{
	ResourceCount = FMath::Max(0, Settings.TotalResourceCount);
	Radius = FMath::Max(0.0f, Settings.SpawnRadius);
	MinimumResourceSpacing = FMath::Max(0.0f, Settings.MinimumResourceSpacing);
	SmallRockWeight = FMath::Max(0, Settings.SmallRockWeight);
	MediumRockWeight = FMath::Max(0, Settings.MediumRockWeight);
	LargeRockWeight = FMath::Max(0, Settings.LargeRockWeight);
	OreWeight = FMath::Max(0, Settings.OreWeight);
	SpacecraftExclusionPadding = FMath::Max(0.0f, Settings.SpacecraftExclusionPadding);
	LandmarkExclusionPadding = FMath::Max(0.0f, Settings.LandmarkExclusionPadding);
	bUseRandomSeed = !Settings.bUseDeterministicSeed;
	RandomSeed = Settings.RandomSeed;
}

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
}

void AJTSMoonResourceSpawner::SetSurfaceGameplayController(AJTSMoonSurfaceController* InSurfaceGameplayController)
{
	SurfaceGameplayController = InSurfaceGameplayController;
}

int32 AJTSMoonResourceSpawner::GenerateResources()
{
	UWorld* const World = GetWorld();
	if (!HasAuthority() || World == nullptr)
	{
		return 0;
	}

	ClearGeneratedResources();
	AJTSPlanetAnchor* const Planet = OwningPlanet.Get();
	if (!IsValid(Planet))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon resource generation requires an active Moon PlanetAnchor."));
		return 0;
	}

	const int32 SafeResourceCount = FMath::Max(0, ResourceCount);
	if (SafeResourceCount <= 0)
	{
		return 0;
	}

	const int32 SafeSmallRockWeight = FMath::Max(0, SmallRockWeight);
	const int32 SafeMediumRockWeight = FMath::Max(0, MediumRockWeight);
	const int32 SafeLargeRockWeight = FMath::Max(0, LargeRockWeight);
	const int32 SafeOreWeight = FMath::Max(0, OreWeight);
	const int64 TotalResourceWeight = static_cast<int64>(SafeSmallRockWeight)
		+ static_cast<int64>(SafeMediumRockWeight)
		+ static_cast<int64>(SafeLargeRockWeight)
		+ static_cast<int64>(SafeOreWeight);
	if (TotalResourceWeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("JumpToSpace Moon Resource Spawn Skipped: all resource weights are zero or negative."));
		return 0;
	}

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
	const int32 MaxCandidateAttempts = FMath::Max(64, SafeResourceCount * 32);
	TArray<FVector> AcceptedResourceLocations;

	const auto FindRealSurfaceCandidate = [Planet, Origin, SafeRadius, &RandomStream](FVector& OutCandidateLocation)
	{
		if (!IsValid(Planet))
		{
			return false;
		}

		const FVector CapCenterDirection = Planet->GetRadialUpVector(Origin);
		const float MaximumAngle = Planet->ArcDistanceToAngleRadians(SafeRadius);
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

		if (SafeMinimumSpacing > 0.0f)
		{
			const bool bTooCloseToExistingResource = AcceptedResourceLocations.ContainsByPredicate(
				[Planet, &GroundLocation, SafeMinimumSpacing](const FVector& ExistingLocation)
				{
					return Planet->ApproximateSurfaceArcDistance(ExistingLocation, GroundLocation) < SafeMinimumSpacing;
				});
			if (bTooCloseToExistingResource)
			{
				++RejectedSpacingCount;
				continue;
			}
		}

		const double ResourceRoll = static_cast<double>(RandomStream.FRand()) * static_cast<double>(TotalResourceWeight);
		const double MediumRockThreshold = static_cast<double>(SafeSmallRockWeight)
			+ static_cast<double>(SafeMediumRockWeight);
		const double LargeRockThreshold = MediumRockThreshold + static_cast<double>(SafeLargeRockWeight);
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		FVector BaseResourceScale(0.5f);
		bool bMiningNode = false;
		int32 TotalYieldUnits = 0;
		EJTSMoonResourceNodeSize NodeSize = EJTSMoonResourceNodeSize::MediumRock;
		if (ResourceRoll < static_cast<double>(SafeSmallRockWeight))
		{
			BaseResourceScale = FVector(0.5f);
		}
		else if (ResourceRoll < MediumRockThreshold)
		{
			BaseResourceScale = FVector(1.0f);
			bMiningNode = true;
			TotalYieldUnits = 2;
			NodeSize = EJTSMoonResourceNodeSize::MediumRock;
		}
		else if (ResourceRoll < LargeRockThreshold)
		{
			BaseResourceScale = FVector(2.0f);
			bMiningNode = true;
			TotalYieldUnits = LargeRockYieldUnits;
			NodeSize = EJTSMoonResourceNodeSize::LargeRock;
		}
		else
		{
			ResourceType = EJTSResourceType::Ore;
			BaseResourceScale = FVector(1.2f, 1.2f, 1.8f);
			bMiningNode = true;
			TotalYieldUnits = OreDepositYieldUnits;
			NodeSize = EJTSMoonResourceNodeSize::OreVein;
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
			AcceptedResourceLocations.Add(GroundLocation);
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

	Resource->InitializeMiningNode(ResourceType, TotalYieldUnits, NodeSize);
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
