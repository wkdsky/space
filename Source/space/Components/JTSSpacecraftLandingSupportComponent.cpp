// Copyright Epic Games, Inc. All Rights Reserved.
#include "space/Components/JTSSpacecraftLandingSupportComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
	USceneComponent* FindSceneComponent(const AActor* Owner, FName Name)
	{
		if (!Owner || Name.IsNone()) return nullptr;
		TInlineComponentArray<USceneComponent*> Components(Owner);
		for (USceneComponent* Component : Components)
		{
			if (Component->GetFName() == Name) return Component;
		}
		return nullptr;
	}

	float AngleDegrees(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0)));
	}

	FVector TangentForward(const FVector& Forward, const FVector& Up)
	{
		FVector Result = FVector::VectorPlaneProject(Forward, Up).GetSafeNormal();
		if (Result.IsNearlyZero()) { FVector Right; Up.FindBestAxisVectors(Result, Right); }
		return Result;
	}

	// Least-squares height plane in a local gravity frame. Double precision keeps far-away planets stable.
	bool FitSupportNormal(const TArray<FVector>& Points, const FVector& Up, FVector& Normal)
	{
		if (Points.Num() < 3) return false;
		FVector X, Y; Up.FindBestAxisVectors(X, Y);
		FVector Mean = FVector::ZeroVector;
		for (const FVector& P : Points) Mean += P;
		Mean /= Points.Num();
		double XX = 0, XY = 0, YY = 0, XZ = 0, YZ = 0;
		for (const FVector& P : Points)
		{
			const FVector D = P - Mean;
			const double Px = FVector::DotProduct(D, X), Py = FVector::DotProduct(D, Y), Pz = FVector::DotProduct(D, Up);
			XX += Px * Px; XY += Px * Py; YY += Py * Py; XZ += Px * Pz; YZ += Py * Pz;
		}
		const double Determinant = XX * YY - XY * XY;
		if (Determinant <= 1.0) return false;
		Normal = (Up - X * ((XZ * YY - YZ * XY) / Determinant)
			- Y * ((YZ * XX - XZ * XY) / Determinant)).GetSafeNormal();
		return !Normal.IsNearlyZero();
	}

	double Cross2D(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	// Convex support polygon, projected along real gravity. Return inward distance to its closest edge.
	float SupportMargin(const TArray<FJTSLandingFootContact>& Contacts, const FVector& CoM, const FVector& GravityUp)
	{
		FVector X, Y; GravityUp.FindBestAxisVectors(X, Y);
		TArray<FVector2D> Points;
		for (const auto& Contact : Contacts)
		{
			const FVector D = Contact.Location - CoM;
			Points.Emplace(FVector::DotProduct(D, X), FVector::DotProduct(D, Y));
		}
		Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
		TArray<FVector2D> Hull;
		for (const FVector2D& P : Points)
		{
			while (Hull.Num() >= 2 && Cross2D(Hull.Last() - Hull[Hull.Num()-2], P - Hull.Last()) <= 0) Hull.Pop();
			Hull.Add(P);
		}
		const int32 LowerCount = Hull.Num();
		for (int32 I = Points.Num()-2; I >= 0; --I)
		{
			const FVector2D P = Points[I];
			while (Hull.Num() > LowerCount && Cross2D(Hull.Last() - Hull[Hull.Num()-2], P - Hull.Last()) <= 0) Hull.Pop();
			Hull.Add(P);
		}
		if (Hull.Num() < 4) return -BIG_NUMBER;
		Hull.Pop();
		float Margin = BIG_NUMBER;
		for (int32 I = 0; I < Hull.Num(); ++I)
		{
			const FVector2D Edge = Hull[(I+1) % Hull.Num()] - Hull[I];
			Margin = FMath::Min(Margin, float(Cross2D(Edge, -Hull[I]) / FMath::Max(1.0, Edge.Size())));
		}
		return Margin;
	}
}

UJTSSpacecraftLandingSupportComponent::UJTSSpacecraftLandingSupportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSSpacecraftLandingSupportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSSpacecraftLandingSupportComponent, FootContacts);
}

void UJTSSpacecraftLandingSupportComponent::SetContacts(const TArray<FJTSLandingFootContact>& Contacts)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		FootContacts = Contacts;
		GetOwner()->ForceNetUpdate();
	}
}

