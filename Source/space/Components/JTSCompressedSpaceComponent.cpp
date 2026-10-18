#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/World/JTSPlanetAnchor.h"

UJTSCompressedSpaceComponent::UJTSCompressedSpaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSCompressedSpaceComponent::CaptureAuthoredBodies(bool bRestorePrevious)
{
	if (bRestorePrevious) RestorePresentation();
	Captured.Reset();
	for (auto& Axis : Axes) Axis.Reset();
	for (const FJTSCompressedCelestialBody& Entry : Bodies)
	{
		if (!IsValid(Entry.Actor)) continue;
		AJTSPlanetAnchor* Planet = Cast<AJTSPlanetAnchor>(Entry.Actor);
		AActor* Visual = Planet ? Planet->GetGameplaySurfaceActor() : Entry.Actor.Get();
		if (!IsValid(Visual)) continue;
		FCapturedBody& Body = Captured.AddDefaulted_GetRef();
		Body.Body = Entry.Actor; Body.Visual = Visual;
		FVector Extent;
		Visual->GetActorBounds(false, Body.Center, Extent);
		if (Planet) Body.Center = Planet->GetPlanetCenter();
		Body.Radius = FMath::Max(1.0, Extent.GetAbsMax());
		Body.Location = Visual->GetActorLocation(); Body.Scale = Visual->GetActorScale3D();
		Body.bCollisionEnabled = Visual->GetActorEnableCollision();
		Body.Navigation = Entry.NavigationPositionKilometers;
		Body.PhysicalRadius = Entry.RadiusKilometers;
		Body.Multiplier = Entry.VisualRadiusMultiplier;
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		struct FLocalBand { double Center, Navigation, Radius; };
		TArray<FLocalBand> Bands;
		for (const auto& Body : Captured)
		{
			FLocalBand* Same = Bands.FindByPredicate([&](const FLocalBand& Band) { return FMath::IsNearlyEqual(Band.Center, Body.Center[Axis], 0.1); });
			if (Same)
			{
				ensureMsgf(FMath::IsNearlyEqual(Same->Navigation, Body.Navigation[Axis], 0.01), TEXT("Coincident world coordinates require coincident navigation coordinates"));
				Same->Radius = FMath::Max(Same->Radius, Body.Radius + NearAltitudeCentimeters);
			}
			else Bands.Add({Body.Center[Axis], Body.Navigation[Axis], Body.Radius + NearAltitudeCentimeters});
		}
		Bands.Sort([](const FLocalBand& A, const FLocalBand& B) { return A.Center < B.Center; });
		for (const auto& Band : Bands)
		{
			const FCoordinateKnot Low{Band.Center - Band.Radius, Band.Navigation - Band.Radius * LocalKilometersPerCentimeter()};
			const FCoordinateKnot High{Band.Center + Band.Radius, Band.Navigation + Band.Radius * LocalKilometersPerCentimeter()};
			if (!Axes[Axis].IsEmpty())
			{
				ensureMsgf(Low.World > Axes[Axis].Last().World && Low.Navigation > Axes[Axis].Last().Navigation,
					TEXT("Author separated, consistently ordered near-flight coordinate bands in the level"));
			}
			Axes[Axis].Add(Low); Axes[Axis].Add(High);
		}
	}
}

const UJTSCompressedSpaceComponent::FCapturedBody* UJTSCompressedSpaceComponent::Find(const AActor* Body) const
{
	return Captured.FindByPredicate([Body](const FCapturedBody& Entry) { return Entry.Body.Get() == Body; });
}

bool UJTSCompressedSpaceComponent::ContainsBody(const AActor* Body) const { return Find(Body) != nullptr; }
double UJTSCompressedSpaceComponent::AuthoredRadius(const AActor* Body) const { const auto* B = Find(Body); return B ? B->Radius : 0.0; }
double UJTSCompressedSpaceComponent::BubbleRadius(const AActor* Body) const { return AuthoredRadius(Body) + NearAltitudeCentimeters; }

double UJTSCompressedSpaceComponent::MapAxis(int32 Axis, double Value) const
{
	const auto& Knots = Axes[Axis];
	const double LocalScale = LocalKilometersPerCentimeter();
	if (Knots.IsEmpty()) return Value * LocalScale;
	const auto Extrapolate = [&](const FCoordinateKnot& Knot)
	{
		const double Delta = Value - Knot.World;
		const double D = FMath::Abs(Delta);
		return Knot.Navigation + Delta * (LocalScale + (FreeSpaceKilometersPerCentimeter - LocalScale) * D / (D + 100000.0));
	};
	if (Value <= Knots[0].World) return Extrapolate(Knots[0]);
	for (int32 Index = 1; Index < Knots.Num(); ++Index)
	{
		const auto& A = Knots[Index - 1]; const auto& B = Knots[Index];
		if (Value > B.World) continue;
		const double Span = B.World - A.World;
		const double T = (Value - A.World) / Span;
		// A positive linear floor plus a monotonic smooth residual: no folds or overshoot.
		return A.Navigation + LocalScale * (Value - A.World)
			+ (B.Navigation - A.Navigation - LocalScale * Span) * T * T * (3.0 - 2.0 * T);
	}
	return Extrapolate(Knots.Last());
}

FVector UJTSCompressedSpaceComponent::BasePosition(const FVector& P) const { return FVector(MapAxis(0,P.X), MapAxis(1,P.Y), MapAxis(2,P.Z)); }

