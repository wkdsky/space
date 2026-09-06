#include "space/World/JTSMoonResourceSpawner.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "space/Items/JTSResourceType.h"
#include "space/World/JTSMoonResourceActor.h"

namespace
{
	constexpr int32 SmallRockWeight = 55;
	constexpr int32 MediumRockWeight = 20;
	constexpr int32 LargeRockWeight = 10;
	constexpr int32 OreWeight = 15;
	constexpr int32 TotalResourceWeight = SmallRockWeight + MediumRockWeight + LargeRockWeight + OreWeight;
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
	GenerateResources();
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

	uint64 SeedEntropy = FPlatformTime::Cycles64() ^ static_cast<uint64>(GetUniqueID());
	const int32 Seed = bUseRandomSeed
		? static_cast<int32>(SeedEntropy ^ (SeedEntropy >> 32))
		: RandomSeed;
	FRandomStream RandomStream(Seed);

	const float SafeRadius = FMath::Max(0.0f, Radius);
	const FVector Origin = GetActorLocation();
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < SafeResourceCount; ++Index)
	{
		const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
		const float Distance = SafeRadius * FMath::Sqrt(RandomStream.FRand());
		const FVector CandidateXY = Origin + FVector(
			FMath::Cos(Angle) * Distance,
			FMath::Sin(Angle) * Distance,
			0.0f);
		FVector GroundLocation;
		if (!ResolveGroundLocation(CandidateXY, GroundLocation))
		{
			continue;
		}

		const int32 ResourceRoll = RandomStream.RandRange(1, TotalResourceWeight);
		EJTSResourceType ResourceType = EJTSResourceType::Rock;
		FVector BaseResourceScale(0.5f);
		int32 ResourceAmount = 1;
		bool bCanPickup = true;
		if (ResourceRoll <= SmallRockWeight)
		{
			BaseResourceScale = FVector(0.5f);
		}
		else if (ResourceRoll <= SmallRockWeight + MediumRockWeight)
		{
			BaseResourceScale = FVector(1.0f);
			ResourceAmount = 2;
		}
		else if (ResourceRoll <= SmallRockWeight + MediumRockWeight + LargeRockWeight)
		{
			BaseResourceScale = FVector(2.0f);
			bCanPickup = false;
		}
		else
		{
			ResourceType = EJTSResourceType::Ore;
			BaseResourceScale = FVector(1.2f, 1.2f, 1.8f);
			ResourceAmount = 1;
			bCanPickup = false;
		}

		const FVector ResourceScale(
			BaseResourceScale.X * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Y * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier),
			BaseResourceScale.Z * RandomStream.FRandRange(MinimumScaleMultiplier, MaximumScaleMultiplier));
		const FRotator ResourceRotation(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
		if (AJTSMoonResourceActor* const Resource = SpawnResource(
			ResourceType,
			ResourceAmount,
			bCanPickup,
			ResourceScale,
			ResourceRotation,
			FText::GetEmpty(),
			GroundLocation))
		{
			++SpawnedCount;
		}
	}

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
}

AJTSMoonResourceActor* AJTSMoonResourceSpawner::SpawnResource(
	EJTSResourceType ResourceType,
	int32 ResourceAmount,
	bool bCanPickup,
	const FVector& ResourceScale,
	const FRotator& ResourceRotation,
	const FText& ResourcePickupText,
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

	Resource->InitializeResource(ResourceType, ResourceAmount, bCanPickup, ResourcePickupText);
	Resource->FinishSpawning(SpawnTransform);
	Resource->AdjustToGround(GroundLocation);
	GeneratedResources.Add(Resource);
	return Resource;
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