bool UJTSSpacecraftLandingSupportComponent::HasValidRig() const
{
	if (Feet.Num() < 3 || Feet.Num() > 8) return false;
	TSet<FName> Names;
	for (const auto& Foot : Feet)
	{
		if (!FindSceneComponent(GetOwner(), Foot.FootComponentName)
			|| !FindSceneComponent(GetOwner(), Foot.StrutComponentName)
			|| Names.Contains(Foot.FootComponentName) || Foot.PadHalfExtent.GetMin() <= 0
			|| Foot.DeployedContactFrame.ContainsNaN() || Foot.StrutRestLength <= Foot.MaxCompression
			|| Foot.MaxExtension < 0 || Foot.MaxCompression < 0) return false;
		Names.Add(Foot.FootComponentName);
	}
	return true;
}

bool UJTSSpacecraftLandingSupportComponent::ProbeFoot(AJTSPlanetAnchor* Planet, const FTransform& Pose,
	const FJTSLandingFootDefinition& Foot, FJTSLandingFootContact& OutContact, EJTSLandingValidationFailure& OutFailure) const
{
	const FVector Up = Pose.GetUnitAxis(EAxis::Z);
	const FVector Sole = Pose.TransformPosition(Foot.DeployedContactFrame.GetLocation());
	const float Scale = Pose.GetScale3D().GetAbsMax();
	const auto Trace = [this, Planet, Up, &Foot, Scale](const FVector& Point, FHitResult& Hit)
	{
		const FVector Start = Point + Up * (Foot.MaxCompression * Scale + 30.0f);
		const FVector End = Point - Up * (Foot.MaxExtension * Scale + 30.0f);
		if (!Planet->TraceGameplaySurfaceSegment(Start, End, Hit) || !Hit.bBlockingHit) return false;
		// An unrelated rock/actor over a real mesh is not silently accepted as terrain underneath it.
		FHitResult Obstacle;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSLandingFootObstacle), true, GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Obstacle, Start, Hit.ImpactPoint - Up * 1.0f, ECC_Visibility, Params)
			&& !Planet->OwnsGameplaySurfaceActor(Obstacle.GetActor())
			&& FVector::DistSquared(Obstacle.ImpactPoint, Hit.ImpactPoint) > 4.0) return false;
		return true;
	};
	FHitResult CentreHit;
	if (!Trace(Sole, CentreHit)) { OutFailure = EJTSLandingValidationFailure::UnsupportedFoot; return false; }
	const FVector Normal = CentreHit.ImpactNormal.GetSafeNormal();
	const float Extension = FVector::DotProduct(Sole - CentreHit.ImpactPoint, Up);
	if (Extension < -Foot.MaxCompression * Scale - 0.5f || Extension > Foot.MaxExtension * Scale + 0.5f)
	{
		OutFailure = EJTSLandingValidationFailure::GearTravelExceeded; return false;
	}
	if (Normal.IsNearlyZero() || AngleDegrees(Normal, Up) > Settings.MaxFootTiltDegrees)
	{
		OutFailure = EJTSLandingValidationFailure::UnevenFootSurface; return false;
	}
	const FVector X = TangentForward(Pose.TransformVectorNoScale(Foot.DeployedContactFrame.GetUnitAxis(EAxis::X)), Normal);
	const FVector Y = FVector::CrossProduct(Normal, X).GetSafeNormal();
	float Roughness = 0;
	// A centre-only trace misses ledges, fissures and a foot balanced on a point. Check all nine sole samples.
	for (int32 Ix = -1; Ix <= 1; ++Ix)
	{
		for (int32 Iy = -1; Iy <= 1; ++Iy)
		{
			if (Ix == 0 && Iy == 0) continue;
			const FVector Point = CentreHit.ImpactPoint + X * (Ix * Foot.PadHalfExtent.X * Scale)
				+ Y * (Iy * Foot.PadHalfExtent.Y * Scale) + Up * Extension;
			FHitResult Hit;
			if (!Trace(Point, Hit)) { OutFailure = EJTSLandingValidationFailure::UnsupportedFoot; return false; }
			Roughness = FMath::Max(Roughness, float(FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - CentreHit.ImpactPoint, Normal))));
			if (Roughness > Settings.MaxPadRoughness
				|| AngleDegrees(Hit.ImpactNormal.GetSafeNormal(), Normal) > Settings.MaxPadNormalVariationDegrees)
			{
				OutFailure = EJTSLandingValidationFailure::UnevenFootSurface; return false;
			}
		}
	}
	OutContact.FootComponentName = Foot.FootComponentName;
	OutContact.StrutComponentName = Foot.StrutComponentName;
	OutContact.Location = CentreHit.ImpactPoint;
	OutContact.Normal = Normal;
	OutContact.Extension = Extension;
	OutContact.Roughness = Roughness;
	return true;
}

