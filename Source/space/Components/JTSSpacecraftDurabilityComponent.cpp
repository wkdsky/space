#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
UJTSSpacecraftDurabilityComponent::UJTSSpacecraftDurabilityComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
void UJTSSpacecraftDurabilityComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        Hull=MaximumHull;
        GetOwner()->OnTakeAnyDamage.AddDynamic(this,&ThisClass::OnDamage);
    }
}
void UJTSSpacecraftDurabilityComponent::OnDamage(AActor*,float Damage,const UDamageType*,AController*,AActor*)
{
    if (!GetOwner()->HasAuthority() || bDisabled || !FMath::IsFinite(Damage) || Damage<=0) return;
    const double Now=GetWorld()->GetTimeSeconds();
    RecentDamageRate=Now-LastDamageTime>3?Damage: FMath::Lerp(RecentDamageRate,Damage/FMath::Max(.2,Now-LastDamageTime),.4f);
    Hull=FMath::Max(0.f,Hull-Damage); bDisabled=Hull<=0;
    EstimatedSurvivalSeconds=Hull/FMath::Max(1.f,RecentDamageRate); LastDamageTime=Now;
    GetOwner()->ForceNetUpdate();
    if (bDisabled) GetWorld()->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()->RequestSave();
}
void UJTSSpacecraftDurabilityComponent::RestoreHull(float Value) { if (GetOwner()->HasAuthority()) { Hull=FMath::Clamp(Value,0.f,MaximumHull); bDisabled=Hull<=0; } }
bool UJTSSpacecraftDurabilityComponent::Reconstruct(APawn* Pawn)
{
    auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner()); auto* C=Cast<AJTSCharacter>(Pawn);
    if (!Ship || !Ship->HasAuthority() || !bDisabled || !C || C->GetHealthComponent()->IsDead()
        || FVector::DistSquared(C->GetActorLocation(),Ship->GetActorLocation())>FMath::Square(1800.f)) return false;
    TArray<UJTSInventoryComponent*> Nearby;
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It)
        if (!It->GetHealthComponent()->IsDead() && FVector::DistSquared(It->GetActorLocation(),Ship->GetActorLocation())<=FMath::Square(1800.f)) Nearby.Add(It->GetInventoryComponent());
    TMap<EJTSResourceType,int32> ShipCosts;
    for (const auto& Cost:ReconstructionCosts)
    {
        if (Cost.Value<0) return false;
        int64 Available=Ship->GetResourceAmount(Cost.Key);
        for (const auto* I:Nearby) Available+=I->GetResourceAmount(Cost.Key);
        if (Available<Cost.Value) return false;
        ShipCosts.Add(Cost.Key,FMath::Min(Cost.Value,Ship->GetResourceAmount(Cost.Key)));
    }
    if (!Ship->TryConsumeResourceAmounts(ShipCosts)) return false;
    for (const auto& Cost:ReconstructionCosts)
    {
        int32 Remaining=Cost.Value-ShipCosts.FindRef(Cost.Key);
        for (auto* I:Nearby) { const int32 Take=FMath::Min(Remaining,I->GetResourceAmount(Cost.Key)); if (Take) I->TryRemoveItem(UJTSItemDefinitionLibrary::GetItemIdForResource(Cost.Key),Take); Remaining-=Take; }
    }
    RestoreHull(MaximumHull); EstimatedSurvivalSeconds=0; Ship->ForceNetUpdate();
    GetWorld()->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()->RequestSave(); return true;
}
void UJTSSpacecraftDurabilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSSpacecraftDurabilityComponent,Hull); DOREPLIFETIME(UJTSSpacecraftDurabilityComponent,bDisabled); DOREPLIFETIME(UJTSSpacecraftDurabilityComponent,EstimatedSurvivalSeconds); }
