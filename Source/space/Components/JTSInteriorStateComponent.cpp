#include "space/Components/JTSInteriorStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSInteriorZone.h"
UJTSInteriorStateComponent::UJTSInteriorStateComponent() { PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }
void UJTSInteriorStateComponent::BeginPlay() { Super::BeginPlay(); if (GetOwner()->HasAuthority()) GetWorld()->GetTimerManager().SetTimer(ZoneTimer,this,&ThisClass::UpdateZone,.2f,true); }
void UJTSInteriorStateComponent::UpdateZone() { const auto* C=Cast<AJTSCharacter>(GetOwner()); Zone=C && !C->IsBoarded()?AJTSInteriorZone::FindAt(GetWorld(),C->GetActorLocation()):nullptr; }
void UJTSInteriorStateComponent::EndPlay(const EEndPlayReason::Type R) { if (auto* World=GetWorld()) World->GetTimerManager().ClearTimer(ZoneTimer); Super::EndPlay(R); }
void UJTSInteriorStateComponent::ClientFade_Implementation(bool bOut)
{
    auto* P=Cast<APawn>(GetOwner()); auto* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
    if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(bOut?0:1,bOut?1:0,.25f,FLinearColor::Black,false,bOut);
}
void UJTSInteriorStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSInteriorStateComponent,Zone); }