bool UJTSSpacecraftLandingSupportComponent::ValidatePose(AJTSPlanetAnchor* Planet, FTransform Pose,
	FJTSPlanetLandingValidationResult& OutResult) const
{
	OutResult = FJTSPlanetLandingValidationResult();
	OutResult.Planet = Planet;
	OutResult.LandingTransform = Pose;
	if (!IsValid(Planet)) { OutResult.Failure = EJTSLandingValidationFailure::NoPlanet; return false; }
	if (!HasValidRig()) { OutResult.Failure = EJTSLandingValidationFailure::MissingLandingGear; return false; }
	const FVector Up = Pose.GetUnitAxis(EAxis::Z), RadialUp = Planet->GetRadialUpVector(Pose.GetLocation()).GetSafeNormal();
	OutResult.GroundSlopeDegrees = AngleDegrees(Up, RadialUp);
	if (OutResult.GroundSlopeDegrees > Settings.MaxHullSlopeDegrees)
	{
		OutResult.Failure = EJTSLandingValidationFailure::TooSteep; return false;
	}
	FVector Mean = FVector::ZeroVector;
	for (const auto& Foot : Feet)
	{
		FJTSLandingFootContact Contact;
		if (!ProbeFoot(Planet, Pose, Foot, Contact, OutResult.Failure)) return false;
		OutResult.FootContacts.Add(Contact);
		Mean += Contact.Location;
	}
	Mean /= Feet.Num();
	OutResult.GroundLocation = Mean;
	OutResult.GroundNormal = Up;
	OutResult.LandingClearance = FVector::DotProduct(Pose.GetLocation() - Mean, Up);
	OutResult.StabilityMargin = SupportMargin(OutResult.FootContacts,
		Pose.TransformPosition(Settings.CentreOfMassLocal), RadialUp);
	if (OutResult.StabilityMargin < Settings.MinimumStabilityMargin)
	{
		OutResult.Failure = EJTSLandingValidationFailure::UnstableSupport; return false;
	}
	const AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !Ship->CanOccupyLandingTransform(Pose))
	{
		OutResult.Failure = EJTSLandingValidationFailure::CollisionBlocked; return false;
	}
	OutResult.bIsValid = true;
	OutResult.Failure = EJTSLandingValidationFailure::None;
	if (Settings.bDrawDebug)
	{
		for (const auto& Contact : OutResult.FootContacts)
		{
			DrawDebugPoint(GetWorld(), Contact.Location, 12, FColor::Green, false, 0.3f);
			DrawDebugLine(GetWorld(), Contact.Location, Contact.Location + Contact.Normal * 60, FColor::Cyan, false, 0.3f);
		}
	}
	return true;
}

