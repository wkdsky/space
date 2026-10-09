// Copyright Epic Games, Inc. All Rights Reserved.
#include "JTSSpacecraftSurfaceEnvelopeComponent.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"

UJTSSpacecraftSurfaceEnvelopeComponent::UJTSSpacecraftSurfaceEnvelopeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSSpacecraftSurfaceEnvelopeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSSpacecraftSurfaceEnvelopeComponent, Frame);
	DOREPLIFETIME(UJTSSpacecraftSurfaceEnvelopeComponent, bFollowing);
}

void UJTSSpacecraftSurfaceEnvelopeComponent::Reset()
{
	Frame = {};
	bFollowing = false;
	Samples.Reset();
	SamplePlanet.Reset();
	SampleElapsed = BIG_NUMBER;
}

bool UJTSSpacecraftSurfaceEnvelopeComponent::GetTakeoffClearanceRadius(double& OutRadius) const
{
	if (!Frame.bValid || Samples.IsEmpty()) return false;
	OutRadius = 0;
	for (const FSample& Sample : Samples) OutRadius = FMath::Max(OutRadius, Sample.Radius);
	OutRadius += Frame.HullClearance + Settings.FlatClearance
		+ FMath::Min(Settings.MaximumRoughnessClearance, Roughness * Settings.RoughnessClearanceScale)
		+ Settings.HoverOffset;
	return true;
}

bool UJTSSpacecraftSurfaceEnvelopeComponent::BuildSamples(AJTSPlanetAnchor* Planet, const FVector& Position, float Radius, const FVector& Velocity, bool bTakeoffSampling)
{
	const auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	const FVector Centre = Planet->GetPlanetCenter();
	const FVector Up = Planet->GetRadialUpVector(Position).GetSafeNormal();
	FJTSPlanetSurfaceHit Ground;
	if (!Ship) return false;
	if (!Planet->TraceToSurface(Position, Ground))
	{
		// Continue the shell across a mesh fissure using neighbouring real surfaces. A missing
		// centre ray rejects landing; it must not disable flight clearance at precisely that spot.
		if (!Frame.bValid || SamplePlanet.Get() != Planet) return false;
		Ground.ImpactPoint = Frame.GroundLocation;
		Ground.ImpactNormal = Frame.Normal;
	}
	const double GroundRadius = FVector::Distance(Ground.ImpactPoint, Centre);
	if (GroundRadius < 1 || Up.IsNearlyZero()) return false;
	FVector X = FVector::VectorPlaneProject(SampleAxis, Up).GetSafeNormal(), Y;
	if (X.IsNearlyZero()) Up.FindBestAxisVectors(X, Y);
	Y = FVector::CrossProduct(Up, X).GetSafeNormal();
	SampleAxis = X;
	SampleOrigin = Ground.ImpactPoint;
	SampleRadius = FMath::Min(Radius, float(GroundRadius * 0.8));
	FootprintRadius = Ship->GetSurfaceFlightFootprintRadius();
	Samples.Reset(90);
	Roughness = 0;
	const auto Probe = [&](double Px, double Py)
	{
		const FVector Offset = X * Px + Y * Py;
		const double Arc = Offset.Size() / GroundRadius;
		const FVector Direction = Arc > UE_DOUBLE_SMALL_NUMBER
			? Up * FMath::Cos(Arc) + Offset.GetSafeNormal() * FMath::Sin(Arc) : Up;
		FJTSPlanetSurfaceHit Hit;
		if (!Planet->TraceToSurface(Centre + Direction * (GroundRadius + Settings.ActivationAltitude + SampleRadius), Hit)) return;
		Samples.Add({Hit.ImpactPoint, FVector::Distance(Hit.ImpactPoint, Centre)});
		if (Offset.Size() <= FMath::Max(FootprintRadius * 1.5f, 900.0f))
		{
			Roughness = FMath::Max(Roughness, float(FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - Ground.ImpactPoint, Ground.ImpactNormal))));
		}
	};
	// A heading-independent neighbourhood covers forward, reverse, strafe and turns equally.
	for (int32 I = -4; I <= 4; ++I)
		for (int32 J = -4; J <= 4; ++J) Probe(I * SampleRadius / 4, J * SampleRadius / 4);
	// A stationary departure has no prediction corridor. Search the wider surroundings for a
	// crater rim, while retaining the ordinary local grid so expanding the radius loses no detail.
	if (bTakeoffSampling)
	{
		for (int32 I = -8; I <= 8; ++I)
			for (int32 J = -8; J <= 8; ++J)
				Probe(I * SampleRadius / 8, J * SampleRadius / 8);
		const float LocalRadius = FMath::Min(SampleRadius, Settings.MinimumSamplingRadius);
		for (int32 I = -4; I <= 4; ++I)
			for (int32 J = -4; J <= 4; ++J) Probe(I * LocalRadius / 4, J * LocalRadius / 4);
	}
	// Keep the physical footprint densely sampled even when speed expands the distant prediction.
	const float Near = FMath::Max(100.0f, FootprintRadius);
	for (int32 I = -1; I <= 1; ++I)
		for (int32 J = -1; J <= 1; ++J) if (I || J) Probe(I * Near, J * Near);
	// Distant grid spacing grows with speed. Densely sample the whole travel corridor too,
	// so a narrow ridge cannot slip between grid nodes until it is underneath the hull.
	const FVector Travel = FVector::VectorPlaneProject(Velocity, Up).GetSafeNormal();
	if (!Travel.IsNearlyZero() && Velocity.Size() > 100)
	{
		const FVector Across = FVector::CrossProduct(Up, Travel);
		const int32 Count = FMath::Clamp(FMath::CeilToInt(SampleRadius / FMath::Max(200.0f, FootprintRadius * 0.5f)), 4, 40);
		for (int32 I = 1; I <= Count; ++I)
			for (int32 Lane = -1; Lane <= 1; ++Lane)
			{
				const FVector Offset = Travel * (I * SampleRadius / Count) + Across * (Lane * FootprintRadius);
				Probe(FVector::DotProduct(Offset, X), FVector::DotProduct(Offset, Y));
			}
	}
	SamplePlanet = Planet;
	if (const AActor* Surface = Planet->GetGameplaySurfaceActor()) SurfaceTransform = Surface->GetActorTransform();
	return !Samples.IsEmpty();
}

