#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSPlanetFragmentEnvelopeComponent.h"

UJTSCompressedSpaceComponent::UJTSCompressedSpaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSCompressedSpaceComponent::CaptureAuthoredBodies(bool bRestorePrevious)
{
	if (bRestorePrevious) RestorePresentation();
	Captured.Reset();
	Groups.Reset();
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
		if (const auto* Envelope = Planet && Planet->IsFragmentedGameplaySurface() ? Planet->GetFragmentEnvelope() : nullptr;
			Envelope && Envelope->HasGeometry())
		{
			// Rotating a mesh AABB inflates the debris region well beyond its actual
			// fragments. Capture one enclosing radius from the existing authored hulls.
			double RadiusSquared = 1.0;
			const FQuat Rotation = Envelope->GeometryRotation.Quaternion();
			for (const FJTSFragmentHull& Hull : Envelope->Data->Fragments)
				for (const FVector& Vertex : Hull.Vertices)
					RadiusSquared = FMath::Max(RadiusSquared, FVector::DistSquared(Planet->GetActorLocation() + Rotation.RotateVector(Vertex), Body.Center));
			Body.Radius = FMath::Sqrt(RadiusSquared);
		}
		Body.Location = Visual->GetActorLocation(); Body.Scale = Visual->GetActorScale3D();
		Body.bCollisionEnabled = Visual->GetActorEnableCollision();
		Body.Navigation = Entry.NavigationPositionKilometers;
		Body.GroupName = Entry.PresentationGroup;
		Body.PhysicalRadius = Entry.RadiusKilometers;
		Body.Multiplier = Entry.VisualRadiusMultiplier;
		Body.bNearFlight = Planet || Entry.bEnableNearFlight;
		const double SizeScale = Entry.NearFlightReferenceRadius > 0.0 ? FMath::Clamp(FMath::Pow(
			FMath::Max(1.0, Body.Radius / Entry.NearFlightReferenceRadius),
			FMath::Clamp(Entry.NearFlightSizeExponent, 0.0, 1.0)), 1.0, FMath::Max(1.0, Entry.NearFlightMaxSpeedMultiplier)) : 1.0;
		Body.NearSpeed = FMath::Max(1.0, Entry.NearFlightSpeedMultiplier) * SizeScale;
		Body.TransitionWidth = FMath::Max3(1.0, Entry.FlightTransitionMinimumWidth,
			Body.Radius * FMath::Max(0.01, Entry.FlightTransitionDistanceRadii));
		Body.ApproachExponent = FMath::Clamp(Entry.FlightApproachExponent, 0.01, 12.0);
		Body.AngularChange = FMath::DegreesToRadians(FMath::Max(0.0, Entry.FlightAngularChangeDegreesPerSecond));
		Body.RelativeAngularChange = FMath::Max(0.0, Entry.FlightRelativeAngularChangePerSecond);
	}
	// One stable primary and one enclosing volume per authored system. Reference-planet,
	// camera and nearest-body changes never select a new transform while flying within it.
	for (int32 Index = 0; Index < Captured.Num(); ++Index)
	{
		auto& Body = Captured[Index];
		if (Body.GroupName.IsNone()) continue;
		int32 GroupIndex = Groups.IndexOfByPredicate([&](const auto& Group) { return Group.Name == Body.GroupName; });
		if (GroupIndex == INDEX_NONE)
		{
			GroupIndex = Groups.AddDefaulted();
			Groups[GroupIndex].Name = Body.GroupName;
			Groups[GroupIndex].PrimaryIndex = Index;
		}
		Body.GroupIndex = GroupIndex;
		auto& Group = Groups[GroupIndex];
		if (Body.Radius > Captured[Group.PrimaryIndex].Radius) Group.PrimaryIndex = Index;
	}
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		auto& Group = Groups[Index];
		const FVector Center = Captured[Group.PrimaryIndex].Center;
		double Radius = 1.0;
		for (const auto& Body : Captured)
			if (Body.GroupIndex == Index) Radius = FMath::Max(Radius,
				FVector::Distance(Body.Center, Center) + Body.Radius + NearAltitudeCentimeters);
		Group.Inner = Radius + FMath::Max(NearAltitudeCentimeters * 4.0, Radius * FMath::Max(0.0, GroupLocalPaddingRadii));
		Group.Outer = Group.Inner + Radius * FMath::Max(1.0, GroupTransitionRadii);
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
FVector UJTSCompressedSpaceComponent::AuthoredCenter(const AActor* Body) const
{
	const auto* B = Find(Body);
	return B ? B->Center : IsValid(Body) ? Body->GetActorLocation() : FVector::ZeroVector;
}