bool UJTSSpacecraftLandingSupportComponent::FitCandidate(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& Forward, FJTSPlanetLandingValidationResult& OutResult) const
{
	const AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	const FVector RadialUp = Planet->GetRadialUpVector(Ground).GetSafeNormal();
	const FVector Scale = Ship->GetActorScale3D();
	float NominalHeight = 0;
	for (const auto& Foot : Feet) NominalHeight -= Foot.DeployedContactFrame.GetLocation().Z * Scale.Z;
	NominalHeight /= Feet.Num();
	FTransform Pose(FRotationMatrix::MakeFromXZ(TangentForward(Forward, RadialUp), RadialUp).ToQuat(), Ground + RadialUp * NominalHeight, Scale);
	double FinalMaximumHeight = 0;
	// Refine stance twice. Ground is always the candidate selected near the pilot's original ground point.
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		TArray<FVector> Points;
		for (const auto& Foot : Feet)
		{
			const FVector Sole = Pose.TransformPosition(Foot.DeployedContactFrame.GetLocation());
			FJTSPlanetSurfaceHit Hit;
			if (!Planet->ProbeSurfaceAlongGravity(Sole + RadialUp * 600, 1400, Hit))
			{
				OutResult.Failure = EJTSLandingValidationFailure::UnsupportedFoot; return false;
			}
			Points.Add(Hit.ImpactPoint);
		}
		FVector Up;
		if (!FitSupportNormal(Points, RadialUp, Up)) { OutResult.Failure = EJTSLandingValidationFailure::UnstableSupport; return false; }
		if (AngleDegrees(Up, RadialUp) > Settings.MaxHullSlopeDegrees)
		{
			OutResult.Failure = EJTSLandingValidationFailure::TooSteep; return false;
		}
		Pose.SetRotation(FRotationMatrix::MakeFromXZ(TangentForward(Forward, Up), Up).ToQuat());
		// Intersect every strut's allowable height interval; never hide an unsupported leg by lifting the hull.
		double MinHeight = -BIG_NUMBER, MaxHeight = BIG_NUMBER, MeanHeight = 0;
		for (int32 I = 0; I < Feet.Num(); ++I)
		{
			const double Height = FVector::DotProduct(Points[I] - Ground
				- Pose.TransformVector(Feet[I].DeployedContactFrame.GetLocation()), Up);
			MinHeight = FMath::Max(MinHeight, Height - Feet[I].MaxCompression * Scale.Z);
			MaxHeight = FMath::Min(MaxHeight, Height + Feet[I].MaxExtension * Scale.Z);
			MeanHeight += Height;
		}
		if (MinHeight > MaxHeight) { OutResult.Failure = EJTSLandingValidationFailure::GearTravelExceeded; return false; }
		Pose.SetLocation(Ground + Up * FMath::Clamp(MeanHeight / Feet.Num(), MinHeight, MaxHeight));
		FinalMaximumHeight = MaxHeight;
	}
	if (ValidatePose(Planet, Pose, OutResult)) return true;
	if (OutResult.Failure != EJTSLandingValidationFailure::CollisionBlocked) return false;
	// Extend all struts only inside their common physical travel interval to gain belly clearance.
	// Unlike the legacy hull-only lift, each alternative must still support every complete sole.
	const FVector Up = Pose.GetUnitAxis(EAxis::Z);
	const double CurrentHeight = FVector::DotProduct(Pose.GetLocation() - Ground, Up);
	const double AvailableLift = FMath::Max(0.0, FinalMaximumHeight - CurrentHeight);
	for (int32 Attempt = 1; Attempt <= 4 && AvailableLift > 0.5; ++Attempt)
	{
		FTransform Higher = Pose;
		Higher.AddToTranslation(Up * (AvailableLift * Attempt / 4));
		if (ValidatePose(Planet, Higher, OutResult)) return true;
	}
	return false;
}

bool UJTSSpacecraftLandingSupportComponent::FitAtSurface(AJTSPlanetAnchor* Planet, const FVector& Ground,
	const FVector& Forward, FJTSPlanetLandingValidationResult& OutResult) const
{
	OutResult = FJTSPlanetLandingValidationResult();
	if (!IsValid(Planet)) { OutResult.Failure = EJTSLandingValidationFailure::NoPlanet; return false; }
	if (!HasValidRig()) { OutResult.Failure = EJTSLandingValidationFailure::MissingLandingGear; return false; }
	return FitCandidate(Planet, Ground, Forward, OutResult);
}

bool UJTSSpacecraftLandingSupportComponent::HasClearApproach(const FTransform& Pose) const
{
    const auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
    const FVector Start = Ship->GetActorLocation();
	const FVector Up = Pose.GetUnitAxis(EAxis::Z);
	const FVector Error = Pose.GetLocation() - Start;
	const FVector Lateral = FVector::VectorPlaneProject(Error, Up);
    // Check the complete physical hull throughout a concurrent correction/rotation/descent.
    // The final runtime steps are swept and revalidated too; there is no fixed staging hover.
    FVector Previous = Start;
    for (int32 I = 0; I <= 16; ++I)
    {
        const float Alpha = I / 16.0f;
        const FVector Location = Start + Lateral * FMath::Min(1.0f, Alpha * 4)
            + Up * FVector::DotProduct(Error, Up) * Alpha;
        const FQuat Q = FQuat::Slerp(Ship->GetActorQuat(), Pose.GetRotation(), FMath::Min(1.0f, Alpha * 2)).GetNormalized();
        if (!Ship->CanOccupyLandingTransform(FTransform(Q, Location, Ship->GetActorScale3D()))
            || !Ship->CanTraverseLandingSegment(Previous, Location, Q)) return false;
        Previous = Location;
    }
    return true;
}

