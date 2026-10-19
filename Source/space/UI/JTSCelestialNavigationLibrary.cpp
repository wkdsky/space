#include "space/UI/JTSCelestialNavigationLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SceneView.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Components/JTSPlanetFragmentEnvelopeComponent.h"

TArray<FJTSCelestialNavigationBody> UJTSCelestialNavigationLibrary::GatherBodies(
	const AJTSSpaceWorldManager* Manager, const AJTSSpacecraftActor* Ship)
{
	TArray<FJTSCelestialNavigationBody> Bodies;
	if (!IsValid(Manager) || !IsValid(Ship)) return Bodies;
	TArray<AJTSPlanetAnchor*> Planets;
	Manager->GetRegisteredPlanets(Planets);
	TSet<const AActor*> Seen;
	for (AJTSPlanetAnchor* Planet : Planets)
	{
		if (!IsValid(Planet)) continue;
		FJTSCelestialNavigationBody& Body = Bodies.AddDefaulted_GetRef();
		Body.Planet = Planet;
		Body.VisualActor = Planet->GetGameplaySurfaceActor();
		Body.DisplayName = Planet->GetNavigationDisplayName();
		Body.bShowOnStarChart = Planet->ShouldShowOnStarChart();
		Body.bShowScreenMarker = Planet->ShouldShowScreenMarker();
		Body.PaletteIndex = Planet->GetNavigationPaletteIndex();
		Body.Center = Planet->GetPlanetCenter();
		Body.ShipDistanceCentimeters = FVector::Distance(Ship->GetActorLocation(), Body.Center);
		Body.MarkerDistanceCentimeters = Planet->GetNavigationMarkerDistance();
		if (Manager->CompressedSpace->IsConfigured() && Planet->GetNavigationMarkerRangeKilometers() > 0.0f)
		{
			Body.ShipDistanceCentimeters = FVector::Distance(
				Manager->CompressedSpace->NavigationPosition(Ship->GetActorLocation()),
				Manager->CompressedSpace->NavigationPosition(Body.Center)) * 100000.0;
			Body.MarkerDistanceCentimeters = Planet->GetNavigationMarkerRangeKilometers() * 100000.0f;
		}
		Body.VisualRadius = Planet->GetVisualRadius();
		Body.FragmentPresentationScale = Manager->CompressedSpace->IsConfigured()
			? Manager->CompressedSpace->VisualScale(Ship->GetActorLocation(), Planet) : 1.0;
		Body.RangeCentimeters = Manager->GetCruiseRangeCentimeters(Ship, Planet);
		if (Manager->CompressedSpace->ContainsBody(Planet))
			Body.Center = Manager->CompressedSpace->PresentationCenter(Ship->GetActorLocation(), Planet);
		Body.bVisible = !Planet->IsHidden() && IsValid(Body.VisualActor) && !Body.VisualActor->IsHidden();
		Seen.Add(Body.VisualActor);
		for (const FJTSCelestialBackgroundNavigation& Entry : Planet->GetBackgroundNavigation())
		{
			AActor* Actor = Entry.Actor;
			if (!IsValid(Actor) || Entry.DisplayName.IsEmpty() || Seen.Contains(Actor)
				|| !Planet->GetCelestialBackgroundActors().Contains(Actor)) continue;
			const UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();
			if (!IsValid(Mesh) || !IsValid(Mesh->GetStaticMesh())) continue;
			Seen.Add(Actor);
			const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
			const FVector Extent = Box.GetExtent();
			const float Radius = Entry.LocalVisualRadius > 0.0f ? Entry.LocalVisualRadius
				: FMath::Pow(Extent.X * Extent.Y * Extent.Z, 1.0 / 3.0);
			FJTSCelestialNavigationBody& Background = Bodies.AddDefaulted_GetRef();
			Background.DisplayName = Entry.DisplayName;
			Background.VisualActor = Actor;
			Background.Center = Mesh->GetComponentTransform().TransformPosition(Box.GetCenter());
			Background.ShipDistanceCentimeters = FVector::Distance(Ship->GetActorLocation(), Background.Center);
			Background.MarkerDistanceCentimeters = Entry.MarkerDistanceCentimeters;
			Background.VisualRadius = Radius * Mesh->GetComponentScale().GetAbsMax();
			Background.RangeCentimeters = Manager->GetBackgroundCruiseRangeCentimeters(Ship, Planet, Actor);
			Background.PaletteIndex = Entry.PaletteIndex;
			Background.bShowOnStarChart = Entry.bShowOnStarChart;
			Background.bShowScreenMarker = Entry.bShowScreenMarker;
			Background.bVisible = !Actor->IsHidden() && Mesh->IsVisible();
		}
	}
	return Bodies;
}