FVector UJTSCompressedSpaceComponent::PresentationCenter(const FVector& Position, const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	if (!Body) return AuthoredCenter(Actor);
	if (!Groups.IsValidIndex(Body->GroupIndex)) return Body->Center;
	const auto& Group = Groups[Body->GroupIndex];
	const FVector Center = Captured[Group.PrimaryIndex].Center;
	return Center + (Body->Center - Center) * GroupVisualScale(Position, Group);
}
double UJTSCompressedSpaceComponent::FlightTransitionWidth(const AActor* Body) const { const auto* B = Find(Body); return B ? B->TransitionWidth : 0.0; }
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

AActor* UJTSCompressedSpaceComponent::ContainingNearFlightBody(const FVector& Position) const
{
	const FCapturedBody* Closest = nullptr;
	double Best = 1.0;
	for (const auto& Body : Captured)
	{
		if (!Body.bNearFlight) continue;
		const double RelativeDistance = FVector::DistSquared(Position, Body.Center)
			/ FMath::Square(Body.Radius + NearAltitudeCentimeters);
		if (RelativeDistance < Best) { Best = RelativeDistance; Closest = &Body; }
	}
	return Closest ? Closest->Body.Get() : nullptr;
}

double UJTSCompressedSpaceComponent::CruiseApproachMultiplier(const FVector& Position,
	const FVector& FlightDirection, double CruiseMultiplier, double /*NearBoundaryMultiplier*/, double Exponent) const
{
	const double Cruise = FMath::Max(1.0, CruiseMultiplier);
	double WeightedLogSpeed = 0.0, TotalWeight = 0.0;
	for (const auto& Body : Captured)
	{
		if (!Body.bNearFlight) continue;
		const double Distance = FVector::Distance(Position, Body.Center);
		const double Inner = Body.Radius + NearAltitudeCentimeters;
		const double Clearance = Distance - Inner;
		if (Clearance >= Body.TransitionWidth || (Exponent <= 0.0 && Clearance >= 0.0)) continue;
		const auto* Planet = Cast<AJTSPlanetAnchor>(Body.Body.Get());
		const double Near = FMath::Clamp(Planet ? Planet->GetNearFlightSpeedMultiplier(Body.Radius) : Body.NearSpeed, 1.0, Cruise);
		const double Strength = FMath::Clamp(Planet ? Planet->GetCruiseApproachExponent(Exponent) : Body.ApproachExponent, 0.01, 12.0);
		const double Progress = FMath::Clamp(Clearance / Body.TransitionWidth, 0.0, 1.0);
		// Shape the curve independently of its width. Arithmetic interpolation avoids
		// the long low-speed tail of interpolating a 100:1 speed range in log space.
		const double Shaped = Progress / (Progress + Strength * (1.0 - Progress));
		const double Travel = FMath::SmoothStep(0.0, 1.0, Shaped);
		double Speed = FMath::Lerp(Near, Cruise, Travel);
		double VisualLoad = 0.0;
		if (Clearance > 0.0 && !FlightDirection.IsNearlyZero()
			&& (Body.AngularChange > 0.0 || Body.RelativeAngularChange > 0.0))
		{
			const FVector Direction = FlightDirection.GetSafeNormal();
			// Preview across a fraction of the body's own scale, rather than reacting
			// to a centimetre-wide compression derivative after reaching it.
			const double Sample = FMath::Max(1.0, FMath::Max(Body.Radius, NearAltitudeCentimeters) * 0.15);
			const double Angle = ApparentDiameterRadians(Position, Body.Body.Get());
			// Central absolute derivative treats approach and departure equally, and
			// includes the actual compressed visual scale rather than just R / distance.
			const double Change = FMath::Abs(ApparentDiameterRadians(Position + Direction * Sample, Body.Body.Get())
				- ApparentDiameterRadians(Position - Direction * Sample, Body.Body.Get())) / (2.0 * Sample);
			const double Budget = FMath::Sqrt(FMath::Square(Body.AngularChange)
				+ FMath::Square(Angle * Body.RelativeAngularChange));
			VisualLoad = (Speed - Near) * FlightDirection.Size() * Change / FMath::Max(UE_DOUBLE_SMALL_NUMBER, Budget);
			// Soft saturation of the extra cruise speed; local handling is always retained.
			// This is one body's contribution, not another global minimum-speed selector.
			Speed = Near + (Speed - Near) / FMath::Sqrt(1.0 + VisualLoad * VisualLoad);
		}
		const double Influence = 1.0 - FMath::SmoothStep(0.0, 1.0, Progress);
		const double Proximity = Inner / FMath::Max(Inner, Distance);
		// A small body's rapid visual expansion must still count beside a broad
		// neighbouring giant. All contributions remain normalized, never multiplied.
		const double Weight = FMath::Square(Influence) * FMath::Square(FMath::Square(Proximity))
			* (1.0 + VisualLoad * VisualLoad);
		WeightedLogSpeed += Weight * FMath::Loge(Speed);
		TotalWeight += Weight;
	}
	// Normalization keeps the result between contributing speeds: extra bodies cannot
	// multiply braking into a stall, or select a winner when two/three regions overlap.
	return TotalWeight > 0.0 ? FMath::Exp(WeightedLogSpeed / TotalWeight) : Cruise;
}

