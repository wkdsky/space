#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSPlanetSurfaceCutComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSPlanetSurfaceCutTest, "JTS.Planet.Surface.ReversibleCuts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSPlanetSurfaceCutTest::RunTest(const FString&)
{
	struct FScope
	{
		UWorld* World;
		FScope()
		{
			auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); World->InitializeActorsForPlay(FURL()); World->BeginPlay();
		}
		~FScope() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	} Scope;
	const FVector Center(2700000, 370000, 200000);
	const FQuat Orientation = FRotator(25, 45, 15).Quaternion();
	auto* Planet = Scope.World->SpawnActor<AJTSPlanetAnchor>(Center, FRotator::ZeroRotator);
	auto* Surface = Scope.World->SpawnActor<AStaticMeshActor>();
	auto* Component = Surface->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable);
	auto* OriginalAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Component->SetStaticMesh(OriginalAsset); Component->SetCollisionProfileName(TEXT("BlockAll"));
	Surface->SetActorTransform(FTransform(Orientation, Center, FVector(200)));
	FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
	auto* Copy = DuplicateObject<UStaticMesh>(OriginalAsset, GetTransientPackage()); Copy->bAllowCPUAccess = true;
	Planet->SurfaceCuts->CutSourceMesh = Copy;
	const FTransform OriginalPose = Surface->GetActorTransform(); auto* OriginalBody = OriginalAsset->GetBodySetup();
	const FVector Up = Orientation.RotateVector(FVector::XAxisVector), Along = Orientation.RotateVector(FVector::YAxisVector);
	const FVector Origin = Center + Up * 10000;
	const FQuat Frame = FRotationMatrix::MakeFromXZ(Along, Up).ToQuat();
	const FTransform CutPose(Frame, Origin - Up * 50, FVector(5, 1, 2));
	auto* First = Scope.World->SpawnActor<AActor>(); auto* Second = Scope.World->SpawnActor<AActor>();
	Planet->SurfaceCuts->RegisterCut(First, Copy, CutPose);
	if (!TestTrue(TEXT("A closed cut volume subtracts from a private runtime surface"), Planet->SurfaceCuts->RebuildNow())) return false;
	FJTSPlanetSurfaceHit Hit;
	TestTrue(TEXT("Surface queries reach the floor below the original surface"), Planet->ProjectPointToSurface(Origin, Hit));
	TestTrue(TEXT("Cut depth exists in actual collision geometry"), FVector::DotProduct(Origin - Hit.ImpactPoint, Up) > 140);
	TestTrue(TEXT("The cut collision is the runtime mesh"), Hit.HitComponent == Planet->SurfaceCuts->RuntimeSurface);
	FHitResult WorldHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurfaceCutRegression), true);
	TestTrue(TEXT("World physics sees the same recessed floor as planet queries"), Scope.World->LineTraceSingleByChannel(WorldHit, Origin + Up * 500, Origin - Up * 500, ECC_Visibility, Params)
		&& FVector::Dist(WorldHit.ImpactPoint, Hit.ImpactPoint) < 1);
	TestTrue(TEXT("Source asset, collision data and planet transform remain intact"), Component->GetStaticMesh() == OriginalAsset
		&& OriginalAsset->GetBodySetup() == OriginalBody && Surface->GetActorTransform().Equals(OriginalPose));
	FJTSPlanetSurfaceHit Outside;
	TestTrue(TEXT("Uncut terrain retains its exact surface"), Planet->ProjectPointToSurface(Origin + Along * 1500, Outside)
		&& FMath::Abs(FVector::DotProduct(Outside.ImpactPoint - Origin, Up)) < 0.1);

	FTransform OtherPose = CutPose; OtherPose.AddToTranslation(Along * 800);
	Planet->SurfaceCuts->RegisterCut(Second, Copy, OtherPose);
	TestTrue(TEXT("Multiple independent cuts coexist"), Planet->SurfaceCuts->RebuildNow());
	auto* Invalid = DuplicateObject<UStaticMesh>(OriginalAsset, GetTransientPackage()); Invalid->bAllowCPUAccess = false;
	auto* RejectedOwner = Scope.World->SpawnActor<AActor>();
	Planet->SurfaceCuts->RegisterCut(RejectedOwner, Invalid, OtherPose);
	TestFalse(TEXT("An unreadable cutter reports rejection"), Planet->SurfaceCuts->RebuildNow());
	TestTrue(TEXT("A rejected cutter cannot erase successful openings"), Planet->ProjectPointToSurface(Origin, Hit)
		&& FVector::DotProduct(Origin - Hit.ImpactPoint, Up) > 140 && Planet->SurfaceCuts->IsCutApplied(First));
	TestFalse(TEXT("Rejected decoration is not marked usable"), Planet->SurfaceCuts->IsCutApplied(RejectedOwner));
	Planet->SurfaceCuts->RemoveCut(RejectedOwner);
	FTransform MovedPose = CutPose; MovedPose.AddToTranslation(Along * 600);
	Planet->SurfaceCuts->RegisterCut(First, Copy, MovedPose);
	TestTrue(TEXT("A cut can move and overlap another cut"), Planet->SurfaceCuts->RebuildNow());
	TestTrue(TEXT("Moving a cut restores its previous location"), Planet->ProjectPointToSurface(Origin, Hit)
		&& FMath::Abs(FVector::DotProduct(Hit.ImpactPoint - Origin, Up)) < 0.1);
	Planet->SurfaceCuts->RemoveCut(First); TestTrue(TEXT("A single cut can be removed"), Planet->SurfaceCuts->RebuildNow());
	TestTrue(TEXT("Removing one cut preserves an overlapping surviving cut"), Planet->ProjectPointToSurface(Origin + Along * 800, Hit)
		&& FVector::DotProduct(Origin - Hit.ImpactPoint, Up) > 140);
	Planet->SurfaceCuts->RemoveCut(Second); Planet->SurfaceCuts->RebuildNow();
	TestNull(TEXT("Removing the last cut releases the temporary mesh"), Planet->SurfaceCuts->RuntimeSurface.Get());
	TestTrue(TEXT("The original render and collision components are restored"), Component->IsVisible()
		&& Component->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics && Planet->GetGameplaySurfaceComponent() == Component);
	return true;
}
#endif