bool UJTSSpacecraftSurfaceEnvelopeComponent::Evaluate(AJTSPlanetAnchor* Planet, const FVector& Position, FJTSSurfaceEnvelopeFrame& Out) const
{
	Out = {};
	const auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !IsValid(Planet) || SamplePlanet.Get() != Planet || Samples.IsEmpty()) return false;
	const FVector Centre = Planet->GetPlanetCenter();
	const FVector Up = (Position - Centre).GetSafeNormal();
	const double Radius = FVector::Distance(SampleOrigin, Centre);
	const double Grade = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Settings.MaximumTerrainGradeDegrees, 3.0f, 30.0f)));
	const double Round = FMath::Max(50.0f, Settings.ShoulderRoundingDistance);
	const double Width = FMath::Max(1.0f, Settings.SmoothMaximumWidth);
	double Maximum = -DBL_MAX, Nearest = DBL_MAX;
	TArray<double, TInlineAllocator<96>> Heights;
	TArray<FVector, TInlineAllocator<96>> Gradients;
	FVector Ground = SampleOrigin;
	for (const auto& Sample : Samples)
	{
		const FVector Direction = (Sample.Location - Centre).GetSafeNormal();
		const double Arc = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, Direction), -1.0, 1.0)) * Radius;
		if (Arc < Nearest) { Nearest = Arc; Ground = Sample.Location; }
		const double Distance = FMath::Max(0.0, Arc - FootprintRadius);
		const double Shoulder = Grade * (FMath::Sqrt(Distance * Distance + Round * Round) - Round);
		const double Height = Sample.Radius - Shoulder;
		Heights.Add(Height);
		Maximum = FMath::Max(Maximum, Height);
		const FVector Toward = FVector::VectorPlaneProject(Direction, Up).GetSafeNormal();
		Gradients.Add(Toward * (Grade * Distance / FMath::Sqrt(Distance * Distance + Round * Round)));
	}
	// An upper smooth maximum preserves mountain tops. Averaging raw heights would cut through them.
	double Sum = 0;
	FVector Gradient = FVector::ZeroVector;
	for (int32 I = 0; I < Heights.Num(); ++I)
	{
		const double Weight = FMath::Exp((Heights[I] - Maximum) / Width);
		Sum += Weight;
		Gradient += Gradients[I] * Weight;
	}
	Gradient /= FMath::Max(Sum, UE_DOUBLE_SMALL_NUMBER);
	const FVector Normal = (Up - Gradient).GetSafeNormal();
	FVector Forward = FVector::VectorPlaneProject(Ship->GetActorForwardVector(), Normal).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = FVector::VectorPlaneProject(Ship->GetActorUpVector(), Normal).GetSafeNormal();
	if (Forward.IsNearlyZero()) Normal.FindBestAxisVectors(Forward, Gradient);
	const FQuat Level = FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
	// Size the shell for a level hull; a temporary pitch cannot pump the shell height itself.
	const float Clearance = Ship->GetLandingCollisionClearanceForRotation(Level, Up);
	const float Extra = FMath::Min(Settings.MaximumRoughnessClearance, Roughness * Settings.RoughnessClearanceScale);
	const double EnvelopeRadius = Maximum + Width * FMath::Loge(Sum) + Clearance + Settings.FlatClearance + Extra;
	Out.bValid = true;
	Out.Location = Centre + Up * EnvelopeRadius;
	Out.RadialUp = Up;
	Out.Normal = Normal;
	Out.GroundLocation = Ground;
	Out.HullClearance = Clearance;
	Out.Roughness = Roughness;
	return true;
}

