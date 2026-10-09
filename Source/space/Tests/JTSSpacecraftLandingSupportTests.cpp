#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Components/JTSSpacecraftLandingSupportComponent.h"
#include "space/Components/JTSSpacecraftSurfaceEnvelopeComponent.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Components/JTSSpacecraftPresentationComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetLandingManager.h"

namespace
{
	struct FLandingTestWorld
	{
		UWorld* World;
		AJTSPlanetAnchor* Planet;
		AJTSSpacecraftActor* Ship;
		UProceduralMeshComponent* Terrain;
		FVector Up = FVector(1, 2, 3).GetSafeNormal();
		FTransform Surface;

		FLandingTestWorld()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->InitializeActorsForPlay(FURL());
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Centre(25000, -17000, 8000);
			Planet = World->SpawnActor<AJTSPlanetAnchor>(AJTSPlanetAnchor::StaticClass(), Centre, FRotator::ZeroRotator, Params);
			FVector X, Y; Up.FindBestAxisVectors(X, Y);
			Surface = FTransform(FRotationMatrix::MakeFromXZ(X, Up).ToQuat(), Centre + Up * 10000);
			AActor* Ground = World->SpawnActor<AActor>();
			Terrain = NewObject<UProceduralMeshComponent>(Ground);
			Ground->AddInstanceComponent(Terrain);
			Ground->SetRootComponent(Terrain);
			Terrain->bUseComplexAsSimpleCollision = true;
			Terrain->SetCollisionProfileName(TEXT("BlockAll"));
			Terrain->RegisterComponent();
			Ground->SetActorTransform(Surface);
			FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Ground);
			FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
			MakeTerrain(0);
			UClass* ShipClass = LoadClass<AJTSSpacecraftActor>(nullptr, TEXT("/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1.BP_Twinfork_R1_C"));
			Ship = World->SpawnActor<AJTSSpacecraftActor>(ShipClass, FTransform(Surface.GetRotation(), Surface.GetLocation() + Up * 900), Params);
			Ship->DispatchBeginPlay();
			Ship->SetFlightTargetPlanet(Planet);
			World->SpawnActor<AJTSPlanetLandingManager>();
		}

		~FLandingTestWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		UJTSSpacecraftLandingSupportComponent* Support() const { return Ship->GetLandingSupportComponent(); }
		// Nonuniform grid allows a fissure smaller than a sole, independently of its centre sample.
		void MakeTerrain(float SlopeDegrees, bool bHole = false, bool bNarrowHole = false, float FrontRightRise = 0)
		{
			const TArray<float> X = {-2500, 180, 200, 240, 270, 290, 300, 340, 2500};
			const TArray<float> Y = {-2500, -500, -450, -420, -400, -360, -300, 300, 330, 430, 500, 2500};
			TArray<FVector> Vertices;
			for (float Py : Y) for (float Px : X)
			{
				const float Rise = Px >= 200 && Px <= 300 && Py >= 330 && Py <= 430 ? FrontRightRise : 0;
				Vertices.Emplace(Px, Py, Px * FMath::Tan(FMath::DegreesToRadians(SlopeDegrees)) + Rise);
			}
			TArray<int32> Triangles;
			for (int32 Iy = 0; Iy < Y.Num()-1; ++Iy) for (int32 Ix = 0; Ix < X.Num()-1; ++Ix)
			{
				const bool InHole = bHole && X[Ix] >= 200 && X[Ix+1] <= 300 && Y[Iy] >= -450 && Y[Iy+1] <= -300;
				const bool InFissure = bNarrowHole && X[Ix] >= 270 && X[Ix+1] <= 290 && Y[Iy] >= -420 && Y[Iy+1] <= -360;
				if (InHole || InFissure) continue;
				const int32 A = Iy * X.Num() + Ix, B = A+1, C = A+X.Num(), D = C+1;
				Triangles.Append({A, B, D, A, D, C});
			}
			Terrain->CreateMeshSection(0, Vertices, Triangles, {}, {}, {}, {}, true);
			Terrain->RecreatePhysicsState();
		}

        void EnterBoundary()
        {
            auto* E=Ship->GetSurfaceEnvelopeComponent();
            E->Reset(); E->Refresh(Planet,Ship->GetActorLocation(),FVector::ZeroVector,900,1);
            Ship->SetActorLocation(E->GetFrame().Location-E->GetFrame().RadialUp*0.5f);
            E->Refresh(Planet,Ship->GetActorLocation(),FVector::ZeroVector,900,1);
            Ship->GetFlightMovementComponent()->StopMovementImmediately();
        }

		void TickLanding(float Delta = 1.0f/60.0f)
		{
			static_cast<AActor*>(Ship)->Tick(Delta);
			Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>()->TickComponent(Delta, LEVELTICK_All, nullptr);
			Ship->GetFlightMovementComponent()->TickComponent(Delta, LEVELTICK_All, nullptr);
		}

		AStaticMeshActor* AddObstacle(const FVector& LocalPosition, const FVector& Scale)
		{
			auto* Actor = World->SpawnActor<AStaticMeshActor>();
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Actor->SetActorTransform(FTransform(Surface.GetRotation(), Surface.TransformPosition(LocalPosition), Scale));
			Actor->GetStaticMeshComponent()->RecreatePhysicsState();
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLandingFootprintRegression, "JTS.Spacecraft.LandingSupport.TerrainAndFeet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSLandingFootprintRegression::RunTest(const FString& Parameters)
{
	FLandingTestWorld F;
	FJTSPlanetLandingValidationResult R;
	TestTrue(TEXT("Real ship has four explicitly configured foot faces"), F.Support()->Feet.Num() == 4);
	TestTrue(TEXT("A clear footprint lands without any LandingSite actor"), F.Support()->FindLanding(F.Planet, false, R));
	TestNull(TEXT("No legacy site grants permission"), R.LandingSite.Get());
	TestEqual(TEXT("All four soles are supported"), R.FootContacts.Num(), 4);
	TestTrue(TEXT("Fit uses the spherical planet's radial frame, far from world Z"), FVector::DotProduct(R.GroundNormal, F.Up) > 0.999);
	const FTransform FlatPose = R.LandingTransform;
	F.MakeTerrain(18);
	TestTrue(TEXT("Continuous 18-degree terrain is within stance tolerance"), F.Support()->FindLanding(F.Planet, false, R));
	TestTrue(TEXT("Hull fits the footprint slope"), FMath::IsNearlyEqual(R.GroundSlopeDegrees, 18.0f, 1.0f));
	F.MakeTerrain(35);
	TestFalse(TEXT("35-degree terrain is refused"), F.Support()->FindLanding(F.Planet, false, R));
	TestEqual(TEXT("Steepness reports its own failure"), R.Failure, EJTSLandingValidationFailure::TooSteep);
	F.MakeTerrain(0);
	F.Support()->Settings.CentreOfMassLocal = FVector(1000, 0, 0);
	TestFalse(TEXT("Four contacts do not legalise a centre of mass outside their polygon"), F.Support()->ValidatePose(F.Planet, FlatPose, R));
	TestEqual(TEXT("Unstable support reports its own failure"), R.Failure, EJTSLandingValidationFailure::UnstableSupport);
	F.Support()->Settings.CentreOfMassLocal = FVector::ZeroVector;
	F.MakeTerrain(0, false, false, 180);
	TestFalse(TEXT("One high platform cannot be hidden by lifting the entire ship"), F.Support()->FitAtSurface(F.Planet, F.Surface.GetLocation(), F.Surface.GetUnitAxis(EAxis::X), R));
	TestEqual(TEXT("Warped stance exceeds independent strut travel"), R.Failure, EJTSLandingValidationFailure::GearTravelExceeded);
	F.MakeTerrain(0);
	auto* HullObstacle = F.AddObstacle(FVector(0, 0, 180), FVector(1, 1, 2));
	TestFalse(TEXT("Clear feet still reject a rock intersecting the hull"), F.Support()->ValidatePose(F.Planet, FlatPose, R));
	TestEqual(TEXT("Hull obstruction is reported"), R.Failure, EJTSLandingValidationFailure::CollisionBlocked);
	HullObstacle->Destroy();
	F.Support()->Feet[0].FootComponentName = TEXT("MissingFoot");
	TestFalse(TEXT("Missing actual foot component cannot land"), F.Support()->FindLanding(F.Planet, false, R));
	TestEqual(TEXT("Missing gear is reported"), R.Failure, EJTSLandingValidationFailure::MissingLandingGear);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLandingCorrectionRegression, "JTS.Spacecraft.LandingSupport.BoundedCorrection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSLandingCorrectionRegression::RunTest(const FString& Parameters)
{
	FLandingTestWorld F;
	FJTSPlanetLandingValidationResult R;
	F.MakeTerrain(0, false, true);
	F.Support()->Settings.MaxPositionCorrection = 0;
	F.Support()->Settings.MaxYawCorrectionDegrees = 0;
	TestFalse(TEXT("A fissure under an edge is refused although the pad centre has ground"), F.Support()->FindLanding(F.Planet, false, R));
	TestEqual(TEXT("Fissure identifies unsupported foot"), R.Failure, EJTSLandingValidationFailure::UnsupportedFoot);
	F.MakeTerrain(0, true);
	TestFalse(TEXT("A hole under one foot is refused at the uncorrected location"), F.Support()->FindLanding(F.Planet, false, R));
	F.Support()->Settings.MaxPositionCorrection = 200;
	TestTrue(TEXT("Local correction finds a completely supported nearby footprint"), F.Support()->FindLanding(F.Planet, false, R));
	TestTrue(TEXT("Correction is useful and bounded by the pilot's two metre radius"), R.PositionCorrection > 1 && R.PositionCorrection <= 200.5f);
	const FTransform CorrectedPose = R.LandingTransform;
	TestTrue(TEXT("The selected exact pose remains independently valid"), F.Support()->ValidatePose(F.Planet, CorrectedPose, R));
	F.Support()->Settings.MaxPositionCorrection = 10;
	TestFalse(TEXT("Assist refuses when safe terrain lies beyond its configured reach"), F.Support()->FindLanding(F.Planet, false, R));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLandingExecutionRegression, "JTS.Spacecraft.LandingSupport.DeployMoveAndRecheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSLandingExecutionRegression::RunTest(const FString& Parameters)
{
	FLandingTestWorld F;
	F.MakeTerrain(0, true);
    TestFalse(TEXT("A high request cannot skip the terrain envelope"),F.Ship->RequestLanding());
    F.EnterBoundary();
	const FVector Start = F.Ship->GetActorLocation();
	TestTrue(TEXT("A server landing request captures the corrected support plan"), F.Ship->RequestLanding());
    F.TickLanding();
    TestTrue(TEXT("First landing frame already descends, with no staging hover"),FVector::DotProduct(F.Ship->GetActorLocation()-Start,F.Up)<-0.1);
    TestTrue(TEXT("Gear starts deploying immediately"),F.Ship->FindComponentByClass<UJTSSpacecraftPresentationComponent>()->GetGearDeployAlpha()>0);
	TestEqual(TEXT("Retracted gear cannot skip alignment/deployment"), F.Ship->GetLandingAssistPhase(), EJTSSpacecraftLandingAssistPhase::Aligning);
	for (int32 I = 0; I < 1200 && !F.Ship->IsLanded(); ++I) F.TickLanding();
	TestTrue(TEXT("Controlled corrected touchdown completes"), F.Ship->IsLanded());
	TestTrue(TEXT("Movement actually follows the sideways correction"), FVector::VectorPlaneProject(F.Ship->GetActorLocation()-Start, F.Up).Size() > 20);
	FJTSPlanetLandingValidationResult R;
	TestTrue(TEXT("Final physical pose supports every sole"), F.Support()->ValidatePose(F.Planet, F.Ship->GetActorTransform(), R));
	TestTrue(TEXT("Gear is fully deployed on touchdown"), F.Ship->IsLandingGearDeployed());
	F.TickLanding();
	for (const auto& Contact : F.Support()->GetFootContacts())
	{
		for (auto* Foot : TInlineComponentArray<UStaticMeshComponent*>(F.Ship))
		{
			if (Foot->GetFName() == Contact.FootComponentName)
			{
				const FVector Sole = Foot->GetComponentTransform().TransformPosition(FVector(0, 0, -18));
				AddInfo(FString::Printf(TEXT("Foot=%s Sole=%s Contact=%s Error=%.3f"), *Foot->GetName(), *Sole.ToString(), *Contact.Location.ToString(), FVector::Distance(Sole, Contact.Location)));
				TestTrue(TEXT("Visible solid foot agrees with the server contact"), FVector::Distance(Sole, Contact.Location) < 1.5);
			}
		}
	}
	// A changing support surface is rechecked before any later state can claim Landed.
	F.Ship->ClearGroundedPlanet();
	F.Ship->SetActorTransform(FTransform(F.Surface.GetRotation(), Start));
	F.MakeTerrain(0);
	F.EnterBoundary();
	TestTrue(TEXT("A second clear-terrain approach begins"), F.Ship->RequestLanding());
	F.Terrain->ClearAllMeshSections();
	for (int32 I = 0; I < 30; ++I) F.TickLanding();
	TestFalse(TEXT("Removed terrain cancels touchdown"), F.Ship->IsLanded());
	TestEqual(TEXT("Removed support returns to flying"), F.Ship->GetFlightState(), EJTSSpacecraftFlightState::Flying);
	TestEqual(TEXT("Contacts are cleared after cancellation"), F.Support()->GetFootContacts().Num(), 0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSEnvelopeBoundaryRegression, "JTS.Spacecraft.SurfaceEnvelope.FlatClearanceAndInvalidLanding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSEnvelopeBoundaryRegression::RunTest(const FString& Parameters)
{
	FLandingTestWorld F;
	auto* E = F.Ship->GetSurfaceEnvelopeComponent();
	auto* Movement = F.Ship->GetFlightMovementComponent();
	// A large-radius planet with an actual planar mesh isolates the promised flat clearance.
	F.Planet->SetActorLocation(F.Surface.GetLocation() - F.Up * 1000000);
	FindFProperty<FFloatProperty>(F.Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(F.Planet, 1000000);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	for (int32 Y = 0; Y <= 32; ++Y)
		for (int32 X = 0; X <= 32; ++X) Vertices.Emplace((X - 16) * 1000, (Y - 16) * 1000, 0);
	for (int32 Y = 0; Y < 32; ++Y)
		for (int32 X = 0; X < 32; ++X)
		{
			const int32 A = Y * 33 + X, B = A + 1, C = A + 33, D = C + 1;
			Triangles.Append({A, B, D, A, D, C});
		}
	F.Terrain->CreateMeshSection(0, Vertices, Triangles, {}, {}, {}, {}, true);
	F.Terrain->RecreatePhysicsState();
	E->Refresh(F.Planet, F.Ship->GetActorLocation(), FVector::ZeroVector, 900, 1);
	const auto Flat = E->GetFrame();
	const float Clearance = FVector::DotProduct(Flat.Location - F.Surface.GetLocation(), F.Up) - Flat.HullClearance;
	AddInfo(FString::Printf(TEXT("Flat envelope extra clearance %.2f cm"), Clearance));
	TestTrue(TEXT("Flat mesh envelope is about one metre above the complete hull"), Clearance >= 100 && Clearance < 140);
	TestTrue(TEXT("Envelope uses planet radial gravity, not world Z"), FVector::DotProduct(Flat.RadialUp, F.Up) > 0.9999);
	TestFalse(TEXT("Landing request above the shell is rejected"), F.Ship->RequestLanding());
	// The centre ray disappears over an actual mesh opening. Neighbouring surface samples
	// must bridge the navigation shell, while no surface below the hull can authorise landing.
	TArray<int32> OpenTriangles;
	for (int32 Y = 0; Y < 32; ++Y)
		for (int32 X = 0; X < 32; ++X)
		{
			if ((X == 15 || X == 16) && (Y == 15 || Y == 16)) continue;
			const int32 A = Y * 33 + X, B = A + 1, C = A + 33, D = C + 1;
			OpenTriangles.Append({A, B, D, A, D, C});
		}
	F.Terrain->CreateMeshSection(0, Vertices, OpenTriangles, {}, {}, {}, {}, true);
	F.Terrain->RecreatePhysicsState();
	F.Ship->SetActorLocation(Flat.Location + F.Up * 30);
	Movement->SetVerticalInput(-1);
	for (int32 I = 0; I < 90; ++I) Movement->TickComponent(1.0f / 60, LEVELTICK_All, nullptr);
	FJTSSurfaceEnvelopeFrame OverHole;
	TestTrue(TEXT("The real neighbours keep an envelope over a missing centre surface"), E->Evaluate(F.Planet, F.Ship->GetActorLocation(), OverHole));
	TestTrue(TEXT("Mesh opening cannot disable the descent floor"), FVector::DotProduct(F.Ship->GetActorLocation() - OverHole.Location, OverHole.RadialUp) >= -1);
	TestEqual(TEXT("Missing centre surface cannot authorise landing"), F.Ship->GetLastLandingFailure(), EJTSLandingValidationFailure::NoSurface);
	// A fissure under one foot has no acceptable correction; repeated Ctrl cannot breach the shell.
	F.MakeTerrain(0, true);
	F.Ship->SetActorTransform(FTransform(F.Surface.GetRotation(), F.Surface.GetLocation() + F.Up * 900));
	Movement->StopMovementImmediately();
	F.Support()->Settings.MaxPositionCorrection = 0;
	F.Support()->Settings.MaxYawCorrectionDegrees = 0;
	E->Reset();
	E->Refresh(F.Planet, F.Ship->GetActorLocation(), FVector::ZeroVector, 900, 1);
	F.Ship->SetActorLocation(E->GetFrame().Location + F.Up * 100);
	Movement->SetVerticalInput(-1);
	float MinimumHeight = BIG_NUMBER;
	for (int32 I = 0; I < 180; ++I)
	{
		Movement->TickComponent(1.0f / 60, LEVELTICK_All, nullptr);
		const auto Frame = E->GetFrame();
		MinimumHeight = FMath::Min(MinimumHeight, float(FVector::DotProduct(F.Ship->GetActorLocation() - Frame.Location, Frame.RadialUp)));
	}
	TestEqual(TEXT("Unsupported foot refuses takeover"), F.Ship->GetLastLandingFailure(), EJTSLandingValidationFailure::UnsupportedFoot);
	TestEqual(TEXT("Invalid terrain leaves the craft flying"), F.Ship->GetFlightState(), EJTSSpacecraftFlightState::Flying);
	TestTrue(TEXT("Held descent never enters the envelope at an invalid landing spot"), MinimumHeight >= -1);
    const auto Boundary=E->GetFrame();
    F.Ship->SetActorLocation(Boundary.Location-Boundary.RadialUp*60);
    for(int32 I=0;I<120;++I) Movement->TickComponent(1.0f/60,LEVELTICK_All,nullptr);
    const auto Recovered=E->GetFrame();
    TestTrue(TEXT("Held illegal descent cannot defeat recovery of a hull displaced below the shell"),
        FVector::DotProduct(F.Ship->GetActorLocation()-Recovered.Location,Recovered.RadialUp)>=-1);

	return true;
}
#endif

