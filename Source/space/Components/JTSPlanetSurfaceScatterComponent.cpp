#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "Engine/World.h"
#include "space/World/JTSPlanetDistributionData.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "space/World/JTSPlanetSurfaceGameplay.h"
#include "space/World/JTSMoonResourceCoverage.h"

UJTSPlanetSurfaceScatterComponent::UJTSPlanetSurfaceScatterComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UJTSPlanetSurfaceScatterComponent::Generate(AJTSPlanetAnchor* Planet, AActor* Controller)
{
	auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	if (!Habitat || !Habitat->HasAuthority() || !Planet || bGenerated) return;
	bGenerated = true;
	const FName Key = LayoutKey.IsNone() ? Habitat->GetFName() : LayoutKey;
	const bool bFrozen = DistributionData && !DistributionData->IsCapturing();
	const FJTSFrozenScatterLayout* Layout = bFrozen ? DistributionData->FindScatter(Key) : nullptr;
	if (bFrozen && !Layout)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing frozen scatter layout %s; rebake the distribution asset."), *Key.ToString());
		return;
	}
	if (Layout) Habitat->SetActorTransform(Layout->OwnerPose);
	TArray<FJTSFrozenSurfaceProp> CapturedProps;
	// Reuse the existing range-resource template's surface spacing and explicit landmark exclusions.
	FJTSMoonResourceCoverage Spacing(Planet, {}, 0);
	if (OwnerClearance > 0) Spacing.RecordPlacement(Habitat->GetActorLocation(), OwnerClearance);
	for (AActor* Exclusion : ExclusionActors) if (IsValid(Exclusion)) Spacing.RecordPlacement(Exclusion->GetActorLocation(), ExclusionRadius);
	auto FindPoint = [&](AJTSPlanetEcologyHabitat* Region, float MinimumSpacing, float MinSun, float MaxSun, FJTSPlanetSurfaceHit& Hit,
		AActor* Near = nullptr, FVector2D Distance = FVector2D::ZeroVector, bool bAtEdge = false)
	{
		for (int32 Attempt = 0; Attempt < 80; ++Attempt)
		{
			if (bAtEdge && Near)
			{
				const auto* Mesh = Near->FindComponentByClass<UStaticMeshComponent>();
				if (!Mesh || !Mesh->GetStaticMesh()) continue;
				const auto Bounds = Mesh->GetStaticMesh()->GetBounds();
				const float Angle = FMath::FRandRange(0, UE_TWO_PI);
				const FVector Edge = Bounds.Origin + FVector(Bounds.BoxExtent.X * FMath::Cos(Angle), Bounds.BoxExtent.Y * FMath::Sin(Angle), 0);
				const FVector Outward = Mesh->GetComponentTransform().TransformVectorNoScale(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0)).GetSafeNormal();
				const FVector Point = Mesh->GetComponentTransform().TransformPosition(Edge) + Outward * FMath::FRandRange(Distance.X, FMath::Max(Distance.X, Distance.Y));
				if (!Planet->ProjectPointToSurface(Point, Hit)) continue;
			}
			else
			{
				if (!Planet->RandomPointInSurfaceCap(Planet->GetRadialUpVector(Near ? Near->GetActorLocation() : Region->GetActorLocation()),
					Near ? FMath::Max(Distance.X, Distance.Y) : Region->CoreRadius, Hit)) continue;
				if (Near && Planet->ApproximateSurfaceArcDistance(Near->GetActorLocation(), Hit.ImpactPoint) < Distance.X) continue;
			}
			if (!Planet->IsSurfacePlacementAllowed(Hit)
				&& (SlopeFootSearchRadius <= 0 || !Planet->FindSurfacePlacementSupport(Hit.ImpactPoint, SlopeFootSearchRadius, Hit))) continue;
			if (!Region->ContainsPoint(Hit.ImpactPoint, Planet, true) || !Spacing.CanPlace(Hit.ImpactPoint, MinimumSpacing)) continue;
			const double SunDot = IsValid(Habitat->SunLight) ? FVector::DotProduct(Planet->GetRadialUpVector(Hit.ImpactPoint), -Habitat->SunLight->GetActorForwardVector()) : 0;
			if (SunDot < MinSun || SunDot > MaxSun) continue;
			return true;
		}
		return false;
	};
	if (!bFrozen && IsValid(PlacementRegion))
	{
		FJTSPlanetSurfaceHit Hit;
		const float MinSun = Habitat->LightPreference == EJTSHabitatLight::Sunlit ? Habitat->TerminatorMargin : -1;
		const float MaxSun = Habitat->LightPreference == EJTSHabitatLight::Shadow ? -Habitat->TerminatorMargin : 1;
		if (FindPoint(PlacementRegion, 700, MinSun, MaxSun, Hit))
		{
			FJTSPlanetSurfaceFrame Frame;
			if (Planet->GetSurfaceFrameAt(Hit.ImpactPoint, FMath::VRand(), Frame))
			{
				const FTransform Previous = Habitat->GetActorTransform();
				for (AActor* Actor : RelocateWithOwner)
				{
					if (!IsValid(Actor) || Actor == Habitat) continue;
					const FTransform Relative = Actor->GetActorTransform().GetRelativeTransform(Previous);
					Actor->SetActorTransform(Relative * Frame.Transform); Actor->ForceNetUpdate();
				}
				Habitat->SetActorTransform(Frame.Transform);
			}
		}
	}
	if (auto* Entrance = Habitat->CreateEntranceAttachment(Planet))
	{
		GeneratedActors.Add(Entrance);
		if (auto* Gameplay = Cast<IJTSPlanetSurfaceGameplay>(Controller)) Gameplay->RegisterSurfaceRuntimeActor(Entrance);
	}
	if (Layout)
	{
		for (const auto& Prop : Layout->Props) SpawnFrozenProp(Prop, Planet, Controller);
		return;
	}
	int32 EntryIndex = -1;
	for (const FJTSPlanetSurfaceScatterEntry& Entry : Entries)
	{
		++EntryIndex;
		if (Entry.Meshes.IsEmpty()) continue;
		for (int32 Index = 0; Index < FMath::Clamp(Entry.Count, 0, 128); ++Index)
		{
			FJTSPlanetSurfaceHit Hit;
			AActor* Near = nullptr;
			if (Entry.bNearRefuge)
			{
				TArray<AActor*> Sources;
				for (AActor* Refuge : Habitat->Refuges) if (IsValid(Refuge)) Sources.Add(Refuge);
				if (IsValid(Habitat->MaintenanceTarget)) Sources.Add(Habitat->MaintenanceTarget);
				if (Sources.IsEmpty()) continue;
				Near = Sources[FMath::RandRange(0, Sources.Num()-1)];
			}
			if (!FindPoint(Habitat, Entry.MinimumSpacing, Entry.MinimumSunDot, Entry.MaximumSunDot, Hit, Near, Entry.RefugeDistance, Entry.bAtRefugeEdge)) continue;
			FJTSPlanetSurfaceFrame Frame;
			if (!Planet->GetSurfaceFrameAt(Hit.ImpactPoint, FMath::VRand(), Frame)) continue;
			FTransform Pose = Frame.Transform;
			Pose.SetScale3D(FVector(FMath::FRandRange(FMath::Max(0.1, Entry.ScaleRange.X), FMath::Max(Entry.ScaleRange.X, Entry.ScaleRange.Y))));
			const int32 Variant = FMath::RandRange(0, Entry.Meshes.Num()-1);
			if (!Entry.Meshes[Variant]) continue;
			const auto Bounds = Entry.Meshes[Variant]->GetBounds();
			bool bValidFootprint = true;
			int32 SupportedSamples = 0;
			for (int32 X = -1; X <= 1 && bValidFootprint; ++X)
				for (int32 Y = -1; Y <= 1 && bValidFootprint; ++Y)
				{
					FJTSPlanetSurfaceHit Foot;
					const FVector Point = Pose.TransformPosition(FVector(Bounds.Origin.X + X * Bounds.BoxExtent.X, Bounds.Origin.Y + Y * Bounds.BoxExtent.Y, 0));
					bValidFootprint = Planet->ProjectPointToSurface(Point, Foot);
					if (bValidFootprint && Planet->IsSurfacePlacementAllowed(Foot)) ++SupportedSamples;
				}
			// A supported base with an approachable half-footprint may touch the adjoining slope.
			if (!bValidFootprint || SupportedSamples < (SlopeFootSearchRadius > 0 ? 4 : 9)) continue;
			FJTSFrozenSurfaceProp Prop;
			Prop.Pose = Pose; Prop.Recipe = Entry; Prop.Variant = Variant;
			Prop.Population = Entry.ResidentCountRange.X >= 0 && Entry.ResidentCountRange.Y >= 0
				? FMath::RandRange(Entry.ResidentCountRange.X, FMath::Max(Entry.ResidentCountRange.X, Entry.ResidentCountRange.Y)) : -1;
			Prop.ResidentKey = FName(*FString::Printf(TEXT("%s/%d/%d"), *Key.ToString(), EntryIndex, Index));
			CapturedProps.Add(Prop);
			SpawnFrozenProp(Prop, Planet, Controller);
			Spacing.RecordPlacement(Hit.ImpactPoint, Entry.MinimumSpacing);
		}
	}
	if (DistributionData) DistributionData->RecordScatter(Key, Habitat->GetActorTransform(), CapturedProps);
}
void UJTSPlanetSurfaceScatterComponent::SpawnFrozenProp(const FJTSFrozenSurfaceProp& Prop, AJTSPlanetAnchor* Planet, AActor* Controller)
{
	auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	if (!Habitat) return;
	const auto& Entry = Prop.Recipe;
	const int32 Variant = Prop.Variant;
	if (!Entry.Meshes.IsValidIndex(Variant) || !Entry.Meshes[Variant]) return;
	FActorSpawnParameters Params; Params.Owner = Planet; Params.OverrideLevel = Habitat->GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Attachment = GetWorld()->SpawnActor<AJTSPlanetSurfaceAttachment>(AJTSPlanetSurfaceAttachment::StaticClass(), Prop.Pose, Params);
	if (!Attachment) return;
	Attachment->Initialize(Entry.Meshes[Variant], Planet, Prop.Pose, Entry.bConformToSurface, Entry.bCastShadow, 0.7f, Entry.bDestructible,
		Entry.CutVolumes.IsValidIndex(Variant) ? Entry.CutVolumes[Variant].Get() : nullptr, Entry.ResourceItem, Entry.ResourceUnits);
	GeneratedActors.Add(Attachment);
	if (Entry.bRefuge) Habitat->Refuges.Add(Attachment);
	if (Entry.bFood) Habitat->FoodPoints.Add(Attachment);
	if (auto* Gameplay = Cast<IJTSPlanetSurfaceGameplay>(Controller)) Gameplay->RegisterSurfaceRuntimeActor(Attachment);
	if (!Entry.ResidentHabitatClass || Prop.Population == 0) return;
	auto* Resident = GetWorld()->SpawnActor<AJTSPlanetEcologyHabitat>(Entry.ResidentHabitatClass, Prop.Pose, Params);
	if (!Resident) return;
	if (Prop.Population >= 0) Resident->SetInitialPopulation(Prop.Population);
	GeneratedActors.Add(Resident); Resident->SunLight = Habitat->SunLight;
	Resident->RefugeProviders.Add(Habitat); Habitat->RefugeProviders.Add(Resident);
	if (Entry.bFood || Entry.ResourceItem) Resident->FoodPoints.Add(Attachment);
	if (Entry.bRefuge) Resident->Refuges.Add(Attachment);
	Resident->SurfaceScatter->DistributionData = DistributionData;
	Resident->SurfaceScatter->LayoutKey = Prop.ResidentKey;
	Resident->SurfaceScatter->ExclusionActors = ExclusionActors;
	Resident->SurfaceScatter->ExclusionRadius = ExclusionRadius;
	Resident->SurfaceScatter->Generate(Planet, Controller);
	if (GetWorld()->IsGameWorld())
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Resident,
			[Resident, WeakPlanet = TWeakObjectPtr<AJTSPlanetAnchor>(Planet), WeakController = TWeakObjectPtr<AActor>(Controller)]
			{ if (WeakPlanet.IsValid() && WeakController.IsValid()) Resident->ActivateEcology(WeakPlanet.Get(), WeakController.Get()); }));
}
void UJTSPlanetSurfaceScatterComponent::Clear()
{
	if (!GetOwner()->HasAuthority()) return;
	if (auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner()))
	{
		Habitat->ReleaseEntranceAttachment();
		Habitat->Refuges.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
		Habitat->FoodPoints.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
		Habitat->RefugeProviders.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
	}
	for (auto Actor : GeneratedActors) if (Actor.IsValid())
	{
		// Editor-world destruction does not dispatch EndPlay. Clear nested recipes explicitly
		// so baking keeps their poses in the manifest without leaving generated level actors.
		if (auto* Resident = Cast<AJTSPlanetEcologyHabitat>(Actor.Get())) Resident->SurfaceScatter->Clear();
		Actor->Destroy();
	}
	GeneratedActors.Reset(); bGenerated = false;
}
void UJTSPlanetSurfaceScatterComponent::EndPlay(const EEndPlayReason::Type Reason) { Clear(); Super::EndPlay(Reason); }
