#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"

namespace
{
	struct FForceWorld
	{
		UWorld* World;
		APawn* Source;
		AActor* Field;
		UJTSStellarTargetComponent* Status;
		FForceWorld()
		{
			const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine)); World->InitializeActorsForPlay(FURL());
			Source = World->SpawnActor<APawn>(); Field = World->SpawnActor<AActor>();
			auto* Actor = World->SpawnActor<AActor>();
			auto* Root = NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Root); Actor->AddInstanceComponent(Root); Root->RegisterComponent();
			auto* Health = NewObject<UJTSHealthComponent>(Actor); Actor->AddInstanceComponent(Health); Health->RegisterComponent(); Health->SetMaxHealth(1000);
			Status = NewObject<UJTSStellarTargetComponent>(Actor); Actor->AddInstanceComponent(Status); Status->RegisterComponent();
		}
		~FForceWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFieldSumTest,"JTS.Stellar.BlackHole.AdditivePhysicalForces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSFieldSumTest::RunTest(const FString&)
{
	FForceWorld W;
	const FVector Position(600,0,45), Up = FVector::UpVector;
	W.Status->ApplyAttraction(W.Source,W.Field,FVector(0,0,73),1650,900,65,.5f);
	const FVector Attraction = W.Status->GetFieldAcceleration(Position,Up);
	W.Status->RemoveFieldForce(W.Field);
	W.Status->ApplyRepulsion(W.Source,180,650,.5f);
	const FVector Repulsion = W.Status->GetFieldAcceleration(Position,Up);
	W.Status->ApplyAttraction(W.Source,W.Field,FVector(0,0,73),1650,900,65,.5f);
	TestTrue(TEXT("Both forces contribute to the vector sum"),W.Status->GetFieldAcceleration(Position,Up).Equals(Attraction+Repulsion,.01f));
	TestTrue(TEXT("Repulsion is strong inside the exclusion radius"),Repulsion.X>8000);
	TestTrue(TEXT("Finite field radii exclude distant enemies"),W.Status->GetFieldAcceleration(FVector(900,0,45),Up).IsNearlyZero());
	TestTrue(TEXT("Coincident enemies are expelled instead of remaining stuck inside the player"),!W.Status->GetFieldAcceleration(FVector(0,0,45),Up).IsNearlyZero());
	W.Status->RemoveForce(W.Source);
	TestTrue(TEXT("Releasing repulsion leaves the black hole's attraction"),W.Status->GetFieldAcceleration(Position,Up).Equals(Attraction,.01f));
	TestTrue(TEXT("Control stays tangent to spherical gravity"),FMath::IsNearlyZero(W.Status->GetFieldAcceleration(FVector(200,50,100),FVector::RightVector).Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSFieldEquilibriumTest,"JTS.Stellar.BlackHole.RepulsionBoundaryEquilibrium",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSFieldEquilibriumTest::RunTest(const FString&)
{
	FForceWorld W;
	W.Status->ApplyAttraction(W.Source,W.Field,FVector(0,0,73),1650,900,65,.5f);
	W.Status->ApplyRepulsion(W.Source,180,650,.5f);
	FVector Position(640,0,45), Velocity(-500,0,0);
	float Impact = 0, MinimumDistance = Position.X;
	for(int32 Step=0;Step<200;++Step)
	{
		Position += W.Status->IntegrateFieldMotion(Position,FVector::UpVector,FVector(-180,0,0),520,FVector::ZeroVector,.05f,Velocity,Impact);
		MinimumDistance=FMath::Min(MinimumDistance,static_cast<float>(Position.X));
		if(Step==3) TestTrue(TEXT("A fast approaching enemy is physically bounced outward after shallow entry"),Velocity.X>0);
	}
	AddInfo(FString::Printf(TEXT("Net-force equilibrium: %.2f cm from player, speed %.2f cm/s, minimum %.2f cm"),Position.X,Velocity.Size(),MinimumDistance));
	TestTrue(TEXT("Attraction and repulsion settle at the outer boundary"),Position.X>620 && Position.X<670);
	TestTrue(TEXT("Enemy cannot enter the player's melee reach while held"),MinimumDistance>500);
	TestTrue(TEXT("Equilibrium has damped motion rather than teleport locking"),Velocity.Size()<5);
	W.Status->RemoveForce(W.Source);
	Position += W.Status->IntegrateFieldMotion(Position,FVector::UpVector,FVector(-180,0,0),520,FVector::ZeroVector,.2f,Velocity,Impact);
	TestTrue(TEXT("Releasing the barrier allows attraction to resume inward acceleration"),Velocity.X<-50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSRepulsionEntryTest,"JTS.Stellar.BlackHole.ShallowEntryAndBoundedPush",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSRepulsionEntryTest::RunTest(const FString&)
{
	FForceWorld W;
	W.Status->ApplyRepulsion(W.Source,180,650,.5f);
	TestTrue(TEXT("An approaching enemy may enter the outer 30 cm without being repelled"),
		W.Status->GetFieldAcceleration(FVector(630,0,45),FVector::UpVector,FVector(-180,0,0)).IsNearlyZero());
	TestTrue(TEXT("Crossing the shallow entry threshold produces a hard outward acceleration"),
		W.Status->GetFieldAcceleration(FVector(618,0,45),FVector::UpVector,FVector(-180,0,0)).X>10000);
	W.Status->ApplyRepulsion(W.Source,180,650,.5f);
	TestTrue(TEXT("A refreshed field retains its engaged push while exiting"),
		W.Status->GetFieldAcceleration(FVector(648,0,45),FVector::UpVector).X>0);
	for (float Start : {0.0f,100.0f,618.0f})
	{
		W.Status->RemoveForce(W.Source); W.Status->ApplyRepulsion(W.Source,180,650,.5f);
		FVector Position(Start,0,45), Velocity=FVector::ZeroVector;
		float Maximum=Start, Impact=0;
		for(int32 Step=0;Step<120;++Step)
		{
			Position += W.Status->IntegrateFieldMotion(Position,FVector::UpVector,FVector::ZeroVector,0,FVector::ZeroVector,.05f,Velocity,Impact);
			const float Distance=FVector(Position.X,Position.Y,0).Size();
			Maximum=FMath::Max(Maximum,Distance);
			if(Step==0) TestTrue(TEXT("Enemies born anywhere inside the range move outward immediately"),Velocity.Size()>50);
		}
		const float Distance=FVector(Position.X,Position.Y,0).Size();
		AddInfo(FString::Printf(TEXT("Spawn %.0f cm: final %.2f cm, maximum %.2f cm, speed %.2f"),Start,Distance,Maximum,Velocity.Size()));
		TestTrue(TEXT("A deep spawn is expelled just beyond the range rather than launched far away"),Distance>650 && Distance<675 && Maximum<680);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNonSolidCoreTest,"JTS.Stellar.BlackHole.NonSolidCoreAndStableSubsteps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FJTSNonSolidCoreTest::RunTest(const FString&)
{
	FForceWorld W;
	W.Status->ApplyAttraction(W.Source,W.Field,FVector::ZeroVector,1650,900,65,.5f);
	const FVector Start(150,0,45);
	FVector FastVelocity(-900,0,0), SlowVelocity = FastVelocity;
	float Impact = 0;
	const FVector Fast = Start + W.Status->IntegrateFieldMotion(Start,FVector::UpVector,FVector::ZeroVector,0,FVector::ZeroVector,.1f,FastVelocity,Impact);
	TestTrue(TEXT("The attraction point does not bounce an enemy away from the area below the core"),FastVelocity.X<0);
	TestTrue(TEXT("An attracted enemy may enter the visual core footprint freely"),Fast.X<65);
	FVector Slow = Start;
	for(int32 I=0;I<2;++I) Slow += W.Status->IntegrateFieldMotion(Slow,FVector::UpVector,FVector::ZeroVector,0,FVector::ZeroVector,.05f,SlowVelocity,Impact);
	TestTrue(TEXT("Different movement frame sizes resolve the same substep trajectory"),Fast.Equals(Slow,2) && FastVelocity.Equals(SlowVelocity,20));
	return true;
}
#endif
