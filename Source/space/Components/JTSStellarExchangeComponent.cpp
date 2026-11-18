#include "space/Components/JTSStellarExchangeComponent.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Items/JTSStellarLootTable.h"
#include "space/Player/JTSPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/GameInstance.h"
#include "space/Core/JTSGameState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Ships/JTSStellarExchangeDock.h"

UJTSStellarExchangeComponent::UJTSStellarExchangeComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}
double UJTSStellarExchangeComponent::ServerTime() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}
float UJTSStellarExchangeComponent::GetRollElapsed() const { return FMath::Max(0.0, ServerTime() - Roll.StartTime); }
int32 UJTSStellarExchangeComponent::GetReelIndex(int32 Column, int32 Count) const
{
	if (Count <= 0) return 0;
	const float T = FMath::Clamp(GetRollElapsed() / FMath::Max(1.f, RollDuration), 0.f, 1.f);
	// Shared server clock and seed give every observer the same decelerating sequence.
	const int32 Steps = FMath::FloorToInt(52.f * (1.f - FMath::Pow(1.f - T, 3.f)) + Column * 7);
	return (Roll.Seed % Count + Steps) % Count;
}
EJTSStellarRollResult UJTSStellarExchangeComponent::TryPull(APawn* Pawn)
{
	AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !Ship->HasAuthority() || !Ship->GetDockComponent()->CanUseService(EJTSShipDockService::StellarExchange, Pawn))
		return EJTSStellarRollResult::NotAvailable;
	if (Roll.bRolling || ServerTime() < NextPullTime) return EJTSStellarRollResult::CoolingDown;
	const auto* Recipient=Pawn ? Pawn->GetPlayerState<AJTSPlayerState>() : nullptr;
	if (!Recipient) return EJTSStellarRollResult::NotAvailable;
	const UJTSStellarLootTable* Table = Ship->GetStellarLootTable();
	if (!Table || Table->Entries.IsEmpty()) return EJTSStellarRollResult::NotAvailable;
	TMap<EJTSResourceType, int32> Costs;
	for (const auto& C : Table->SpinCosts) if (C.Amount > 0) Costs.FindOrAdd(C.ResourceType) += C.Amount;
	for (const auto& C : Costs) if (!Ship->HasResource(C.Key, C.Value)) return EJTSStellarRollResult::InsufficientResources;
	FName Result;
	if (!Table->Roll(State.DiscoveredCores, Result)) return EJTSStellarRollResult::NotAvailable;
	if (!Costs.IsEmpty() && !Ship->TryConsumeResourceAmounts(Costs)) return EJTSStellarRollResult::InsufficientResources;
	PendingResult = Result; PendingToken=FGuid::NewGuid(); PendingRecipientId=Recipient->GetOnlineIdentityString();
	Roll.bRolling = true; Roll.StartTime = ServerTime(); Roll.Seed = FMath::RandRange(0, MAX_int32 / 2);
	++State.Sequence;
	NextPullTime = Roll.StartTime + FMath::Max(RollDuration, Table->SpinCooldownSeconds);
	GetWorld()->GetTimerManager().SetTimer(RevealTimer, this, &ThisClass::CompleteRoll, RollDuration, false);
	Ship->ForceNetUpdate(); Persist();
	return EJTSStellarRollResult::Succeeded;
}
void UJTSStellarExchangeComponent::CompleteRoll()
{
	if (!GetOwner()->HasAuthority() || PendingResult.IsNone()) return;
	FJTSSharedStellarPrize& Prize = State.Stock.AddDefaulted_GetRef();
	Prize.Token = PendingToken; Prize.ItemId = PendingResult; Prize.RecipientId=PendingRecipientId;
	State.LastResult = PendingResult;
	const auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	const auto* Table = Ship ? Ship->GetStellarLootTable() : nullptr;
	const auto* Entry = Table ? Table->FindEntry(PendingResult) : nullptr;
	if (Entry && Entry->bCore) State.DiscoveredCores.AddUnique(PendingResult);
	PendingResult = NAME_None; PendingToken.Invalidate(); PendingRecipientId.Reset(); Roll.bRolling = false;
	DeliverPendingPrizes();
	GetOwner()->ForceNetUpdate(); Persist();
}
bool UJTSStellarExchangeComponent::TryClaim(APawn*, FGuid) { return false; }

