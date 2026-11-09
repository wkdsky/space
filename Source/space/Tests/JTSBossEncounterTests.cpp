#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSBossEmergenceFeedbackComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSNestBossEncounter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

namespace
{
struct FJTSBossTestWorld
{
	UWorld* World;
	AJTSPlanetAnchor* Planet;
	FVector Origin;
	FVector Forward = FVector::RightVector;
	FVector Up = FVector::ForwardVector;
	uint64 OriginalFrame = GFrameCounter;
	FJTSBossTestWorld()
	{
		const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
		World->SetGameMode(URL); // APawn::ShouldTakeDamage requires an authoritative GameMode.
		World->InitializeActorsForPlay(URL); World->BeginPlay();
		const FVector Center(2700000, 370000, 200000);
		Planet = World->SpawnActor<AJTSPlanetAnchor>(); Planet->SetActorLocation(Center);
		auto* Surface = World->SpawnActor<AStaticMeshActor>();
		Surface->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Surface->SetActorLocation(Center); Surface->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Surface->SetActorScale3D(FVector(200)); Surface->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		FindFProperty<FObjectProperty>(Planet->GetClass(), TEXT("GameplaySurfaceActor"))->SetObjectPropertyValue_InContainer(Planet, Surface);
		FindFProperty<FFloatProperty>(Planet->GetClass(), TEXT("ApproximateRadius"))->SetPropertyValue_InContainer(Planet, 10000);
		Planet->MaximumPlacementSlopeDegrees = 65;
		Origin = Project(Center + Up * 10000);
	}
	~FJTSBossTestWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GFrameCounter = OriginalFrame; }
	FVector Project(FVector Position) const { FJTSPlanetSurfaceHit Hit; return Planet->ProjectPointToSurface(Position, Hit) ? Hit.ImpactPoint : Position; }
	void Advance(float Seconds) { for (int32 I = 0; I < FMath::CeilToInt(Seconds / .04f); ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .04f); } }
	UClass* BossClass() const { return LoadClass<AJTSPlanetBoss>(nullptr, TEXT("/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorBoss.BP_MirrorBoss_C")); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSNestBossTriggerTest, "JTS.Moon.Boss.SevenNestsOneEmergence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSNestBossTriggerTest::RunTest(const FString&)
{
	FJTSBossTestWorld S;
	auto* Encounter = S.World->SpawnActor<AJTSNestBossEncounter>();
	Encounter->SetActorLocation(S.Origin); Encounter->Planet = S.Planet; Encounter->BossClass = S.BossClass(); Encounter->WakeDelay = .1f;
	if (!TestNotNull(TEXT("Configured boss Blueprint"), Encounter->BossClass.Get())) return false;
	for (int32 I = 0; I < 7; ++I)
	{
		auto* Nest = S.World->SpawnActor<AJTSPlanetEcologyHabitat>();
		FindFProperty<FBoolProperty>(Nest->GetClass(), TEXT("bCanBlockEntrance"))->SetPropertyValue_InContainer(Nest, true);
		const FVector Direction = S.Forward.RotateAngleAxis(I * 360.f / 7, S.Up);
		Nest->SetActorLocation(S.Project(S.Origin + Direction * 1800));
		Encounter->Nests.Add(Nest);
	}
	if (!Encounter->HasActorBegunPlay()) Encounter->DispatchBeginPlay();
	Encounter->RebuildRidges();
	TestEqual(TEXT("One independent strip per nest; no ring mesh"), Encounter->Ridges->GetNumSections(), 7);
	TestEqual(TEXT("Ridges cannot displace a walking capsule"), Encounter->Ridges->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	for (int32 I = 0; I < 6; ++I) Encounter->Nests[I]->CommitEntranceBlockade();
	S.Advance(1.2f);
	TestFalse(TEXT("Six of seven never wakes the boss"), Encounter->AreAllNestsDestroyed());
	TestNull(TEXT("No premature spawn"), Encounter->ActiveBoss.Get());
	Encounter->Nests[6]->CommitEntranceBlockade(); S.Advance(.6f);
	if (!TestNotNull(TEXT("All seven wake one boss"), Encounter->ActiveBoss.Get())) return false;
	auto* First = Encounter->ActiveBoss.Get();
	TestEqual(TEXT("Boss starts with its emergence animation"), First->Combat->GetAction(), EJTSBossAction::Emerge);
	FJTSPlanetSurfaceHit Surface; S.Planet->ProjectPointToSurface(First->GetActorLocation(), Surface);
	TestTrue(TEXT("Spawn uses translated real surface, not world Z"), FVector::Dist(First->GetActorLocation(), Surface.ImpactPoint) < 1);
	S.Advance(4); TestEqual(TEXT("Trigger never duplicates boss"), Encounter->ActiveBoss.Get(), First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBossCombatTest, "JTS.Moon.Boss.TelegraphsAttacksHitPartsAndDeath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBossCombatTest::RunTest(const FString&)
{
	FJTSBossTestWorld S;
	const auto Rotation = FRotationMatrix::MakeFromXZ(S.Forward, S.Up).Rotator();
	auto* Boss = S.World->SpawnActor<AJTSPlanetBoss>(S.BossClass(), S.Origin, Rotation);
	if (!TestNotNull(TEXT("Boss asset"), Boss)) return false;
	if (!Boss->HasActorBegunPlay()) Boss->DispatchBeginPlay();
	auto* Pawn = S.World->SpawnActor<AJTSCharacter>();
	Pawn->SetActorTickEnabled(false); Pawn->GetCharacterMovement()->SetComponentTickEnabled(false);
	auto* PC = S.World->SpawnActor<APlayerController>(); PC->Possess(Pawn);
	if (!Pawn->HasActorBegunPlay()) Pawn->DispatchBeginPlay();
	Pawn->GetHealthComponent()->SetMaxHealth(1000, true);
	auto PositionPlayer = [&](float Distance) {
		const FVector Location = S.Project(Boss->GetActorLocation() + Boss->GetActorForwardVector() * Distance);
		Pawn->SetActorLocationAndRotation(Location + S.Planet->GetRadialUpVector(Location) * 90, Rotation, false);
	};
	PositionPlayer(350); Boss->InitializeOnPlanet(S.Planet);
	const float Initial = Boss->Health->GetHealth();
	UGameplayStatics::ApplyDamage(Boss, 20, PC, Pawn, nullptr);
	TestEqual(TEXT("Buried emergence is invulnerable"), Boss->Health->GetHealth(), Initial);
	S.Advance(4.8f);
	if (!TestTrue(TEXT("Close player selects a claw attack"), Boss->Combat->GetAction() == EJTSBossAction::Slam || Boss->Combat->GetAction() == EJTSBossAction::Sweep)) return false;
	const auto FirstAttack = Boss->Combat->GetAction();
	const float PlayerBefore = Pawn->GetHealthComponent()->GetHealth();
	S.Advance(.30f);
	TestEqual(TEXT("Windup cannot deal damage"), Pawn->GetHealthComponent()->GetHealth(), PlayerBefore);
	// Aim at the forward contact arc after the tell, then remain for the whole active window.
	PositionPlayer(300); S.Advance(1.0f);
	const float AfterActive = Pawn->GetHealthComponent()->GetHealth();
	S.Advance(.20f);
	TestTrue(TEXT("Attack hits at most once per player per action"), PlayerBefore - Pawn->GetHealthComponent()->GetHealth() <= Boss->Combat->Attacks[FirstAttack].Damage + .01f);
	TestTrue(TEXT("Active window can damage a close player"), AfterActive < PlayerBefore);
	TSet<EJTSBossAction> Seen; Seen.Add(FirstAttack);
	for (int32 I = 0; I < 220; ++I) { PositionPlayer(300); S.Advance(.04f); Seen.Add(Boss->Combat->GetAction()); }
	TestTrue(TEXT("Close combo includes slam"), Seen.Contains(EJTSBossAction::Slam));
	TestTrue(TEXT("Close combo includes sweep"), Seen.Contains(EJTSBossAction::Sweep));
	// Between sweep reach and slam reach, choose the longer claw action and include capsule contact.
	for (int32 I = 0; I < 100; ++I) { PositionPlayer(480); S.Advance(.04f); if (Boss->Combat->GetAction() == EJTSBossAction::Idle) break; }
	const float BeforeEdge = Pawn->GetHealthComponent()->GetHealth();
	bool bShortSweepAtEdge = false;
	for (int32 I = 0; I < 200; ++I)
	{
		PositionPlayer(480); S.Advance(.04f);
		bShortSweepAtEdge |= Boss->Combat->GetAction() == EJTSBossAction::Sweep;
	}
	TestFalse(TEXT("Never chooses a sweep that cannot reach the player"), bShortSweepAtEdge);
	TestTrue(TEXT("Slam can contact the capsule at its outer reach"), Pawn->GetHealthComponent()->GetHealth() < BeforeEdge);
	for (int32 I = 0; I < 650; ++I) { PositionPlayer(1350); S.Advance(.04f); Seen.Add(Boss->Combat->GetAction()); }
	TestTrue(TEXT("Distant player triggers charge"), Seen.Contains(EJTSBossAction::Charge));
	TestTrue(TEXT("Distant player also triggers leap slam"), Seen.Contains(EJTSBossAction::Leap));
	// Point damage resolves the real bone-following part on the server.
	TArray<UBoxComponent*> Bodies; Boss->GetComponents(Bodies);
	UBoxComponent* Shell = nullptr; UBoxComponent* Head = nullptr;
	for (auto* Box : Bodies) { if (Box->GetAttachSocketName() == TEXT("body")) Shell = Box; if (Box->GetAttachSocketName() == TEXT("head")) Head = Box; }
	TestNotNull(TEXT("Shell damage part"), Shell); TestNotNull(TEXT("Head damage part"), Head);
	if (Shell && Head)
	{
		auto HitPart = [&](UBoxComponent* Part) { FHitResult Hit(Boss, Part, Part->GetComponentLocation(), S.Up); return UGameplayStatics::ApplyPointDamage(Boss, 10, -S.Up, Hit, PC, Pawn, nullptr); };
		const float ShellDamage = HitPart(Shell); const float HeadDamage = HitPart(Head);
		TestTrue(TEXT("Exposed head takes more damage than mirror shell"), HeadDamage > ShellDamage * 1.5f);
	}
	Boss->Health->ApplyDamage(100000, PC, Pawn, true);
	TestEqual(TEXT("Death stops every attack"), Boss->Combat->GetAction(), EJTSBossAction::Death);
	for (auto* Box : Bodies) TestEqual(TEXT("Dead parts cannot intercept weapons or players"), Box->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	const float PlayerAtDeath = Pawn->GetHealthComponent()->GetHealth(); S.Advance(4);
	TestEqual(TEXT("Dead boss never damages again"), Pawn->GetHealthComponent()->GetHealth(), PlayerAtDeath);
	TestFalse(TEXT("Collapsed death pose is held without replay"), Boss->Mesh->GetSingleNodeInstance()->IsPlaying());
	S.Advance(2.8f);
	auto* Fade = Cast<UMaterialInstanceDynamic>(Boss->Mesh->GetMaterial(0));
	if (TestNotNull(TEXT("Corpse switches to configured translucent material"), Fade))
	{
		const float Opacity = Fade->K2_GetScalarParameterValue(TEXT("CorpseOpacity"));
		TestTrue(TEXT("Corpse gradually becomes half transparent"), Opacity > .3f && Opacity < .7f);
	}
	S.Advance(3);
	TestTrue(TEXT("Fully faded corpse is cleaned up by the server"), Boss->IsActorBeingDestroyed());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBossSurfaceTurnTest, "JTS.Moon.Boss.GroundedTurnsAcrossWorldEulerWrap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBossSurfaceTurnTest::RunTest(const FString&)
{
	FJTSBossTestWorld S;
	auto* Boss = S.World->SpawnActor<AJTSPlanetBoss>(S.BossClass(), S.Origin, S.Planet->MakeSurfaceAlignedRotation(S.Origin, S.Forward));
	if (!TestNotNull(TEXT("Boss asset"), Boss)) return false;
	if (!Boss->HasActorBegunPlay()) Boss->DispatchBeginPlay();
	Boss->Combat->Attacks.Reset();
	auto* Pawn = S.World->SpawnActor<AJTSCharacter>();
	Pawn->SetActorTickEnabled(false); Pawn->GetCharacterMovement()->SetComponentTickEnabled(false);
	auto* PC = S.World->SpawnActor<APlayerController>(); PC->Possess(Pawn);
	if (!Pawn->HasActorBegunPlay()) Pawn->DispatchBeginPlay();
	Boss->InitializeOnPlanet(S.Planet); S.Advance(3.1f);
	bool bSawTurn = false; float WorstUp = 1, MaxStep = 0, MaxBurial = 0;
	for (float Angle : {175.f, -175.f, 20.f, 170.f})
	{
		const FVector Target = S.Project(S.Origin + S.Forward.RotateAngleAxis(Angle, S.Up) * 300);
		Pawn->SetActorLocation(Target + S.Planet->GetRadialUpVector(Target) * 90);
		for (int32 I = 0; I < 70; ++I)
		{
			const FQuat Previous = Boss->GetActorQuat(); S.Advance(.04f);
			FJTSPlanetSurfaceFrame Frame; S.Planet->GetSurfaceFrameAt(Boss->GetActorLocation(), Boss->GetActorForwardVector(), Frame);
			WorstUp = FMath::Min(WorstUp, float(FVector::DotProduct(Boss->GetActorUpVector(), Frame.Up)));
			MaxStep = FMath::Max(MaxStep, FMath::RadiansToDegrees(Previous.AngularDistance(Boss->GetActorQuat())));
			MaxBurial = FMath::Max(MaxBurial, float(FVector::Dist(Boss->GetActorLocation(), S.Project(Boss->GetActorLocation()))));
			bSawTurn |= Boss->Combat->GetAction() == EJTSBossAction::TurnLeft || Boss->Combat->GetAction() == EJTSBossAction::TurnRight;
		}
	}
	TestTrue(TEXT("Large turns use a stepping pivot action"), bSawTurn);
	TestTrue(TEXT("Body up stays aligned to real support throughout the turn"), WorstUp > .999f);
	// Timer callbacks can accumulate more than one simulation frame; Update caps its elapsed step at .12 s.
	TestTrue(FString::Printf(TEXT("No instantaneous heading flip (largest step %.3f degrees)"), MaxStep), MaxStep <= Boss->Combat->TurnSpeedDegrees * .12f + .2f);
	TestTrue(TEXT("Root neither buries nor lifts during turning"), MaxBurial < 2);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBossTremorTest, "JTS.Moon.Boss.EmergenceTremorEnvelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBossTremorTest::RunTest(const FString&)
{
	auto* Pattern = NewObject<UJTSGroundTremorPattern>();
	FCameraShakePatternStartParams Start; Start.bOverrideDuration = true; Start.DurationOverride = 1.6f;
	Pattern->StartShakePattern(Start);
	FCameraShakePatternUpdateParams Params; Params.DeltaTime = .005f;
	float PeakPitch = 0, PeakTranslation = 0; int32 Crossings = 0; float Previous = 0;
	for (int32 I = 0; I < 340; ++I)
	{
		FCameraShakePatternUpdateResult Result; Pattern->UpdateShakePattern(Params, Result);
		PeakPitch = FMath::Max(PeakPitch, FMath::Abs(float(Result.Rotation.Pitch)));
		PeakTranslation = FMath::Max(PeakTranslation, float(Result.Location.Size()));
		if (Previous * Result.Rotation.Pitch < 0) ++Crossings;
		Previous = Result.Rotation.Pitch;
	}
	TestTrue(TEXT("Visible rotational tremor"), PeakPitch > .5f && PeakPitch <= .651f);
	TestTrue(TEXT("Ground rumble translates the view by centimetres"), PeakTranslation > 1.5f && PeakTranslation < 2.2f);
	TestTrue(TEXT("Rapid tremor rather than a slow sway"), Crossings > 25);
	TestTrue(TEXT("Shake ends at the remaining emergence duration"), Pattern->IsFinished());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSBossLateTremorTest, "JTS.Moon.Boss.LateInitializationRestartsLocalTremor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FJTSBossLateTremorTest::RunTest(const FString&)
{
	FJTSBossTestWorld S;
	S.Advance(12); // A real encounter spawns well after level BeginPlay.
	auto* Pawn = S.World->SpawnActor<AJTSCharacter>();
	Pawn->SetActorTickEnabled(false); Pawn->GetCharacterMovement()->SetComponentTickEnabled(false);
	auto* PC = S.World->SpawnActor<APlayerController>(); PC->Possess(Pawn);
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	Pawn->SetActorLocation(S.Origin + S.Forward * 400 + S.Up * 96);
	auto* Boss = S.World->SpawnActor<AJTSPlanetBoss>(S.BossClass(), S.Origin, S.Planet->MakeSurfaceAlignedRotation(S.Origin, S.Forward));
	if (!TestNotNull(TEXT("Boss"), Boss) || !TestNotNull(TEXT("Local camera"), PC->PlayerCameraManager.Get())) return false;
	if (!Boss->HasActorBegunPlay()) Boss->DispatchBeginPlay();
	S.Advance(.2f); // BeginPlay's default action clock is stale; initialization follows later.
	Boss->InitializeOnPlanet(S.Planet); S.Advance(.2f);
	auto* Modifier = Cast<UCameraModifier_CameraShake>(PC->PlayerCameraManager->FindCameraModifierByClass(UCameraModifier_CameraShake::StaticClass()));
	if (!TestNotNull(TEXT("Camera shake modifier"), Modifier)) return false;
	auto ActiveCount = [&]() { TArray<FActiveCameraShakeInfo> Active; Modifier->GetActiveCameraShakes(Active); return Active.Num(); };
	TestTrue(TEXT("Nearby local player receives late emergence shake"), ActiveCount() > 0);
	Pawn->SetActorLocation(S.Origin + S.Up * 1200); S.Advance(.6f);
	TestEqual(TEXT("Player high above the surface receives no ground tremor"), ActiveCount(), 0);
	Pawn->SetActorLocation(S.Origin + S.Forward * 400 + S.Up * 96); S.Advance(.2f);
	TestTrue(TEXT("Entering the pit during emergence starts remaining shake"), ActiveCount() > 0);
	Pawn->SetActorLocation(S.Origin + S.Forward * 8000 + S.Up * 96); S.Advance(.6f);
	TestEqual(TEXT("Players outside the pit radius receive no tremor"), ActiveCount(), 0);
	Boss->Combat->Die(); S.Advance(.6f);
	TestEqual(TEXT("Leaving emergence stops its tremor"), ActiveCount(), 0);
	return true;
}
#endif