double UJTSCompressedSpaceComponent::CruiseSimulationStep(const FVector& Position, const FVector& WorldVelocity, double RemainingSeconds) const
{
	const double WorldSpeed = WorldVelocity.Size();
	if (WorldSpeed <= UE_SMALL_NUMBER) return RemainingSeconds;
	const FVector Direction = WorldVelocity / WorldSpeed;
	double Distance = WorldSpeed * RemainingSeconds;
	for (const auto& Body : Captured)
	{
		if (!Body.bNearFlight) continue;
		const FVector Offset = Position - Body.Center;
		const double Radius = Offset.Size();
		const double Inner = Body.Radius + NearAltitudeCentimeters;
		const double Outer = Inner + Body.TransitionWidth;
		const double Along = FVector::DotProduct(Offset, Direction);
		if (Radius <= Outer)
			Distance = FMath::Min(Distance, FMath::Max(2.0, Body.TransitionWidth * 0.002));
		else if (Along < 0.0)
		{
			const double Discriminant = FMath::Square(Outer) - (Offset.SizeSquared() - Along * Along);
			if (Discriminant > 0.0)
				Distance = FMath::Min(Distance, FMath::Max(2.0, -Along - FMath::Sqrt(Discriminant)));
		}
		// Bound both visual growth and shrinkage when integrating the same speed field.
		if (Radius <= Outer || Along < 0.0)
		{
			const double Initial = ApparentDiameterRadians(Position, Body.Body.Get());
			const double MaximumGrowth = FMath::DegreesToRadians(0.5);
			if (FMath::Abs(ApparentDiameterRadians(Position + Direction * Distance, Body.Body.Get()) - Initial) > MaximumGrowth)
			{
				double Low = 0.0, High = Distance;
				for (int32 I = 0; I < 24; ++I)
				{
					const double Mid = (Low + High) * 0.5;
					if (FMath::Abs(ApparentDiameterRadians(Position + Direction * Mid, Body.Body.Get()) - Initial) > MaximumGrowth) High = Mid;
					else Low = Mid;
				}
				Distance = FMath::Min(Distance, FMath::Max(0.01, Low));
			}
		}
	}
	return FMath::Min(RemainingSeconds, FMath::Max(0.000001, Distance / WorldSpeed));
}

