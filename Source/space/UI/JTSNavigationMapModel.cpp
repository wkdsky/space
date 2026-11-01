#include "space/UI/JTSNavigationMapModel.h"

#include "Engine/World.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Components/JTSPlanetGravityComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/UI/JTSCelestialNavigationLibrary.h"
#include "space/UI/JTSCruiseNavigationWidget.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	struct FMapBody
	{
		FJTSCelestialNavigationBody Source;
		const AActor* Actor = nullptr;
		FVector Center = FVector::ZeroVector;
		FName Group;
		double Radius = 1.0;
		double Gravity = 0.0;
	};
	struct FMapGroup
	{
		FName Id;
		TArray<int32> Members;
		int32 Primary = INDEX_NONE;
		double Extent = 1.0;
		double Range = 0.0;
	};
	void SurfaceAxes(const AJTSPlanetAnchor* Planet, const FVector& Up, FVector& East, FVector& North)
	{
		const FVector Pole = Planet->GetActorQuat().GetUpVector();
		East = FVector::CrossProduct(Pole, Up).GetSafeNormal();
		if (East.IsNearlyZero())
			East = FVector::VectorPlaneProject(Planet->GetActorQuat().GetRightVector(), Up).GetSafeNormal();
		North = FVector::CrossProduct(Up, East).GetSafeNormal();
	}
}

FVector2D FJTSNavigationTerrainPatch::Project(const FVector& WorldPosition, const FVector& PlanetCenter) const
{
	const FVector Delta = WorldPosition - Origin;
	const FVector2D Planar(FVector::DotProduct(Delta, East), -FVector::DotProduct(Delta, North));
	// The far hemisphere projects onto the same plane but is occluded by the planet.
	if (FVector::DotProduct(WorldPosition - PlanetCenter, Up) < 0)
		return (Planar.IsNearlyZero() ? FVector2D(1, 0) : Planar.GetSafeNormal()) * 2;
	return Planar / FMath::Max(1.0f, HalfWidth);
}

FVector2D FJTSNavigationMapModel::SurfaceCoordinates(const FVector& Position, const FVector& Center, const FQuat& PlanetRotation)
{
	const FVector Radial = PlanetRotation.UnrotateVector((Position - Center).GetSafeNormal());
	return FVector2D(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Radial.Z, -1.0, 1.0))),
		Radial.SizeSquared2D() < 1.e-12 ? 0.0 : FMath::RadiansToDegrees(FMath::Atan2(Radial.Y, Radial.X)));
}

double FJTSNavigationMapModel::AdvanceZoom(double Current, double Target, double DeltaSeconds, double TransitionSeconds)
{
	if (!FMath::IsFinite(Current) || !FMath::IsFinite(Target)) return 2.0;
	const double Step = FMath::Max(0.0, DeltaSeconds) / FMath::Max(0.2, TransitionSeconds);
	return FMath::Clamp(Current + FMath::Clamp(Target - Current, -Step, Step), 0.0, 2.0);
}

FVector2D FJTSNavigationMapModel::SystemDialPosition(float BearingDegrees, float RangeCentimeters)
{
	const double Angle = FMath::DegreesToRadians(BearingDegrees);
	const double Reach = FMath::Lerp(0.42, 0.78,
		static_cast<double>(UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(RangeCentimeters)));
	return FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * Reach;
}

FVector FJTSNavigationMapModel::ProjectSystemContact(const FVector& Delta, const FQuat& ShipRotation, double Range)
{
	const FVector Local = ShipRotation.UnrotateVector(Delta);
	const double Distance = Local.Size();
	const double Radius = 0.94 * (1.0 - FMath::Exp(-1.8 * FMath::Sqrt(Distance / FMath::Max(1.0, Range))));
	return Local.GetSafeNormal() * Radius;
}