void UJTSStellarExchangeComponent::DeliverPendingPrizes()
{
	if (!GetOwner()->HasAuthority()) return;
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	const auto* Table=Ship ? Ship->GetStellarLootTable() : nullptr;
	if (!Table) return;
	AJTSStellarExchangeDock* Dock=nullptr;
	for (TActorIterator<AJTSStellarExchangeDock> It(GetWorld());It;++It)
		if (It->GetSpacecraft()==Ship) { Dock=*It; break; }
	bool Changed=false;
	for (int32 I=State.Stock.Num()-1;I>=0;--I)
	{
		const auto Prize=State.Stock[I];
		AJTSPlayerState* Recipient=nullptr;
		for (TActorIterator<AJTSPlayerState> It(GetWorld());It;++It)
			if (!It->IsInactive() && It->GetOnlineIdentityString()==Prize.RecipientId) { Recipient=*It; break; }
		// A paid result restored before its owner logs in remains private and durable.
		if (!Prize.RecipientId.IsEmpty() && !Recipient) continue;
		const auto* Entry=Table->FindEntry(Prize.ItemId);
		if (!Entry) continue;
		auto Item=Table->MakeTextItem(Prize.ItemId); Item.InstanceId=Prize.Token;
		bool Delivered=false;
		if (Recipient)
		{
			if (Entry->FirmwareUnits>0)
			{ Recipient->RestoreStellarFirmwareUnits(Recipient->GetStellarFirmwareUnits()+Entry->FirmwareUnits); Delivered=true; }
			else
			{ Recipient->GetStellarLoadout()->ConfigureLootTable(const_cast<UJTSStellarLootTable*>(Table)); Delivered=Recipient->GetStellarLoadout()->TryStoreItem(Item); }
			if (Entry->bCore) Recipient->RecordDiscoveredStellarCore(Prize.ItemId);
		}
		if (!Delivered && Dock) Delivered=Dock->EjectPrize(Item,Recipient ? Recipient->GetPawn() : nullptr);
		if (Delivered) { State.Stock.RemoveAt(I); Changed=true; }
	}
	if (State.Stock.IsEmpty()) GetWorld()->GetTimerManager().ClearTimer(DeliveryTimer);
	else if (!GetWorld()->GetTimerManager().IsTimerActive(DeliveryTimer))
		GetWorld()->GetTimerManager().SetTimer(DeliveryTimer,this,&ThisClass::DeliverPendingPrizes,.5f,true);
	if (Changed) { GetOwner()->ForceNetUpdate(); Persist(); }
}
FJTSStellarExchangeSnapshot UJTSStellarExchangeComponent::CaptureSnapshot() const
{
	auto Result = State;
	// Travel/save while the reels spin still owns the charged prize exactly once.
	if (!PendingResult.IsNone())
	{
		FJTSSharedStellarPrize& P = Result.Stock.AddDefaulted_GetRef();
		P.ItemId = PendingResult; P.Token = PendingToken; P.RecipientId=PendingRecipientId; Result.LastResult = PendingResult;
		const auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
		const auto* Table = Ship ? Ship->GetStellarLootTable() : nullptr;
		const auto* Entry = Table ? Table->FindEntry(PendingResult) : nullptr;
		if (Entry && Entry->bCore) Result.DiscoveredCores.AddUnique(PendingResult);
	}
	return Result;
}
void UJTSStellarExchangeComponent::RestoreSnapshot(const FJTSStellarExchangeSnapshot& Saved)
{
	if (!GetOwner()->HasAuthority()) return;
	GetWorld()->GetTimerManager().ClearTimer(RevealTimer);
	State = Saved; Roll = {}; PendingResult = NAME_None; PendingToken.Invalidate(); PendingRecipientId.Reset(); NextPullTime = 0;
	GetWorld()->GetTimerManager().SetTimer(DeliveryTimer,this,&ThisClass::DeliverPendingPrizes,.5f,true);
	GetOwner()->ForceNetUpdate();
}
void UJTSStellarExchangeComponent::Persist()
{
	auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	auto* Instance = GetWorld()->GetGameInstance();
	if (!Ship || !Ship->HasAuthority() || !Instance) return;
	if (auto* Expedition = Instance->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		// Commit delivery and the recipient's equipment together in one expedition snapshot.
		Expedition->CaptureWorldState(GetWorld()->GetGameState<AJTSGameState>(), Ship);
		Expedition->RequestSave();
	}
}
void UJTSStellarExchangeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(RevealTimer); World->GetTimerManager().ClearTimer(DeliveryTimer); }
	Super::EndPlay(Reason);
}
void UJTSStellarExchangeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSStellarExchangeComponent, State); DOREPLIFETIME(UJTSStellarExchangeComponent, Roll);
}
