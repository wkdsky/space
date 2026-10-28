#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "space/Core/JTSGameState.h"
#include "space/Modes/JTSGameplayGameModeBase.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLobbyAdmissionTest, "JTS.Lobby.Admission.CloseAndReopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSLobbyAdmissionTest::RunTest(const FString& Parameters)
{
	const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	AJTSGameplayGameModeBase* Mode=World->SpawnActor<AJTSGameplayGameModeBase>();
	AJTSGameState* State=World->SpawnActor<AJTSGameState>();
	FindFProperty<FObjectProperty>(AGameModeBase::StaticClass(),TEXT("GameState"))->SetObjectPropertyValue_InContainer(Mode,State);
	World->SetGameState(State);
	AJTSPlayerController* Host=World->SpawnActor<AJTSPlayerController>();
	Host->PlayerState=World->SpawnActor<AJTSPlayerState>();
	CastChecked<AJTSPlayerState>(Host->PlayerState)->SetExpeditionHost(true);
	AJTSPlayerController* Guest=World->SpawnActor<AJTSPlayerController>();
	Guest->PlayerState=World->SpawnActor<AJTSPlayerState>();
	State->SetGameplayPhase(EJTSGameplayPhase::WaitingToStart);
	State->SetAcceptingNewPlayers(true);

	for (int32 Cycle=0;Cycle<2;++Cycle)
	{
		TestFalse(TEXT("Guest cannot close joining"),Mode->RequestSetJoiningOpen(Guest,false));
		TestTrue(TEXT("Rejected request leaves admission open"),State->IsAcceptingNewPlayers());
		TestTrue(TEXT("Host can close joining"),Mode->RequestSetJoiningOpen(Host,false));
		TestFalse(TEXT("Admission closes"),State->IsAcceptingNewPlayers());
		TestFalse(TEXT("Guest cannot reopen joining"),Mode->RequestSetJoiningOpen(Guest,true));
		TestFalse(TEXT("Rejected request leaves admission closed"),State->IsAcceptingNewPlayers());
		TestTrue(TEXT("Host can reopen joining"),Mode->RequestSetJoiningOpen(Host,true));
		TestTrue(TEXT("Admission reopens"),State->IsAcceptingNewPlayers());
	}
	TestTrue(TEXT("Legacy close action still closes"),Mode->RequestCloseJoining(Host));
	State->SetLaunchDeadline(3.0);
	TestFalse(TEXT("Countdown prevents reopening"),Mode->RequestSetJoiningOpen(Host,true));
	TestFalse(TEXT("Countdown leaves admission closed"),State->IsAcceptingNewPlayers());
	State->SetLaunchDeadline(0.0);
	State->SetGameplayPhase(EJTSGameplayPhase::EarthCollection);
	TestFalse(TEXT("Active expedition prevents reopening"),Mode->RequestSetJoiningOpen(Host,true));
	TestFalse(TEXT("Missing controller is rejected"),Mode->RequestSetJoiningOpen(nullptr,true));

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJTSLobbyDisplayNameTest, "JTS.Lobby.DisplayName.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJTSLobbyDisplayNameTest::RunTest(const FString& Parameters)
{
	FString Name;
	TestTrue(TEXT("Chinese display name is accepted"),AJTSPlayerState::NormalizeDisplayName(TEXT("宇航员小李"),Name));
	TestTrue(TEXT("Outer spaces are trimmed"),AJTSPlayerState::NormalizeDisplayName(TEXT("  Explorer  "),Name));
	TestEqual(TEXT("Normalized name"),Name,FString(TEXT("Explorer")));
	TestFalse(TEXT("Blank name is rejected"),AJTSPlayerState::NormalizeDisplayName(TEXT("   "),Name));
	TestFalse(TEXT("Long name is rejected"),AJTSPlayerState::NormalizeDisplayName(FString::ChrN(21,TEXT('A')),Name));
	TestFalse(TEXT("Embedded newline is rejected"),AJTSPlayerState::NormalizeDisplayName(TEXT("A\nB"),Name));
	TestFalse(TEXT("Trailing control characters are rejected"),AJTSPlayerState::NormalizeDisplayName(TEXT("A\n"),Name));
	return true;
}
#endif
