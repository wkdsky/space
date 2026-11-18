#include "space/Items/JTSReadableDocument.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
AJTSReadableDocument::AJTSReadableDocument()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=false;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    DisplayName=NSLOCTEXT("JTS","DocumentLabel","文件夹");
}
bool AJTSReadableDocument::CanInteract_Implementation(APawn* P) const
{
    auto* C=Cast<AJTSCharacter>(P);
    return C && Document && !C->GetHealthComponent()->IsDead() && FVector::DistSquared(C->GetActorLocation(),GetActorLocation())<FMath::Square(350.f);
}
FText AJTSReadableDocument::GetInteractionPrompt_Implementation(APawn*) const { return NSLOCTEXT("JTS","ReadDocumentAction","[E] 阅读"); }
void AJTSReadableDocument::Interact_Implementation(APawn* P)
{
    if (!HasAuthority() || !CanInteract_Implementation(P)) return;
    if (auto* PC=Cast<AJTSPlayerController>(P->GetController())) PC->ClientReadDocument(Document);
    auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
    if (E && !ReadFlag.IsNone()) E->SetFacilityFlag(ReadFlag);
    if (!Document->DocumentId.IsNone() && !E->GetSnapshot().ReadLogIds.Contains(Document->DocumentId)) { E->GetMutableSnapshot().ReadLogIds.Add(Document->DocumentId); E->RequestSave(); }
}