void UJTSSpacecraftSurfaceEnvelopeComponent::Refresh(AJTSPlanetAnchor* Planet, const FVector& Position,
    const FVector& Velocity, float GroundAltitude, float DeltaTime, bool bAllowFollowingExit, bool bTakeoffSampling)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	const float ActiveAltitude = Frame.bValid
		? FMath::Max(Settings.ActivationAltitude, float(FVector::Distance(Frame.Location, Frame.GroundLocation)) + Settings.FollowingExitHeight + 300)
		: Settings.ActivationAltitude;
	if (IsValid(Planet) && GroundAltitude >= BIG_NUMBER * 0.5f && Frame.bValid && SamplePlanet.Get() == Planet)
		GroundAltitude = FVector::Distance(Position, Planet->GetPlanetCenter()) - FVector::Distance(Frame.GroundLocation, Planet->GetPlanetCenter());
	// A validated ground departure must finish even when a deep basin's rim is above the
	// ordinary flight activation band. Losing the frame here would stall the clearance phase.
	if (!IsValid(Planet) || (!bTakeoffSampling && GroundAltitude > ActiveAltitude))
	{
		Reset(); return;
	}
	SampleElapsed += DeltaTime;
	const float Radius = bTakeoffSampling ? FMath::Max(Settings.MinimumSamplingRadius, Settings.MaximumSamplingRadius)
		: FMath::Clamp(float(Velocity.Size()) * Settings.PredictionTime + FootprintRadius * 2,
		Settings.MinimumSamplingRadius, FMath::Max(Settings.MinimumSamplingRadius, Settings.MaximumSamplingRadius));
	const AActor* Surface = Planet->GetGameplaySurfaceActor();
	if (Samples.IsEmpty() || SamplePlanet.Get() != Planet || SampleElapsed >= Settings.SamplingInterval
		|| FVector::Distance(Position, SampleOrigin) > SampleRadius * 0.6f
		|| (Surface && !Surface->GetActorTransform().Equals(SurfaceTransform, 0.01)))
	{
		SampleElapsed = 0;
		if (!BuildSamples(Planet, Position, Radius, Velocity, bTakeoffSampling)) { Reset(); return; }
	}
	FJTSSurfaceEnvelopeFrame NewFrame;
	if (!Evaluate(Planet, Position, NewFrame)) { Reset(); return; }
	if (Frame.bValid)
	{
		const float Alpha = 1 - FMath::Exp(-Settings.NormalResponse * DeltaTime);
		NewFrame.Normal = FMath::Lerp(Frame.Normal, NewFrame.Normal, Alpha).GetSafeNormal();
	}
	Frame = NewFrame;
	const float Height = FVector::DotProduct(Position - Frame.Location, Frame.RadialUp);
	// Terrain dropping away is not a request to return to camera-controlled pitch. Keep one
	// attitude source through mountain crests; only a deliberate climb releases the follow mode.
	if (bFollowing && bAllowFollowingExit) bFollowing = Height < FMath::Max(Settings.FollowingExitHeight, Settings.FollowingEnterHeight + 100);
	else if (!bFollowing) bFollowing = Height <= Settings.FollowingEnterHeight;
	if (Settings.bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Frame.Location, 35, 12, FColor::Cyan, false, Settings.SamplingInterval);
		DrawDebugLine(GetWorld(), Frame.Location, Frame.Location + Frame.Normal * 300, FColor::Green, false, Settings.SamplingInterval);
		for (const auto& Sample : Samples) DrawDebugPoint(GetWorld(), Sample.Location, 5, FColor::Yellow, false, Settings.SamplingInterval);
	}
}
