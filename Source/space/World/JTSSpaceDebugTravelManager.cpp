#include "JTSSpaceDebugTravelManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "space/Modes/JTSSpaceWorldGameMode.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSSpaceDebugTravelManager::AJTSSpaceDebugTravelManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

AJTSSpaceDebugTravelManager* AJTSSpaceDebugTravelManager::Find(const UObject* Context)
{
	if (const UWorld* World = Context ? Context->GetWorld() : nullptr)
		for (TActorIterator<AJTSSpaceDebugTravelManager> It(World); It; ++It) return *It;
	return nullptr;
}

bool AJTSSpaceDebugTravelManager::IsAvailable() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bEnabled;
#endif
}

bool AJTSSpaceDebugTravelManager::ContainsDestination(FName PlanetId) const
{
	return !PlanetId.IsNone() && Destinations.ContainsByPredicate([PlanetId](const auto& Entry) { return Entry.PlanetId == PlanetId; });
}

bool AJTSSpaceDebugTravelManager::CanUse(const AJTSPlayerController* Controller) const
{
	const auto* State = Controller ? Controller->GetPlayerState<AJTSPlayerState>() : nullptr;
	return IsAvailable() && State && State->IsExpeditionHost();
}

FName AJTSSpaceDebugTravelManager::GetPreferredPlanetId() const
{
	const auto* Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
	const FName Preferred = Expedition->GetDebugDefaultPlanetId();
	return IsAvailable() && ContainsDestination(Preferred) ? Preferred : NAME_None;
}

void AJTSSpaceDebugTravelManager::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		DefaultPlanetId = GetPreferredPlanetId();
		if (DefaultPlanetId.IsNone())
			if (const auto* Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
				if (const auto* Planet = Manager->GetCurrentPlanet()) DefaultPlanetId = Planet->GetPlanetId();
	}
}

void AJTSSpaceDebugTravelManager::Request(AJTSPlayerController* Controller, FName PlanetId, bool bSetDefault)
{
	if (!HasAuthority() || !CanUse(Controller) || !ContainsDestination(PlanetId)) return;
	auto* Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	auto* GameMode = GetWorld()->GetAuthGameMode<AJTSSpaceWorldGameMode>();
	AJTSPlanetAnchor* Planet = Manager ? Manager->FindPlanetById(PlanetId) : nullptr;
	if (!GameMode || !Planet || !Planet->HasGameplaySurface()) return;
	if (bSetDefault)
	{
		if (GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()->SetDebugDefaultPlanetId(PlanetId))
		{
			DefaultPlanetId = PlanetId;
			Status = NSLOCTEXT("SpaceDebug", "DefaultSaved", "Default saved for next play.");
		}
		else Status = NSLOCTEXT("SpaceDebug", "SaveFailed", "Default could not be saved.");
		ForceNetUpdate();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTravelTime < 1.0) return;
	LastTravelTime = Now;
	const bool bSuccess = GameMode->DebugTravelExpedition(Controller, Planet);
	Status = bSuccess ? NSLOCTEXT("SpaceDebug", "Arrived", "Crew and ship arrived.")
		: NSLOCTEXT("SpaceDebug", "Failed", "No safe arrival. Travel cancelled.");
	ForceNetUpdate();
}

void AJTSSpaceDebugTravelManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSSpaceDebugTravelManager, bEnabled);
	DOREPLIFETIME(AJTSSpaceDebugTravelManager, Destinations);
	DOREPLIFETIME(AJTSSpaceDebugTravelManager, DefaultPlanetId);
	DOREPLIFETIME(AJTSSpaceDebugTravelManager, Status);
}