bool UJTSCompressedSpaceComponent::ClipAtFirstBubbleEntry(const FVector& Start, const FVector& End, FVector& OutPosition) const
{
	const FVector Delta = End - Start;
	const double A = Delta.SizeSquared();
	if (A <= UE_DOUBLE_SMALL_NUMBER) return false;
	double Earliest = 2.0;
	for (const auto& Body : Captured)
	{
		if (!Body.bNearFlight) continue;
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
	if (!Body) return 0.0;
	const auto& Primary = Groups.IsValidIndex(Body->GroupIndex)
		? Captured[Groups[Body->GroupIndex].PrimaryIndex] : *Body;
	return FMath::Max(Primary.PhysicalRadius * Primary.Multiplier,
		Primary.Radius * LocalKilometersPerCentimeter()) * (Body->Radius / Primary.Radius);
}

double UJTSCompressedSpaceComponent::ApparentDiameterRadians(const FVector& Position, const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	if (!Body) return 0.0;
	return 2.0 * FMath::Asin(FMath::Clamp(Body->Radius * VisualScale(Position, Actor)
		/ FMath::Max(1.0, FVector::Distance(Position, PresentationCenter(Position, Actor))), 0.0, 0.999999));
}

double UJTSCompressedSpaceComponent::GroupVisualScale(const FVector& Position, const FCapturedGroup& Group) const
{
	const auto& Primary = Captured[Group.PrimaryIndex];
	const double Distance = FVector::Distance(Position, Primary.Center);
	if (Distance <= Group.Inner) return 1.0;
	const double Range = FMath::Max(UE_DOUBLE_SMALL_NUMBER, FVector::Distance(NavigationPosition(Position), Primary.Navigation));
	const double FarScale = FMath::Clamp(DisplayRadiusKilometers(Primary.Body.Get()) * Distance / (Range * Primary.Radius), 0.00000001, 1.0);
	const double T = FMath::Clamp((Distance - Group.Inner) / (Group.Outer - Group.Inner), 0.0, 1.0);
	// Quintic blend meets both endpoint values and derivatives; all members share it,
	// including orbital offsets, rather than independently deflating their meshes.
	const double Ease = T*T*T*(T*(T*6.0-15.0)+10.0);
	return FMath::Lerp(1.0, FarScale, Ease);
}

double UJTSCompressedSpaceComponent::VisualScale(const FVector& Position, const AActor* Actor) const
{
	const auto* Body = Find(Actor);
	if (!Body) return 1.0;
	if (Groups.IsValidIndex(Body->GroupIndex)) return GroupVisualScale(Position, Groups[Body->GroupIndex]);
	const double Distance = FVector::Distance(Position, Body->Center);
	const double Inner = Body->Radius + NearAltitudeCentimeters;
	if (Distance <= Inner) return 1.0;
	const auto AstronomicalScaleAt = [&](const FVector& P, double D)
	{
		const double Range = FMath::Max(UE_DOUBLE_SMALL_NUMBER, FVector::Distance(NavigationPosition(P), Body->Navigation));
		return FMath::Clamp(DisplayRadiusKilometers(Actor) * D / (Range * Body->Radius), 0.00000001, 1.0);
	};
	// Coincident coordinate bands inherit the larger neighbour's width. Blending
	// against NAV at the current point would retain its sharp derivatives even after
	// easing the blend. Instead sample the astronomical scale at a stable outer shell.
	// Half the nearest separation preserves each body's distant companion view;
	// four own radii also cover the local coordinate cube around a large planet.
	double NearestSquared = UE_DOUBLE_BIG_NUMBER;
	for (const auto& Other : Captured)
		if (Other.Body != Body->Body) NearestSquared = FMath::Min(NearestSquared, FVector::DistSquared(Other.Center, Body->Center));
	const double Outer = FMath::Max(Inner + Body->Radius * 4.0,
		NearestSquared < UE_DOUBLE_BIG_NUMBER ? FMath::Sqrt(NearestSquared) * 0.5 : 0.0);
	if (Distance >= Outer) return AstronomicalScaleAt(Position, Distance);
	const FVector OuterPosition = Body->Center + (Position - Body->Center) * (Outer / Distance);
	const double OuterScale = AstronomicalScaleAt(OuterPosition, Outer);
	const double Transition = FMath::SmoothStep(0.0, 1.0,
		FMath::Clamp((Distance - Inner) / (Outer - Inner), 0.0, 1.0));
	return FMath::Lerp(1.0, OuterScale, Transition);
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
			Visual->SetActorLocation(PresentationCenter(Position, Body.Body.Get()) + (Body.Location - Body.Center) * Ratio);
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
