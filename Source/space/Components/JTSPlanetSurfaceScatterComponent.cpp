#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
	// Reuse the existing range-resource template's surface spacing and explicit landmark exclusions.
	FJTSMoonResourceCoverage Spacing(Planet, {}, 0);
	if (OwnerClearance > 0) Spacing.RecordPlacement(Habitat->GetActorLocation(), OwnerClearance);
	for (AActor* Exclusion : ExclusionActors) if (IsValid(Exclusion)) Spacing.RecordPlacement(Exclusion->GetActorLocation(), ExclusionRadius);
	auto FindPoint = [&](AJTSPlanetEcologyHabitat* Region, float MinimumSpacing, float MinSun, float MaxSun, FJTSPlanetSurfaceHit& Hit)
	{
		for (int32 Attempt = 0; Attempt < 80; ++Attempt)
		{
			if (!Planet->RandomPointInSurfaceCap(Planet->GetRadialUpVector(Region->GetActorLocation()), Region->CoreRadius, Hit)
				|| !Region->ContainsPoint(Hit.ImpactPoint, Planet, true) || !Spacing.CanPlace(Hit.ImpactPoint, MinimumSpacing)) continue;
			if (FVector::DotProduct(Hit.ImpactNormal, Planet->GetRadialUpVector(Hit.ImpactPoint)) < 0.6) continue;
			const double SunDot = IsValid(Habitat->SunLight) ? FVector::DotProduct(Planet->GetRadialUpVector(Hit.ImpactPoint), -Habitat->SunLight->GetActorForwardVector()) : 0;
			if (SunDot < MinSun || SunDot > MaxSun) continue;
			return true;
		}
		return false;
	};
	if (IsValid(PlacementRegion))
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
	for (const FJTSPlanetSurfaceScatterEntry& Entry : Entries)
	{
		if (Entry.Meshes.IsEmpty()) continue;
		for (int32 Index = 0; Index < FMath::Clamp(Entry.Count, 0, 128); ++Index)
		{
			FJTSPlanetSurfaceHit Hit;
			if (!FindPoint(Habitat, Entry.MinimumSpacing, Entry.MinimumSunDot, Entry.MaximumSunDot, Hit)) continue;
			FJTSPlanetSurfaceFrame Frame;
			if (!Planet->GetSurfaceFrameAt(Hit.ImpactPoint, FMath::VRand(), Frame)) continue;
			FTransform Pose = Frame.Transform;
			Pose.SetScale3D(FVector(FMath::FRandRange(FMath::Max(0.1, Entry.ScaleRange.X), FMath::Max(Entry.ScaleRange.X, Entry.ScaleRange.Y))));
			FActorSpawnParameters Params; Params.Owner = Planet; Params.OverrideLevel = Habitat->GetLevel();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Attachment = GetWorld()->SpawnActor<AJTSPlanetSurfaceAttachment>(AJTSPlanetSurfaceAttachment::StaticClass(), Pose, Params);
			if (!Attachment) continue;
			const int32 Variant = FMath::RandRange(0, Entry.Meshes.Num()-1);
			Attachment->Initialize(Entry.Meshes[Variant], Planet, Pose, Entry.bConformToSurface, Entry.bCastShadow, 0.7f, Entry.bDestructible,
				Entry.CutVolumes.IsValidIndex(Variant) ? Entry.CutVolumes[Variant].Get() : nullptr);
			GeneratedActors.Add(Attachment);
			if (Entry.bRefuge) Habitat->Refuges.Add(Attachment);
			if (Entry.bFood) Habitat->FoodPoints.Add(Attachment);
			Spacing.RecordPlacement(Hit.ImpactPoint, Entry.MinimumSpacing);
			if (auto* Gameplay = Cast<IJTSPlanetSurfaceGameplay>(Controller)) Gameplay->RegisterSurfaceRuntimeActor(Attachment);
			if (Entry.ResidentHabitatClass)
			{
				auto* Resident = GetWorld()->SpawnActor<AJTSPlanetEcologyHabitat>(Entry.ResidentHabitatClass, Frame.Transform, Params);
				if (!Resident) continue;
				GeneratedActors.Add(Resident); Resident->SunLight = Habitat->SunLight;
				Habitat->RefugeProviders.Add(Resident);
				if (Entry.bFood) Resident->FoodPoints.Add(Attachment);
				if (Entry.bRefuge) Resident->Refuges.Add(Attachment);
				Resident->SurfaceScatter->ExclusionActors = ExclusionActors;
				Resident->SurfaceScatter->Generate(Planet, Controller);
				// The coalesced surface-cut rebuild runs before residents choose a refuge.
				GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Resident,
					[Resident, WeakPlanet = TWeakObjectPtr<AJTSPlanetAnchor>(Planet), WeakController = TWeakObjectPtr<AActor>(Controller)]
					{ if (WeakPlanet.IsValid() && WeakController.IsValid()) Resident->ActivateEcology(WeakPlanet.Get(), WeakController.Get()); }));
			}
		}
	}
}
void UJTSPlanetSurfaceScatterComponent::Clear()
{
	if (!GetOwner()->HasAuthority()) return;
	if (auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner()))
	{
		Habitat->Refuges.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
		Habitat->FoodPoints.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
		Habitat->RefugeProviders.RemoveAll([this](const auto& Actor) { return GeneratedActors.Contains(Actor.Get()); });
	}
	for (auto Actor : GeneratedActors) if (Actor.IsValid()) Actor->Destroy();
	GeneratedActors.Reset(); bGenerated = false;
}
void UJTSPlanetSurfaceScatterComponent::EndPlay(const EEndPlayReason::Type Reason) { Clear(); Super::EndPlay(Reason); }
