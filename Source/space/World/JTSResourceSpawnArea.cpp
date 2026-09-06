// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSResourceSpawnArea.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "space/Items/JTSResourcePickupActor.h"
#include "space/Ships/JTSSpacecraftActor.h"

namespace
{
	template <typename ElementType>
	void ShuffleArray(TArray<ElementType>& Array, FRandomStream& RandomStream)
	{
		for (int32 Index = Array.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = RandomStream.RandRange(0, Index);
			if (Index != SwapIndex)
			{
				Swap(Array[Index], Array[SwapIndex]);
			}
		}
	}

	FVector2D MakeRandomizedCellPosition(
		const FVector2D& CellCenter,
		const FVector2D& CellSize,
		FRandomStream& RandomStream)
	{
		const FVector2D JitterRange = CellSize * 0.35f;
		return CellCenter + FVector2D(
			RandomStream.FRandRange(-JitterRange.X, JitterRange.X),
			RandomStream.FRandRange(-JitterRange.Y, JitterRange.Y));
	}
}

AJTSResourceSpawnArea::AJTSResourceSpawnArea()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBox"));
	SetRootComponent(SpawnBox);
	SpawnBox->SetBoxExtent(FVector(1800.0f, 1800.0f, 100.0f));
	SpawnBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBox->SetGenerateOverlapEvents(false);
	SpawnBox->SetHiddenInGame(true);
	SpawnBox->SetVisibility(true);
	SpawnBox->ShapeColor = FColor(40, 180, 255, 80);
	SpawnBox->bDrawOnlyIfSelected = false;
	SpawnBox->SetLineThickness(3.0f);

	ResourcePickupClass = AJTSResourcePickupActor::StaticClass();
}

void AJTSResourceSpawnArea::ApplyEarthSpawnSettings(const FJTSEarthResourceSpawnSettings& Settings)
{
	FuelPickupCount = FMath::Max(0, Settings.FuelPickupCount);
	WaterPickupCount = FMath::Max(0, Settings.WaterPickupCount);
	FoodPickupCount = FMath::Max(0, Settings.FoodPickupCount);
	MinimumPickupSpacing = FMath::Max(0.0f, Settings.MinimumPickupSpacing);
	PlayerExclusionRadius = FMath::Max(0.0f, Settings.PlayerExclusionRadius);
	SpacecraftExclusionRadius = FMath::Max(0.0f, Settings.SpacecraftExclusionRadius);
	EdgePadding = FMath::Max(0.0f, Settings.EdgePadding);

	if (SpawnBox != nullptr)
	{
		const FVector CurrentExtent = SpawnBox->GetUnscaledBoxExtent();
		SpawnBox->SetBoxExtent(
			FVector(
				FMath::Max(0.0f, Settings.SpawnHalfExtentX),
				FMath::Max(0.0f, Settings.SpawnHalfExtentY),
				FMath::Max(0.0f, CurrentExtent.Z)),
			false);
	}
}

