#include "space/World/JTSPlanetDistributionData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

const FJTSFrozenScatterLayout* UJTSPlanetDistributionData::FindScatter(FName Key) const
{
	return ScatterLayouts.FindByPredicate([Key](const auto& Layout) { return Layout.Key == Key; });
}
const FJTSFrozenResourceLayout* UJTSPlanetDistributionData::FindResources(FName Key) const
{
	return ResourceLayouts.FindByPredicate([Key](const auto& Layout) { return Layout.Key == Key; });
}
void UJTSPlanetDistributionData::ApplyActorPoses(UWorld* World) const
{
	if (!World || World->GetNetMode() == NM_Client) return;
	for (TActorIterator<AActor> It(World); It; ++It)
		if (const auto* Entry = ActorPoses.FindByPredicate([&](const auto& Value) { return Value.ActorName == It->GetFName(); }))
			It->SetActorTransform(Entry->Pose);
}
void UJTSPlanetDistributionData::RecordScatter(FName Key, const FTransform& Pose, const TArray<FJTSFrozenSurfaceProp>& Props)
{
	if (!bCapturing) return;
	FJTSFrozenScatterLayout Layout; Layout.Key = Key; Layout.OwnerPose = Pose; Layout.Props = Props;
	ScatterLayouts.Add(MoveTemp(Layout));
}
void UJTSPlanetDistributionData::RecordResource(FName Key, const FJTSFrozenResource& Resource)
{
	if (!bCapturing) return;
	auto* Layout = ResourceLayouts.FindByPredicate([Key](const auto& Value) { return Value.Key == Key; });
	if (!Layout) { Layout = &ResourceLayouts.AddDefaulted_GetRef(); Layout->Key = Key; }
	Layout->Resources.Add(Resource);
}
bool UJTSPlanetDistributionData::BakeDistribution(AJTSPlanetAnchor* Planet, const TArray<AJTSPlanetEcologyHabitat*>& Habitats,
	const TArray<AJTSMoonResourceSpawner*>& Spawners)
{
#if WITH_EDITOR
	if (!IsValid(Planet) || !Planet->GetWorld() || Planet->GetWorld()->IsGameWorld() || bCapturing) return false;
	Modify(); ActorPoses.Reset(); ScatterLayouts.Reset(); ResourceLayouts.Reset(); bCapturing = true;
	TMap<AActor*, FTransform> OriginalPoses;
	auto Remember = [&](AActor* Actor) { if (IsValid(Actor)) OriginalPoses.FindOrAdd(Actor, Actor->GetActorTransform()); };
	for (auto* Habitat : Habitats)
	{
		if (!IsValid(Habitat)) continue;
		Remember(Habitat);
		for (AActor* Actor : Habitat->SurfaceScatter->RelocateWithOwner) Remember(Actor);
		Habitat->SurfaceScatter->DistributionData = this;
		Habitat->SurfaceScatter->Clear();
	}
	TMap<AJTSMoonResourceSpawner*, int32> Batches;
	for (auto* Spawner : Spawners)
	{
		if (!IsValid(Spawner)) continue;
		Remember(Spawner); Spawner->DistributionData = this; Spawner->SetOwningPlanet(Planet);
		Batches.Add(Spawner, Spawner->InitialSpawnBatchSize); Spawner->InitialSpawnBatchSize = 0;
		FJTSFrozenResourceLayout Empty; Empty.Key = Spawner->GetFName(); ResourceLayouts.Add(Empty);
	}
	AJTSMoonResourceSpawner::GenerateResourceGroup(Spawners);
	for (auto* Habitat : Habitats) if (IsValid(Habitat)) Habitat->SurfaceScatter->Generate(Planet, nullptr);
	for (const auto& Pair : OriginalPoses)
	{
		FJTSFrozenActorPose Pose; Pose.ActorName = Pair.Key->GetFName(); Pose.Pose = Pair.Key->GetActorTransform(); ActorPoses.Add(Pose);
	}
	for (auto* Habitat : Habitats) if (IsValid(Habitat)) Habitat->SurfaceScatter->Clear();
	for (const auto& Pair : Batches) { Pair.Key->ClearGeneratedResources(); Pair.Key->InitialSpawnBatchSize = Pair.Value; }
	for (const auto& Pair : OriginalPoses) Pair.Key->SetActorTransform(Pair.Value);
	bCapturing = false; MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
