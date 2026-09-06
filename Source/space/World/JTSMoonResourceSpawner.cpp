#include "space/World/JTSMoonResourceSpawner.h"

#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "space/Items/JTSResourceType.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonResourceActor.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"

namespace
{
	constexpr float MinimumScaleMultiplier = 0.85f;
	constexpr float MaximumScaleMultiplier = 1.15f;
}

AJTSMoonResourceSpawner::AJTSMoonResourceSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	ResourceActorClass = AJTSMoonResourceActor::StaticClass();
}

void AJTSMoonResourceSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Moon GameMode applies its Blueprint balance values after all level actors have begun play.
	if (GetWorld() == nullptr || GetWorld()->GetAuthGameMode<AJTSMoonGameMode>() == nullptr)
	{
		GenerateResources();
	}
}

void AJTSMoonResourceSpawner::ApplyMoonSpawnSettings(const FJTSMoonResourceSpawnSettings& Settings)
{
	ResourceCount = FMath::Max(0, Settings.TotalResourceCount);
	Radius = FMath::Max(0.0f, Settings.SpawnRadius);
	SmallRockWeight = FMath::Max(0, Settings.SmallRockWeight);
	MediumRockWeight = FMath::Max(0, Settings.MediumRockWeight);
	LargeRockWeight = FMath::Max(0, Settings.LargeRockWeight);
	OreWeight = FMath::Max(0, Settings.OreWeight);
	SpacecraftExclusionPadding = FMath::Max(0.0f, Settings.SpacecraftExclusionPadding);
}

int32 AJTSMoonResourceSpawner::GenerateResources()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	ClearGeneratedResources();

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
	const FVector Origin = GetActorLocation();
	const AJTSMoonGameMode* const MoonGameMode = World->GetAuthGameMode<AJTSMoonGameMode>();
	const int32 LargeRockYieldUnits = IsValid(MoonGameMode) ? MoonGameMode->GetLargeRockTotalYieldUnits() : 6;
	const int32 OreDepositYieldUnits = IsValid(MoonGameMode) ? MoonGameMode->GetOreDepositTotalYieldUnits() : 6;
	int32 SpawnedCount = 0;
	int32 RejectedNearShipCount = 0;
	int32 CandidateAttemptCount = 0;
	const int32 MaxCandidateAttempts = FMath::Max(64, SafeResourceCount * 32);

	while (SpawnedCount < SafeResourceCount && CandidateAttemptCount < MaxCandidateAttempts)
	{
		++CandidateAttemptCount;
		const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		const float Distance = SafeRadius * FMath::Sqrt(RandomStream.FRand());
		const FVector CandidateXY = Origin + FVector(
			FMath::Cos(Angle) * Distance,
			FMath::Sin(Angle) * Distance,
			0.0f);
		if (IsCandidateExcludedBySpacecraft(CandidateXY))
		{
			++RejectedNearShipCount;
			continue;
		}

		FVector GroundLocation;
		if (!ResolveGroundLocation(CandidateXY, GroundLocation))
		{
			continue;
		}

		const double ResourceRoll = static_cast<double>(RandomStream.FRand()) * static_cast<double>(TotalResourceWeight);
		const double MediumRockThreshold = static_cast<double>(SafeSmallRockWeight)
			+ static_cast<double>(SafeMediumRockWeight);
		const double LargeRockThreshold = MediumRockThreshold + static_cast<double>(SafeLargeRockWeight);
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		FVector BaseResourceScale(0.5f);
		int32 InitialPickupCount = 1;
		bool bMiningNode = false;
		int32 TotalYieldUnits = 0;
		if (ResourceRoll < static_cast<double>(SafeSmallRockWeight))
		{
			BaseResourceScale = FVector(0.5f);
		}
		else if (ResourceRoll < MediumRockThreshold)
		{
			BaseResourceScale = FVector(1.0f);
			InitialPickupCount = 2;
		}
		else if (ResourceRoll < LargeRockThreshold)
		{
			BaseResourceScale = FVector(2.0f);
			bMiningNode = true;
			TotalYieldUnits = LargeRockYieldUnits;
		}
		else
		{
			ResourceType = EJTSResourceType::Ore;
			BaseResourceScale = FVector(1.2f, 1.2f, 1.8f);
			bMiningNode = true;
			TotalYieldUnits = OreDepositYieldUnits;
		}

		const FVector ResourceScale(
			BaseResourceScale.X * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Y * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Z * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier));
		const FRotator ResourceRotation(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
		if (bMiningNode)
		{
			if (AJTSMoonResourceActor* const Resource = SpawnMiningNode(
				ResourceType,
				TotalYieldUnits,
				ResourceScale,
				ResourceRotation,
				GroundLocation))
			{
				++SpawnedCount;
			}
		}
		else
		{
			int32 SpawnedPickupCount = 0;
			for (int32 PickupIndex = 0; PickupIndex < InitialPickupCount; ++PickupIndex)
			{
				if (SpawnInitialPickup(GroundLocation) != nullptr)
				{
					++SpawnedPickupCount;
				}
			}
			if (SpawnedPickupCount == InitialPickupCount)
			{
				++SpawnedCount;
			}
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Spawn: Total=%d RejectedNearShip=%d"),
		SpawnedCount,
		RejectedNearShipCount);

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

	Resource->InitializeMiningNode(ResourceType, TotalYieldUnits);
	Resource->FinishSpawning(SpawnTransform);
	Resource->AdjustToGround(GroundLocation);
	GeneratedResources.Add(Resource);
	return Resource;
}