int32 AJTSResourceSpawnArea::GenerateResources()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || SpawnBox == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("JTSResourceSpawnArea '%s' could not generate resources because its World or Box is unavailable."), *GetName());
		return 0;
	}

	ClearGeneratedPickups();

	if (bClearExistingResourcePickups)
	{
		TArray<AJTSResourcePickupActor*> ExistingPickups;
		for (TActorIterator<AJTSResourcePickupActor> It(World); It; ++It)
		{
			AJTSResourcePickupActor* const Pickup = *It;
			if (IsValid(Pickup))
			{
				ExistingPickups.Add(Pickup);
			}
		}

		for (AJTSResourcePickupActor* const Pickup : ExistingPickups)
		{
			if (!IsValid(Pickup))
			{
				continue;
			}

			AActor* const PickupOwner = Pickup->GetOwner();
			if (PickupOwner == nullptr || PickupOwner == this)
			{
				Pickup->Destroy();
			}
		}
	}

	const int32 SafeFuelCount = FMath::Max(0, FuelPickupCount);
	const int32 SafeWaterCount = FMath::Max(0, WaterPickupCount);
	const int32 SafeFoodCount = FMath::Max(0, FoodPickupCount);
	const int32 TotalCount = SafeFuelCount + SafeWaterCount + SafeFoodCount;
	if (TotalCount <= 0)
	{
		return 0;
	}

	const FVector BoxExtent = SpawnBox->GetScaledBoxExtent().GetAbs();
	const FTransform SpawnBoxRotationTransform(SpawnBox->GetComponentQuat(), SpawnBox->GetComponentLocation());
	const float SafeEdgePadding = FMath::Max(0.0f, EdgePadding);
	const float AvailableWidth = (BoxExtent.X * 2.0f) - (SafeEdgePadding * 2.0f);
	const float AvailableDepth = (BoxExtent.Y * 2.0f) - (SafeEdgePadding * 2.0f);
	if (AvailableWidth <= 0.0f || AvailableDepth <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSResourceSpawnArea '%s' has no usable XY space after EdgePadding %.1f; requested %d resources, generated 0."), *GetName(), SafeEdgePadding, TotalCount);
		return 0;
	}

	uint64 SeedEntropy = FPlatformTime::Cycles64() ^ static_cast<uint64>(GetUniqueID());
	const int32 Seed = bUseRandomSeed
		? static_cast<int32>(SeedEntropy ^ (SeedEntropy >> 32))
		: RandomSeed;
	FRandomStream RandomStream(Seed);

	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TotalCount))));
	const int32 Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(TotalCount) / static_cast<float>(Columns)));
	const FVector2D CellSize(AvailableWidth / static_cast<float>(Columns), AvailableDepth / static_cast<float>(Rows));

	TArray<FVector2D> CandidateLocalPositions;
	CandidateLocalPositions.Reserve(TotalCount);
	for (int32 RowIndex = 0; RowIndex < Rows; ++RowIndex)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < Columns; ++ColumnIndex)
		{
			const FVector2D CellCenter(
				(-BoxExtent.X + SafeEdgePadding) + (CellSize.X * (static_cast<float>(ColumnIndex) + 0.5f)),
				(-BoxExtent.Y + SafeEdgePadding) + (CellSize.Y * (static_cast<float>(RowIndex) + 0.5f)));
			CandidateLocalPositions.Add(MakeRandomizedCellPosition(CellCenter, CellSize, RandomStream));
		}
	}
	ShuffleArray(CandidateLocalPositions, RandomStream);

	TArray<EJTSResourceType> ResourceTypes;
	ResourceTypes.Reserve(TotalCount);
	for (int32 Index = 0; Index < SafeFuelCount; ++Index)
	{
		ResourceTypes.Add(EJTSResourceType::Fuel);
	}
	for (int32 Index = 0; Index < SafeWaterCount; ++Index)
	{
		ResourceTypes.Add(EJTSResourceType::Water);
	}
	for (int32 Index = 0; Index < SafeFoodCount; ++Index)
	{
		ResourceTypes.Add(EJTSResourceType::Food);
	}
	ShuffleArray(ResourceTypes, RandomStream);

	TArray<APawn*> ValidPlayers;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (IsValid(*It) && !(*It)->IsPendingKillPending())
		{
			ValidPlayers.Add(*It);
		}
	}

	AJTSSpacecraftActor* Spacecraft = nullptr;
	int32 SpacecraftCount = 0;
	for (TActorIterator<AJTSSpacecraftActor> It(World); It; ++It)
	{
		if (!IsValid(*It))
		{
			continue;
		}

		++SpacecraftCount;
		if (Spacecraft == nullptr)
		{
			Spacecraft = *It;
		}
	}
	if (SpacecraftCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSResourceSpawnArea '%s' found %d spacecraft actors; excluding the first valid spacecraft only."), *GetName(), SpacecraftCount);
	}

	const float SafeMinimumSpacing = FMath::Max(0.0f, MinimumPickupSpacing);
	const float SafePlayerExclusion = FMath::Max(0.0f, PlayerExclusionRadius);
	const float SafeSpacecraftExclusion = FMath::Max(0.0f, SpacecraftExclusionRadius);
	const float SafeTraceStart = FMath::Max(0.0f, GroundTraceStartHeight);
	const float SafeTraceDistance = FMath::Max(0.0f, GroundTraceDistance);
	const float MinimumLocalX = -BoxExtent.X + SafeEdgePadding;
	const float MaximumLocalX = BoxExtent.X - SafeEdgePadding;
	const float MinimumLocalY = -BoxExtent.Y + SafeEdgePadding;
	const float MaximumLocalY = BoxExtent.Y - SafeEdgePadding;
	TArray<FVector> AcceptedLocations;
	AcceptedLocations.Reserve(ResourceTypes.Num());

	int32 ResourceTypeIndex = 0;
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateLocalPositions.Num() && ResourceTypeIndex < ResourceTypes.Num(); ++CandidateIndex)
	{
		const FVector2D BaseLocalPosition = CandidateLocalPositions[CandidateIndex];
		bool bPlaced = false;

		for (int32 Attempt = 0; Attempt < 3 && !bPlaced; ++Attempt)
		{
			const FVector2D LocalPosition = Attempt == 0
				? BaseLocalPosition
				: MakeRandomizedCellPosition(BaseLocalPosition, CellSize * 0.65f, RandomStream);
			if (LocalPosition.X < MinimumLocalX || LocalPosition.X > MaximumLocalX
				|| LocalPosition.Y < MinimumLocalY || LocalPosition.Y > MaximumLocalY)
			{
				continue;
			}

			const FVector CandidateWorldXY = SpawnBoxRotationTransform.TransformPosition(FVector(LocalPosition.X, LocalPosition.Y, 0.0f));

			bool bExcluded = false;
			for (const FVector& AcceptedLocation : AcceptedLocations)
			{
				if (SafeMinimumSpacing > 0.0f && FVector::DistSquared2D(CandidateWorldXY, AcceptedLocation) < FMath::Square(SafeMinimumSpacing))
				{
					bExcluded = true;
					break;
				}
			}
			if (bExcluded)
			{
				continue;
			}

			for (const APawn* const PlayerPawn : ValidPlayers)
			{
				if (SafePlayerExclusion > 0.0f && FVector::DistSquared2D(CandidateWorldXY, PlayerPawn->GetActorLocation()) < FMath::Square(SafePlayerExclusion))
				{
					bExcluded = true;
					break;
				}
			}
			if (bExcluded)
			{
				continue;
			}

			if (IsValid(Spacecraft)
				&& SafeSpacecraftExclusion > 0.0f
				&& FVector::DistSquared2D(CandidateWorldXY, Spacecraft->GetActorLocation()) < FMath::Square(SafeSpacecraftExclusion))
			{
				continue;
			}

			const FVector TraceStart(CandidateWorldXY.X, CandidateWorldXY.Y, CandidateWorldXY.Z + SafeTraceStart);
			const FVector TraceEnd(CandidateWorldXY.X, CandidateWorldXY.Y, CandidateWorldXY.Z + SafeTraceStart - SafeTraceDistance);
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(JTSResourceGroundTrace), false, this);
			for (const TObjectPtr<AJTSResourcePickupActor>& ExistingPickup : GeneratedPickups)
			{
				if (IsValid(ExistingPickup.Get()))
				{
					TraceParams.AddIgnoredActor(ExistingPickup.Get());
				}
			}
			for (TActorIterator<AJTSResourcePickupActor> It(World); It; ++It)
			{
				if (IsValid(*It))
				{
					TraceParams.AddIgnoredActor(*It);
				}
			}

			FHitResult GroundHit;
			if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams) || !GroundHit.bBlockingHit)
			{
				continue;
			}

			TSubclassOf<AJTSResourcePickupActor> PickupClass = ResourcePickupClass;
			if (PickupClass == nullptr)
			{
				PickupClass = AJTSResourcePickupActor::StaticClass();
			}
			const FTransform DeferredTransform(FRotator::ZeroRotator, GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 100.0f));
			AJTSResourcePickupActor* const Pickup = World->SpawnActorDeferred<AJTSResourcePickupActor>(PickupClass, DeferredTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!IsValid(Pickup))
			{
				continue;
			}

			Pickup->InitializeResource(ResourceTypes[ResourceTypeIndex]);
			Pickup->FinishSpawning(DeferredTransform);

			const FVector PickupBoundsExtent = Pickup->GetVisualBoundsExtent();

			const FVector FinalLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, PickupBoundsExtent.Z + 2.0f);
			Pickup->SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);

			FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(JTSResourcePlacementOverlap), false, this);
			OverlapParams.AddIgnoredActor(Pickup);
			const FCollisionShape PlacementShape = FCollisionShape::MakeBox(PickupBoundsExtent * 0.98f);
			const bool bBlocked = World->OverlapBlockingTestByChannel(
				FinalLocation,
				FQuat::Identity,
				ECC_Visibility,
				PlacementShape,
				OverlapParams);
			if (bBlocked)
			{
				Pickup->Destroy();
				continue;
			}

			GeneratedPickups.Add(Pickup);
			AcceptedLocations.Add(FinalLocation);
			++ResourceTypeIndex;
			bPlaced = true;
		}
	}

	if (ResourceTypeIndex < ResourceTypes.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSResourceSpawnArea '%s' requested %d resources but generated %d after finite placement attempts."), *GetName(), TotalCount, ResourceTypeIndex);
	}

	return ResourceTypeIndex;
}

void AJTSResourceSpawnArea::ClearGeneratedPickups()
{
	for (TObjectPtr<AJTSResourcePickupActor>& Pickup : GeneratedPickups)
	{
		if (IsValid(Pickup))
		{
			Pickup->Destroy();
		}
	}
	GeneratedPickups.Reset();
}