bool UJTSSpacecraftLandingSupportComponent::FindLanding(AJTSPlanetAnchor* Planet, bool bControlledDescent,
	FJTSPlanetLandingValidationResult& OutResult) const
{
	OutResult = FJTSPlanetLandingValidationResult();
	AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !IsValid(Planet)) { OutResult.Failure = EJTSLandingValidationFailure::NoPlanet; return false; }
	if (!HasValidRig()) { OutResult.Failure = EJTSLandingValidationFailure::MissingLandingGear; return false; }
	float SurfaceAltitude = 0;
	Planet->GetAltitudeAboveSurface(Ship->GetActorLocation(), SurfaceAltitude);
	const float ProbeRange = bControlledDescent ? FMath::Max(6000.0f, SurfaceAltitude + 2000.0f)
		: FMath::Max(6000.0f, Settings.MaxLandingHeight + 2000.0f);
	if (!Ship->RefreshGroundInfo(Planet, ProbeRange))
	{
		OutResult.Failure = EJTSLandingValidationFailure::NoSurface; return false;
	}
	const auto Ground = Ship->GetGroundInfo();
	OutResult.GroundDistance = Ground.Distance;
	// Controlled descent was admitted at the terrain envelope boundary. A mountain's smooth
	// shoulder can be higher than the old fixed capture range; do not introduce a second gate.
	if (!bControlledDescent && Ground.Distance > Settings.MaxLandingHeight) { OutResult.Failure = EJTSLandingValidationFailure::TooHigh; return false; }
	const FVector Up = Planet->GetRadialUpVector(Ground.GroundLocation).GetSafeNormal();
	const FVector Velocity = Ship->GetFlightVelocity();
	const float RadialSpeed = FVector::DotProduct(Velocity, Up);
	if ((!bControlledDescent && Velocity.Size() > Settings.MaxLandingSpeed)
		|| (bControlledDescent && (FVector::VectorPlaneProject(Velocity, Up).Size() > Settings.MaxLandingSpeed
			|| RadialSpeed > Settings.MaxLandingSpeed || -RadialSpeed > Settings.MaxCaptureDescentSpeed)))
	{
		OutResult.Failure = EJTSLandingValidationFailure::TooFast; return false;
	}
	const FVector X = TangentForward(Ship->GetActorForwardVector(), Up), Y = FVector::CrossProduct(Up, X);
	float BestScore = BIG_NUMBER;
	FJTSPlanetLandingValidationResult Best, FirstFailure;
	// Centre first, then two eight-direction rings; three small heading choices. Hard cap: 51 candidates.
	for (int32 Candidate = 0; Candidate < 17; ++Candidate)
	{
		const float Radius = Candidate == 0 ? 0 : Settings.MaxPositionCorrection * (Candidate <= 8 ? 0.5f : 1.0f);
		if (Candidate > 0 && Radius <= KINDA_SMALL_NUMBER) continue;
		const float Angle = ((Candidate - 1) % 8) * PI / 4;
		const FVector Offset = Candidate == 0 ? FVector::ZeroVector : (X * FMath::Cos(Angle) + Y * FMath::Sin(Angle)) * Radius;
		FJTSPlanetSurfaceHit CandidateGround;
		if (!Planet->ProbeSurfaceAlongGravity(Ground.GroundLocation + Offset + Up * 700, 1800, CandidateGround)) continue;
		// Curvature/slope must not extend the pilot's permitted tangent correction.
		const float Correction = FVector::VectorPlaneProject(CandidateGround.ImpactPoint - Ground.GroundLocation, Up).Size();
		if (Correction > Settings.MaxPositionCorrection + 0.5f) continue;
		for (int32 YawChoice = 0; YawChoice < 3; ++YawChoice)
		{
			if (YawChoice > 0 && Settings.MaxYawCorrectionDegrees <= KINDA_SMALL_NUMBER) continue;
			const float Yaw = YawChoice == 0 ? 0 : Settings.MaxYawCorrectionDegrees * (YawChoice == 1 ? -1 : 1);
			const FVector Forward = FQuat(Up, FMath::DegreesToRadians(Yaw)).RotateVector(X);
			FJTSPlanetLandingValidationResult Result;
			if (!FitCandidate(Planet, CandidateGround.ImpactPoint, Forward, Result) || !HasClearApproach(Result.LandingTransform))
			{
				if (Result.bIsValid) { Result.bIsValid = false; Result.Failure = EJTSLandingValidationFailure::ApproachBlocked; }
				if (Candidate == 0 && YawChoice == 0) FirstFailure = Result;
				continue;
			}
			Result.PositionCorrection = Correction;
			Result.GroundDistance = Ground.Distance;
			float Score = Correction + FMath::Abs(Yaw) * 4 + Result.GroundSlopeDegrees * 3;
			for (const auto& Contact : Result.FootContacts) Score += FMath::Abs(Contact.Extension) + Contact.Roughness * 10;
			if (Score < BestScore) { Best = Result; BestScore = Score; }
			// A safe uncorrected footprint wins, keeping assist behaviour predictable for the pilot.
			if (Candidate == 0 && YawChoice == 0) { OutResult = Result; return true; }
		}
	}
	OutResult = Best.bIsValid ? Best : FirstFailure;
	return OutResult.bIsValid;
}