AJTSWorldPickupActor* AJTSMoonResourceSpawner::SpawnInitialPickup(const FVector& GroundLocation, const FVector& PreferredDirection)
{
	APawn* const PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGroundedPickup(
		GetWorld(),
		EJTSWorldPickupItemType::Rock,
		GroundLocation,
		PlayerPawn,
		this,
		PreferredDirection);
	if (IsValid(Pickup))
	{
		GeneratedPickups.Add(Pickup);
	}
	return Pickup;
}

bool AJTSMoonResourceSpawner::ResolveGroundLocation(const FVector& CandidateXY, FVector& OutGroundLocation) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const float SafeTraceStartHeight = FMath::Max(0.0f, GroundTraceStartHeight);
	const float SafeTraceDistance = FMath::Max(0.0f, GroundTraceDistance);
	const FVector TraceStart(CandidateXY.X, CandidateXY.Y, GetActorLocation().Z + SafeTraceStartHeight);
	const FVector TraceEnd(CandidateXY.X, CandidateXY.Y, GetActorLocation().Z + SafeTraceStartHeight - SafeTraceDistance);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JTSMoonResourceGroundTrace), false, this);
	TraceParams.AddIgnoredActor(this);

	// Ground placement must only see Moon terrain, never gameplay actors or a prior resource node.
	if (APawn* const LocalPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		TraceParams.AddIgnoredActor(LocalPlayerPawn);
	}
	for (TActorIterator<AJTSCharacter> PlayerIt(World); PlayerIt; ++PlayerIt)
	{
		if (IsValid(*PlayerIt))
		{
			TraceParams.AddIgnoredActor(*PlayerIt);
		}
	}
	for (TActorIterator<AJTSSpacecraftActor> SpacecraftIt(World); SpacecraftIt; ++SpacecraftIt)
	{
		if (IsValid(*SpacecraftIt))
		{
			TraceParams.AddIgnoredActor(*SpacecraftIt);
		}
	}
	for (TActorIterator<AJTSMoonResourceActor> ResourceIt(World); ResourceIt; ++ResourceIt)
	{
		if (IsValid(*ResourceIt))
		{
			TraceParams.AddIgnoredActor(*ResourceIt);
		}
	}
	for (const TObjectPtr<AJTSMoonResourceActor>& Resource : GeneratedResources)
	{
		if (IsValid(Resource))
		{
			TraceParams.AddIgnoredActor(Resource.Get());
		}
	}

	FHitResult GroundHit;
	if (SafeTraceDistance > 0.0f
		&& World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams)
		&& GroundHit.bBlockingHit)
	{
		OutGroundLocation = GroundHit.ImpactPoint;
		return true;
	}

	return false;
}

bool AJTSMoonResourceSpawner::IsCandidateExcludedBySpacecraft(const FVector& CandidateXY) const
{
	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const UJTSMoonWrapSubsystem* const WrapSubsystem = World->GetSubsystem<UJTSMoonWrapSubsystem>();
	const FVector2D CandidatePosition(CandidateXY.X, CandidateXY.Y);
	const float SafePadding = FMath::Max(0.0f, SpacecraftExclusionPadding);
	for (TActorIterator<AJTSSpacecraftActor> SpacecraftIt(World); SpacecraftIt; ++SpacecraftIt)
	{
		const AJTSSpacecraftActor* const Spacecraft = *SpacecraftIt;
		if (!IsValid(Spacecraft))
		{
			continue;
		}

		const FBox ShipBounds = Spacecraft->GetResourceExclusionBounds();
		if (!ShipBounds.IsValid)
		{
			continue;
		}

		const FVector ShipCenter = ShipBounds.GetCenter();
		const FVector ShipExtent = ShipBounds.GetExtent();
		const FVector2D ShipCenterXY(ShipCenter.X, ShipCenter.Y);
		const FVector2D RelativeCandidate = WrapSubsystem != nullptr && WrapSubsystem->IsConfiguredForMoon()
			? WrapSubsystem->ShortestWrappedDelta2D(ShipCenterXY, CandidatePosition)
			: CandidatePosition - ShipCenterXY;
		if (FMath::Abs(RelativeCandidate.X) <= ShipExtent.X + SafePadding
			&& FMath::Abs(RelativeCandidate.Y) <= ShipExtent.Y + SafePadding)
		{
			return true;
		}
	}

	return false;
}
