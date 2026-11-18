#include "space/World/JTSMerchant.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "space/Core/JTSGameState.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Player/JTSPlayerController.h"
FText AJTSMerchant::GetInteractionPrompt_Implementation(APawn*) const { return NSLOCTEXT("JTS","TalkMerchant","[E] 与 Jummt 交谈"); }
void AJTSMerchant::Interact_Implementation(APawn* P)
{
    if (!HasAuthority() || !CanInteract_Implementation(P) || GiftStateId.IsNone()) return;
    auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
    if (E->GetSnapshot().MerchantStateIds.Contains(GiftStateId))
    { if (auto* PC=Cast<AJTSPlayerController>(P->GetController())) PC->ClientReadDocument(RepeatDialogue?RepeatDialogue:Document); return; }
    auto* S=GetWorld()->GetGameState<AJTSGameState>(); auto* Ship=S?S->GetActiveSpacecraft():nullptr;
    if (!Ship || !Ship->GetDockComponent()) return;
    auto* Dock=Ship->GetDockComponent();
    if (!Dock->GetInstalledIds().Contains(DockModuleId) && !Dock->InstallDock(DockModuleId)) return;
    E->GetMutableSnapshot().MerchantStateIds.Add(GiftStateId);
    E->LoadCoordinate(GiftCoordinateId); E->RecordImportantItem(GiftImportantItemId); E->SetFacilityFlag(GiftStateId); E->RequestSave();
    Super::Interact_Implementation(P);
}