void UJTSSpacecraftLandingSupportComponent::ApplyContactPresentation(float DeployAlpha) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || FootContacts.IsEmpty() || DeployAlpha < 0.9f) return;
	const float Blend = FMath::SmoothStep(0.9f, 1.0f, DeployAlpha);
	const FTransform Pose = Owner->GetActorTransform();
	const FVector Up = Pose.GetUnitAxis(EAxis::Z);
	for (const auto& Contact : FootContacts)
	{
		const FJTSLandingFootDefinition* Definition = Feet.FindByPredicate([&Contact](const auto& Foot)
			{ return Foot.FootComponentName == Contact.FootComponentName; });
		USceneComponent* Foot = FindSceneComponent(Owner, Contact.FootComponentName);
		USceneComponent* Strut = FindSceneComponent(Owner, Contact.StrutComponentName);
		if (!Definition || !Foot || !Strut) continue;
		const float Extension = Contact.Extension * Blend;
		const float Stretch = 1 + Extension / FMath::Max(1.0f, Definition->StrutRestLength * float(Pose.GetScale3D().Z));
		FVector StrutScale = Strut->GetRelativeScale3D();
		StrutScale.Z *= Stretch;
		Strut->SetRelativeScale3D(StrutScale);
		const FTransform Frame = Definition->DeployedContactFrame * Pose;
		const FQuat Nominal = Frame.GetRotation();
		const FQuat Swivel = FQuat::FindBetweenNormals(Frame.GetUnitAxis(EAxis::Z), Contact.Normal);
		const float Angle = FMath::RadiansToDegrees(Swivel.GetAngle());
		const float JointBlend = FMath::Min(1.0f, Settings.MaxFootTiltDegrees / FMath::Max(0.01f, Angle)) * Blend;
		const FQuat Rotation = FQuat::Slerp(FQuat::Identity, Swivel, JointBlend) * Nominal;
		const FVector Sole = Frame.GetLocation() - Up * Extension;
		const FVector Origin = Sole - Rotation.RotateVector(Definition->SoleCentreLocal * Frame.GetScale3D());
		Foot->SetWorldLocationAndRotation(Origin, Rotation);
		// A scaled telescopic parent must not stretch the solid sole as well.
		Foot->SetWorldScale3D(Frame.GetScale3D());
	}
}

bool UJTSSpacecraftLandingSupportComponent::GetLandingPreview(AJTSPlanetAnchor* Planet,
	FJTSPlanetLandingValidationResult& OutResult) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld()) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (CachedPreviewPlanet.Get() != Planet || PreviewTime < 0 || Now - PreviewTime >= 0.25
		|| FVector::DistSquared(CachedPreviewPose.GetLocation(), Owner->GetActorLocation()) > FMath::Square(100.0)
		|| CachedPreviewPose.GetRotation().AngularDistance(Owner->GetActorQuat()) > FMath::DegreesToRadians(3.0))
	{
		FindLanding(Planet, true, CachedPreview);
		CachedPreviewPlanet = Planet;
		CachedPreviewPose = Owner->GetActorTransform();
		PreviewTime = Now;
	}
	OutResult = CachedPreview;
	return OutResult.bIsValid;
}
