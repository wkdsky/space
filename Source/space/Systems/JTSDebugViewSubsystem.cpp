#include "JTSDebugViewSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "ShaderCore.h"
#include "space/World/JTSSpaceDebugTravelManager.h"

void UJTSDebugViewSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void UJTSDebugViewSubsystem::Deinitialize()
{
	RestoreView();
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	Super::Deinitialize();
}

bool UJTSDebugViewSubsystem::CanUseDarkVisibility() const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return false;
#else
	const auto* LocalPlayer = GetLocalPlayer();
	const auto* Debug = LocalPlayer ? AJTSSpaceDebugTravelManager::Find(LocalPlayer) : nullptr;
	return LocalPlayer && Cast<UGameViewportClient>(LocalPlayer->GetViewportClient())
		&& Debug && Debug->IsAvailable() && AllowDebugViewmodes();
#endif
}

bool UJTSDebugViewSubsystem::IsDarkVisibilityEnabled() const
{
	return bHasSavedView && ModifiedViewport.IsValid() && ModifiedViewport->ViewModeIndex == VMI_Unlit;
}

bool UJTSDebugViewSubsystem::SetDarkVisibilityEnabled(bool bEnabled)
{
	if (!bEnabled)
	{
		RestoreView();
		return true;
	}
	if (!CanUseDarkVisibility()) return false;
	if (IsDarkVisibilityEnabled()) return true;
	RestoreView();
	auto* Viewport = Cast<UGameViewportClient>(GetLocalPlayer()->GetViewportClient());
	SavedShowFlags = Viewport->EngineShowFlags;
	SavedViewMode = Viewport->ViewModeIndex;
	ModifiedViewport = Viewport;
	ModifiedWorld = GetLocalPlayer()->GetWorld();
	bHasSavedView = true;
	Viewport->SetViewMode(VMI_Unlit);
	if (Viewport->ViewModeIndex != VMI_Unlit)
	{
		RestoreView();
		return false;
	}
	return true;
}

void UJTSDebugViewSubsystem::RestoreView()
{
	if (bHasSavedView)
	{
		if (auto* Viewport = ModifiedViewport.Get())
		{
			Viewport->ViewModeIndex = SavedViewMode;
			Viewport->EngineShowFlags = SavedShowFlags;
		}
	}
	bHasSavedView = false;
	ModifiedViewport.Reset();
	ModifiedWorld.Reset();
}

void UJTSDebugViewSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (World == ModifiedWorld.Get()) RestoreView();
}
