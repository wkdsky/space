#include "space/Components/JTSDiscoveryComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "space/Core/JTSGameState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/World/JTSSpaceWorldManager.h"

UJTSDiscoveryComponent::UJTSDiscoveryComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
UJTSDiscoveryComponent* UJTSDiscoveryComponent::Find(const UObject* C)
{
	const UWorld* W=C?C->GetWorld():nullptr;
	const AJTSGameState* S=W?W->GetGameState<AJTSGameState>():nullptr;
	return S?S->GetDiscoveryComponent():nullptr;
}
void UJTSDiscoveryComponent::BeginPlay()
{
	Super::BeginPlay();
	if(GetOwner()->HasAuthority() && GetWorld()->GetGameInstance())
		if(auto* E=GetWorld()->GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()) Records=E->GetSnapshot().AnalyzedTypes;
}
void UJTSDiscoveryComponent::SetCatalog(UJTSAnalysisCatalog* Value)
{
	if(GetOwner()->HasAuthority() && Value) { Catalog=Value; GetOwner()->ForceNetUpdate(); }
}
const FJTSAnalyzedTypeRecord* UJTSDiscoveryComponent::GetRecord(EJTSItemId Id,FName Species) const
{ return Records.FindByPredicate([Id,Species](const FJTSAnalyzedTypeRecord& R){return R.ItemId==Id && R.SpecimenId==Species;}); }
bool UJTSDiscoveryComponent::Knows(EJTSItemId Id,FName Species) const
{
	if(GetRecord(Id,Species)) return true;
	if(Catalog) if(const auto* E=Catalog->Find(Id,Species)) return E->bInitiallyKnown;
	// Natural resources default to unidentified, even before the configured catalogue replicates.
	EJTSResourceType Resource;
	return !UJTSItemDefinitionLibrary::TryGetResourceType(Id,Resource)
		|| Id==EJTSItemId::Fuel || Id==EJTSItemId::Water || Id==EJTSItemId::Food;
}
bool UJTSDiscoveryComponent::IsKnown(const UObject* C,EJTSItemId Id,FName Species)
{
	if(!AJTSSpaceWorldManager::FindSpaceWorldManager(C)) return true;
	if(auto* D=Find(C)) return D->Knows(Id,Species);
	EJTSResourceType Resource;
	return !UJTSItemDefinitionLibrary::TryGetResourceType(Id,Resource) || Id==EJTSItemId::Fuel || Id==EJTSItemId::Water || Id==EJTSItemId::Food;
}
FText UJTSDiscoveryComponent::DisplayName(const UObject* C,EJTSItemId Id,FName Species)
{
	if(!IsKnown(C,Id,Species)) return FText::FromString(TEXT("???"));
	if(auto* D=Find(C)) if(D->Catalog) if(const auto* E=D->Catalog->Find(Id,Species)) return E->Name;
	return UJTSItemDefinitionLibrary::GetItemDisplayName(Id);
}
void UJTSDiscoveryComponent::StampPickup(const UObject* C,FJTSItemInstance& Item,float Span,int32 Variant)
{
	// Dropped/carried samples retain their true provenance; moving them to another planet is not a discovery.
	if(Item.ObservedPlanetIds.IsEmpty())
	{
		if(const UWorld* W=C?C->GetWorld():nullptr) if(const auto* S=W->GetGameState<AJTSGameState>())
			if(!S->GetCurrentPlanetId().IsEmpty()) Item.ObservedPlanetIds.Add(FName(*S->GetCurrentPlanetId()));
	}
	if(Item.SampleSpanCm<=0) Item.SampleSpanCm=FMath::Clamp(Span,1.f,2000.f);
	Item.SampleVisualVariant=Variant;
}
bool UJTSDiscoveryComponent::Analyze(const FJTSItemInstance& Item)
{
	if(!GetOwner()->HasAuthority() || Item.IsEmpty()) return false;
	FJTSAnalyzedTypeRecord* R=Records.FindByPredicate([&](const FJTSAnalyzedTypeRecord& V){return V.ItemId==Item.ItemId && V.SpecimenId==Item.SpecimenId;});
	bool Changed=!R;
	if(!R) { FJTSAnalyzedTypeRecord New; New.ItemId=Item.ItemId; New.SpecimenId=Item.SpecimenId; R=&Records.Add_GetRef(New); }
	for(FName P:Item.ObservedPlanetIds) if(!P.IsNone() && !R->Sources.Contains(P)) { R->Sources.Add(P); Changed=true; }
	if(Changed) Persist();
	return Changed;
}
void UJTSDiscoveryComponent::ObservePickup(const FJTSItemInstance& Item) { if(Knows(Item.ItemId,Item.SpecimenId)) Analyze(Item); }
void UJTSDiscoveryComponent::Persist()
{
	GetOwner()->ForceNetUpdate();
	if(auto* GI=GetWorld()->GetGameInstance()) if(auto* E=GI->GetSubsystem<UJTSExpeditionSubsystem>())
	{ E->GetMutableSnapshot().AnalyzedTypes=Records; E->RequestSave(); }
}
void UJTSDiscoveryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSDiscoveryComponent,Catalog); DOREPLIFETIME(UJTSDiscoveryComponent,Records);
}