void FJTSNavigationMapModel::UpdateTerrain(const AJTSPlanetAnchor* Planet, const FVector& Position, float HalfWidth)
{
	Terrain = MakeShared<FJTSNavigationTerrainPatch>();
	Terrain->Origin = Position;
	Terrain->Up = (Position - Planet->GetPlanetCenter()).GetSafeNormal();
	SurfaceAxes(Planet, Terrain->Up, Terrain->East, Terrain->North);
	Terrain->Radius = FVector::Distance(Position, Planet->GetPlanetCenter());
	Terrain->HalfWidth = FMath::Min(FMath::Max(500.0f, HalfWidth), static_cast<float>(Terrain->Radius * 0.5));
}

FJTSNavigationMapFrame FJTSNavigationMapModel::Build(const AJTSSpaceWorldManager* Manager,
	const AJTSSpacecraftActor* Ship, const AJTSCharacter* Character)
{
	FJTSNavigationMapFrame Frame;
	if (!IsValid(Manager)) return Frame;
	const auto& Settings = Manager->NavigationMapSettings;
	Frame.TransitionSeconds = Settings.TransitionSeconds;
	Frame.bHasShip = IsValid(Ship);
	Frame.bPlayerOnFoot = IsValid(Character) && !Character->IsBoarded();
	const auto* Gravity = Frame.bPlayerOnFoot ? Character->FindComponentByClass<UJTSPlanetGravityComponent>() : nullptr;
	const AJTSPlanetAnchor* GroundPlanet = Gravity ? Gravity->GetPlanetAnchor() : nullptr;
	if (!IsValid(GroundPlanet) && IsValid(Ship) && Ship->IsLanded()) GroundPlanet = Ship->GetLandedPlanet();
	if (Frame.bPlayerOnFoot && !IsValid(GroundPlanet)) GroundPlanet = Manager->GetCurrentPlanet();
	Frame.bGroundMap = IsValid(GroundPlanet) && (Frame.bPlayerOnFoot || (IsValid(Ship) && Ship->IsLanded()));
	if (Frame.bGroundMap)
	{
		const FVector PlayerLocation = Frame.bPlayerOnFoot ? Character->GetActorLocation() : Ship->GetActorLocation();
		UpdateTerrain(GroundPlanet, PlayerLocation, Settings.GroundMapHalfWidth);
		Frame.Terrain = Terrain;
		Frame.GroundHalfWidth = Terrain->HalfWidth;
		Frame.PlayerPosition = Terrain->Project(PlayerLocation, GroundPlanet->GetPlanetCenter());
		if (IsValid(Ship))
		{
			Frame.ShipPosition = Terrain->Project(Ship->GetActorLocation(), GroundPlanet->GetPlanetCenter());
			Frame.ShipHeading = FVector2D(FVector::DotProduct(Ship->GetActorForwardVector(), Terrain->East),
				-FVector::DotProduct(Ship->GetActorForwardVector(), Terrain->North)).GetSafeNormal();
			Frame.ShipSurfaceDistance = GroundPlanet->ApproximateSurfaceArcDistance(PlayerLocation, Ship->GetActorLocation());
		}
		const FVector Forward = Frame.bPlayerOnFoot ? Character->GetActorForwardVector() : Ship->GetActorForwardVector();
		Frame.PlayerHeading = FVector2D(FVector::DotProduct(Forward, Terrain->East), -FVector::DotProduct(Forward, Terrain->North)).GetSafeNormal();
		const FVector2D Coordinates = SurfaceCoordinates(PlayerLocation, GroundPlanet->GetPlanetCenter(), GroundPlanet->GetActorQuat());
		Frame.Coordinates = FString::Printf(TEXT("%07.3f%s  %08.3f%s"), FMath::Abs(Coordinates.X),
			Coordinates.X >= 0 ? TEXT("N") : TEXT("S"), FMath::Abs(Coordinates.Y), Coordinates.Y >= 0 ? TEXT("E") : TEXT("W"));
		Frame.PlanetName = GroundPlanet->GetNavigationDisplayName().ToString();
		Frame.Stage = EJTSNavigationMapStage::Surface;
	}
	if (!IsValid(Ship)) return Frame;

	const UJTSCompressedSpaceComponent* Space = Manager->CompressedSpace;
	const bool bCompressed = Space && Space->IsConfigured();
	const FVector ShipLocation = Ship->GetActorLocation();
	const FVector Observer = bCompressed ? Space->NavigationPosition(ShipLocation) : ShipLocation / 100000.0;
	const AActor* NearBody = bCompressed ? Space->ContainingNearFlightBody(ShipLocation)
		: Manager->GetNavigationReferencePlanet(Ship);
	const bool bNear = NearBody && !Manager->IsCruisePresentationActive(Ship);
	if (bNear) FocusBody = NearBody;
	if (Frame.bGroundMap) FocusBody = GroundPlanet;
	TArray<FMapBody> Bodies;
	TArray<FMapGroup> Groups;
	for (const auto& Source : UJTSCelestialNavigationLibrary::GatherBodies(Manager, Ship))
	{
		FMapBody Body;
		Body.Source = Source;
		Body.Actor = Source.Planet ? static_cast<const AActor*>(Source.Planet) : Source.VisualActor.Get();
		Body.Center = Source.Center / 100000.0;
		Body.Radius = FMath::Max(0.001, Source.VisualRadius / 100000.0);
		Body.Gravity = Source.Planet && Source.Planet->IsGravityEnabled() ? Source.Planet->GetGravityStrength() : 0.0;
		Body.Group = Body.Actor ? Body.Actor->GetFName() : NAME_None;
		if (bCompressed)
			for (const auto& Entry : Space->Bodies)
				if (Entry.Actor == Body.Actor || Entry.Actor == Source.VisualActor)
				{
					Body.Center = Entry.NavigationPositionKilometers;
					Body.Radius = FMath::Max(0.001, Entry.RadiusKilometers);
					Body.Gravity = Source.Planet ? Body.Gravity : Entry.NavigationSurfaceGravity;
					if (!Entry.PresentationGroup.IsNone()) Body.Group = Entry.PresentationGroup;
					break;
				}
		int32 GroupIndex = Groups.IndexOfByPredicate([&](const FMapGroup& G) { return G.Id == Body.Group; });
		if (GroupIndex == INDEX_NONE) { GroupIndex = Groups.AddDefaulted(); Groups[GroupIndex].Id = Body.Group; }
		auto& Group = Groups[GroupIndex];
		Group.Members.Add(Bodies.Num());
		if (Group.Primary == INDEX_NONE || Body.Radius > Bodies[Group.Primary].Radius) Group.Primary = Bodies.Num();
		Bodies.Add(MoveTemp(Body));
	}
	if (Groups.IsEmpty()) return Frame;
	int32 Nearest = INDEX_NONE, Active = INDEX_NONE, NearGroup = INDEX_NONE;
	double BestScore = TNumericLimits<double>::Max();
	for (int32 I = 0; I < Groups.Num(); ++I)
	{
		auto& Group = Groups[I];
		const auto& Primary = Bodies[Group.Primary];
		Group.Extent = Primary.Radius * FMath::Max(2.0f, Settings.MinimumSystemRadii);
		for (int32 Member : Group.Members)
		{
			Group.Extent = FMath::Max(Group.Extent, (FVector::Distance(Primary.Center, Bodies[Member].Center)
				+ Bodies[Member].Radius) * FMath::Max(1.2f, Settings.SystemExtentMultiplier));
			if ((bNear && Bodies[Member].Actor == NearBody) || (Frame.bGroundMap && Bodies[Member].Actor == GroundPlanet)) NearGroup = I;
		}
		Group.Range = FVector::Distance(Observer, Primary.Center);
		const double Score = Group.Range / Group.Extent;
		if (Score < BestScore) { BestScore = Score; Nearest = I; }
		if (Group.Id == ActiveSystem) Active = I;
	}
	if (NearGroup != INDEX_NONE) Active = NearGroup;
	else if (Active == INDEX_NONE || Groups[Active].Range / Groups[Active].Extent > BestScore * (1.0 + Settings.StageHysteresis)) Active = Nearest;
	const auto& Group = Groups[Active];
	const auto& Primary = Bodies[Group.Primary];
	ActiveSystem = Group.Id;
	Frame.SystemId = Group.Id;
	Frame.SystemName = Primary.Source.DisplayName.ToString();
	const double ExitScale = PreviousStage == EJTSNavigationMapStage::System ? 1.0 + Settings.StageHysteresis : 1.0;
	Frame.Stage = Frame.bGroundMap || bNear ? EJTSNavigationMapStage::Surface
		: Group.Range <= Group.Extent * ExitScale ? EJTSNavigationMapStage::System : EJTSNavigationMapStage::Interstellar;
	PreviousStage = Frame.Stage;
	// The observer is always the origin. Full hull orientation preserves yaw, pitch and roll.
	const double MapExtent = Group.Extent / FMath::Max(1.2f, Settings.SystemExtentMultiplier) * 1.35;
	if (!Frame.bGroundMap) Frame.ShipPosition = FVector2D::ZeroVector;
	Frame.ShipBearing = 0.0f;
	const bool bFocusInGroup = Group.Members.ContainsByPredicate([&](int32 I) { return Bodies[I].Actor == FocusBody.Get(); });
	if (!bFocusInGroup) FocusBody = Primary.Actor;
	for (int32 Member : Group.Members)
	{
		const auto& Body = Bodies[Member];
		auto& Contact = Frame.Bodies.AddDefaulted_GetRef();
		Contact.Id = Body.Actor->GetFName();
		Contact.Name = Body.Source.DisplayName.ToString();
		const FVector Radar = ProjectSystemContact(Body.Center - Observer, Ship->GetActorQuat(), MapExtent);
		Contact.PlanePosition = FVector2D(Radar.Y, -Radar.X * 0.55);
		Contact.Height = Radar.Z;
		Contact.Position = Contact.PlanePosition - FVector2D(0, Radar.Z * 0.65);
		Contact.Radius = FMath::Clamp(static_cast<float>(FMath::Sqrt(Body.Radius / MapExtent) * 0.4),
			Member == Group.Primary ? 0.26f : 0.035f, 0.34f);
		Contact.Gravity = FMath::Max(0.0, Body.Gravity / 980.0);
		Contact.Palette = Body.Source.PaletteIndex;
		Contact.bCharted = Body.Source.bShowOnStarChart;
		Contact.bPrimary = Member == Group.Primary;
		Contact.bFocus = Body.Actor == FocusBody.Get();
		if (Contact.bFocus && !Frame.bGroundMap)
		{
			Frame.PlanetName = Contact.Name;
			Frame.Altitude = Body.Source.Planet ? Manager->GetNavigationSurfaceRangeCentimeters(Ship, Body.Source.Planet) : Body.Source.RangeCentimeters;
		}
	}
	for (const auto& OtherGroup : Groups)
	{
		const auto& Star = Bodies[OtherGroup.Primary];
		if (!OtherGroup.Members.ContainsByPredicate([&](int32 I) { return Bodies[I].Source.bShowOnStarChart; })) continue;
		const FVector Direction = (Star.Center - Observer).GetSafeNormal();
		const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Direction, Ship->GetActorRightVector()),
			FVector::DotProduct(Direction, Ship->GetActorForwardVector())));
		auto& System = Frame.Systems.AddDefaulted_GetRef();
		System.Id = OtherGroup.Id;
		System.Name = Star.Source.DisplayName.ToString();
		System.RangeCentimeters = FMath::Max(0.0, OtherGroup.Range - Star.Radius) * 100000.0;
		System.Position = SystemDialPosition(Bearing, System.RangeCentimeters);
		System.Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FVector::DotProduct(Direction, Ship->GetActorUpVector()), -1.0, 1.0)));
		System.Palette = Star.Source.PaletteIndex;
		System.bActive = OtherGroup.Id == Group.Id;
	}
	return Frame;
}