bool UJTSCelestialNavigationLibrary::IsWithinMarkerDistance(const FJTSCelestialNavigationBody& Body)
{
	return Body.bShowScreenMarker && FMath::IsFinite(Body.ShipDistanceCentimeters)
		&& FMath::IsFinite(Body.MarkerDistanceCentimeters) && Body.ShipDistanceCentimeters >= 0.0f
		&& Body.MarkerDistanceCentimeters > 0.0f && Body.ShipDistanceCentimeters <= Body.MarkerDistanceCentimeters;
}

bool UJTSCelestialNavigationLibrary::ProjectBody(const FJTSCelestialNavigationBody& Body,
	const FVector& ViewOrigin, const FMatrix& ViewProjection, const FIntRect& ViewRect, FBox2D& OutBounds)
{
	OutBounds = FBox2D(ForceInit);
	const FVector Offset = Body.Center - ViewOrigin;
	const double Distance = Offset.Size();
	const double Radius = Body.VisualRadius;
	if (!Body.bVisible || !FMath::IsFinite(Radius) || Radius <= 0.0 || ViewRect.IsEmpty()) return false;
	if (const auto* Envelope = IsValid(Body.Planet) && Body.Planet->IsFragmentedGameplaySurface()
		? Body.Planet->GetFragmentEnvelope() : nullptr; Envelope && Envelope->HasGeometry())
	{
		// Project the existing authored convex wraps, not an enclosing sphere or a
		// rotated Actor AABB. Linear perspective extrema lie on hull vertices.
		const FQuat Rotation = Envelope->GeometryRotation.Quaternion();
		for (const FJTSFragmentHull& Hull : Envelope->Data->Fragments)
			for (const FVector& Vertex : Hull.Vertices)
			{
				const FVector World = Body.Center + Rotation.RotateVector(Vertex) * Body.FragmentPresentationScale;
				FVector2D Point;
				if (!FSceneView::ProjectWorldToScreen(World, ViewRect, ViewProjection, Point)) return false;
				OutBounds += Point - FVector2D(ViewRect.Min);
			}
		return OutBounds.bIsValid && OutBounds.Max.X >= 0.0 && OutBounds.Min.X <= ViewRect.Width()
			&& OutBounds.Max.Y >= 0.0 && OutBounds.Min.Y <= ViewRect.Height();
	}
	if (Distance <= Radius) return false;
	FVector2D Projected;
	if (!FSceneView::ProjectWorldToScreen(Body.Center, ViewRect, ViewProjection, Projected)) return false;
	const FVector Normal = Offset / Distance;
	FVector AxisX, AxisY;
	Normal.FindBestAxisVectors(AxisX, AxisY);
	const FVector DiskCenter = Body.Center - Normal * (Radius * Radius / Distance);
	const double DiskRadius = Radius * FMath::Sqrt(1.0 - FMath::Square(Radius / Distance));
	// Tangent-circle samples avoid using a rotated AABB as a planet's apparent size.
	for (int32 Index = 0; Index < 32; ++Index)
	{
		const double Angle = 2.0 * PI * Index / 32.0;
		const FVector Rim = DiskCenter + DiskRadius * (AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle));
		if (!FSceneView::ProjectWorldToScreen(Rim, ViewRect, ViewProjection, Projected)) return false;
		OutBounds += Projected - FVector2D(ViewRect.Min);
	}
	return OutBounds.Max.X >= 0.0 && OutBounds.Min.X <= ViewRect.Width()
		&& OutBounds.Max.Y >= 0.0 && OutBounds.Min.Y <= ViewRect.Height();
}

bool UJTSCelestialNavigationLibrary::IsOccluded(const FJTSCelestialNavigationBody& Body,
	const FVector& ViewOrigin, const TArray<FJTSCelestialNavigationBody>& Bodies)
{
	const FVector Offset = Body.Center - ViewOrigin;
	const double Distance = Offset.Size();
	if (Distance <= Body.VisualRadius) return false;
	const FVector Direction = Offset / Distance;
	for (const FJTSCelestialNavigationBody& Other : Bodies)
	{
		if (!Other.bVisible || Other.VisualActor == Body.VisualActor) continue;
		const FVector OtherOffset = Other.Center - ViewOrigin;
		const double Along = FVector::DotProduct(OtherOffset, Direction);
		const double PerpendicularSquared = OtherOffset.SizeSquared() - Along * Along;
		const double RadiusSquared = FMath::Square(Other.VisualRadius);
		if (Along <= 0.0 || PerpendicularSquared >= RadiusSquared) continue;
		const double Near = Along - FMath::Sqrt(RadiusSquared - FMath::Max(0.0, PerpendicularSquared));
		if (Near > 0.0 && Near < Distance - Body.VisualRadius) return true;
	}
	return false;
}
