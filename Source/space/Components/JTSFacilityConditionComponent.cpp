#include "space/Components/JTSFacilityConditionComponent.h"
#include "Engine/World.h"
#include "space/Core/JTSGameState.h"
UJTSFacilityConditionComponent::UJTSFacilityConditionComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool UJTSFacilityConditionComponent::EvaluateCount(int32 Count, int32 Total, EJTSConditionMode InMode, int32 Minimum)
{
    if (InMode==EJTSConditionMode::All) return Count==Total;
    if (InMode==EJTSConditionMode::Any) return Count>0;
    return Total>0 && Count>=FMath::Max(1,Minimum);
}
bool UJTSFacilityConditionComponent::IsSatisfied() const
{
    if (Requirements.IsEmpty()) return EvaluateCount(0,0,Mode,MinimumSatisfied);
    const auto* State=GetWorld()?GetWorld()->GetGameState<AJTSGameState>():nullptr;
    if (!State) return false;
    int32 Count=0;
    TSet<FString> Seen;
    for (const auto& R:Requirements)
    {
        const FString Key=FString::Printf(TEXT("%d:%s"),int32(R.Source),*R.Id.ToString());
        if (R.Id.IsNone() || Seen.Contains(Key)) return false;
        Seen.Add(Key);
        const int32 PreviousCount=Count;
        switch(R.Source)
        {
        case EJTSFacilityFact::Flag: Count+=State->FacilityFlagIds.Contains(R.Id); break;
        case EJTSFacilityFact::ImportantItem: Count+=State->HasImportantItem(R.Id); break;
        case EJTSFacilityFact::BlockedEntrance: Count+=State->BlockedEntranceIds.Contains(R.Id); break;
        case EJTSFacilityFact::DefeatedEncounter: Count+=State->DefeatedEncounterIds.Contains(R.Id); break;
        case EJTSFacilityFact::Coordinate: Count+=State->LoadedCoordinateIds.Contains(R.Id); break;
        }
        if (R.bNegate) Count=PreviousCount+(Count==PreviousCount ? 1 : 0);
    }
    return EvaluateCount(Count,Requirements.Num(),Mode,MinimumSatisfied);
}