FVector UJTSCompressedSpaceComponent::NavigationPosition(const FVector& Position) const
{
	// Separable monotonic coordinates also preserve approach on oblique routes: every
	// coordinate moves towards the target's coordinate. Radial blend islands do not.
	return BasePosition(Position);
}

FVector UJTSCompressedSpaceComponent::NavigationVelocity(const FVector& Position, const FVector& Velocity) const
{
	const double Speed = Velocity.Size();
	if (Speed < UE_SMALL_NUMBER) return FVector::ZeroVector;
	const FVector Step = Velocity / Speed; // centred one-centimetre derivative, in km/cm
	return (NavigationPosition(Position + Step) - NavigationPosition(Position - Step)) * (Speed * 50000.0);
}

double UJTSCompressedSpaceComponent::RangeCentimeters(const FVector& Position, const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	return Body ? FMath::Max(0.0, FVector::Distance(NavigationPosition(Position), Body->Navigation)
		- Body->Radius * LocalKilometersPerCentimeter()) * 100000.0 : 0.0;
}

double UJTSCompressedSpaceComponent::RouteKilometers(const AActor* First, const AActor* Second) const
{
	const auto* A = Find(First); const auto* B = Find(Second);
	return A && B ? FVector::Distance(A->Navigation, B->Navigation) : 0.0;
}

AJTSPlanetAnchor* UJTSCompressedSpaceComponent::ContainingPlanet(const FVector& Position) const
{
	for (const auto& Body : Captured)
		if (AJTSPlanetAnchor* Planet = Cast<AJTSPlanetAnchor>(Body.Body.Get()); IsValid(Planet)
			&& FVector::DistSquared(Position, Body.Center) < FMath::Square(Body.Radius + NearAltitudeCentimeters)) return Planet;
	return nullptr;
}

bool UJTSCompressedSpaceComponent::ClipAtFirstBubbleEntry(const FVector& Start, const FVector& End, FVector& OutPosition) const
{
	const FVector Delta = End - Start;
	const double A = Delta.SizeSquared();
	if (A <= UE_DOUBLE_SMALL_NUMBER) return false;
	double Earliest = 2.0;
	for (const auto& Body : Captured)
	{
		if (!Cast<AJTSPlanetAnchor>(Body.Body.Get())) continue;
		const FVector Offset = Start - Body.Center;
		const double C = Offset.SizeSquared() - FMath::Square(Body.Radius + NearAltitudeCentimeters);
		if (C < 0.0) continue;
		const double B = FVector::DotProduct(Offset, Delta);
		const double Discriminant = B * B - A * C;
		if (Discriminant <= 0.0) continue;
		const double T = (-B - FMath::Sqrt(Discriminant)) / A;
		if (T >= 0.0 && T <= 1.0) Earliest = FMath::Min(Earliest, T);
	}
	if (Earliest > 1.0) return false;
	// A two-centimetre inset establishes the new mode without consuming the remainder
	// of a cruise-speed frame inside near space. No speed-dependent surface tunnelling.
	OutPosition = Start + Delta * FMath::Min(1.0, Earliest + 2.0 / FMath::Sqrt(A));
	return true;
}

double UJTSCompressedSpaceComponent::DisplayRadiusKilometers(const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	return Body ? FMath::Max(Body->PhysicalRadius * Body->Multiplier,
		Body->Radius * LocalKilometersPerCentimeter()) : 0.0;
}

double UJTSCompressedSpaceComponent::VisualScale(const FVector& Position, const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	if (!Body) return 1.0;
	const double Distance = FVector::Distance(Position, Body->Center);
	const double Inner = Body->Radius + NearAltitudeCentimeters;
	if (Distance <= Inner) return 1.0;
	const double RangeKm = FMath::Max(UE_DOUBLE_SMALL_NUMBER, FVector::Distance(NavigationPosition(Position), Body->Navigation));
	// One fixed apparent radius per body. The local-metric floor guarantees full geometry
	// before bubble entry without a direction-dependent visual expansion annulus.
	return FMath::Clamp(DisplayRadiusKilometers(Actor) * Distance / (RangeKm * Body->Radius), 0.00000001, 1.0);
}

void UJTSCompressedSpaceComponent::ApplyPresentation(const FVector& Position)
{
	for (const auto& Body : Captured)
		if (AActor* Visual = Body.Visual.Get())
		{
			const double Ratio = VisualScale(Position, Body.Body.Get());
			// Small distant visuals are not physical surfaces. Restore authored collision
			// only after their full-size geometry has returned before bubble entry.
			if (Cast<AJTSPlanetAnchor>(Body.Body.Get()))
				Visual->SetActorEnableCollision(Body.bCollisionEnabled && Ratio >= 0.999999);
			Visual->SetActorScale3D(Body.Scale * Ratio);
			Visual->SetActorLocation(Body.Center + (Body.Location - Body.Center) * Ratio);
			// Each body remains independently visible. Rendering naturally rejects subpixel disks.
			Visual->SetActorHiddenInGame(false);
			if (AActor* Actor = Body.Body.Get()) Actor->SetActorHiddenInGame(false);
		}
}

void UJTSCompressedSpaceComponent::RestorePresentation()
{
	for (const auto& Body : Captured)
		if (AActor* Visual = Body.Visual.Get())
		{
			Visual->SetActorScale3D(Body.Scale);
			Visual->SetActorLocation(Body.Location);
			Visual->SetActorHiddenInGame(false);
			Visual->SetActorEnableCollision(Body.bCollisionEnabled);
		}
}
