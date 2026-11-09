#include "space/Items/JTSExpeditionKeyPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Modes/JTSGameplayGameModeBase.h"
#include "space/Player/JTSCharacter.h"

AJTSExpeditionKeyPickup::AJTSExpeditionKeyPickup()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true; SetReplicateMovement(true);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume")); SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(40);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetGenerateOverlapEvents(false); InteractionVolume->SetCanEverAffectNavigation(false);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardMesh")); Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCanEverAffectNavigation(false);
}
bool AJTSExpeditionKeyPickup::CanInteract_Implementation(APawn* Pawn) const
{
	const auto* Character = Cast<AJTSCharacter>(Pawn);
	const auto* State = GetWorld()->GetGameState<AJTSGameState>();
	return !bCollected && !ImportantItemId.IsNone() && Character && !Character->GetHealthComponent()->IsDead()
		&& FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) < FMath::Square(350.f)
		&& (!State || !State->HasImportantItem(ImportantItemId));
}
FText AJTSExpeditionKeyPickup::GetInteractionPrompt_Implementation(APawn*) const
{
	return NSLOCTEXT("JTS", "ImportantPickupAction", "[E] 拾取");
}
void AJTSExpeditionKeyPickup::Interact_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(Pawn)) return;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ImportantItemPickup), false, Pawn); Query.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query)) return;
	auto* Mode = GetWorld()->GetAuthGameMode<AJTSGameplayGameModeBase>();
	if (Mode && Mode->GrantImportantItem(ImportantItemId, PickupNotice)) { bCollected = true; Destroy(); }
}
