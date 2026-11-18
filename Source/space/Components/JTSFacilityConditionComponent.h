#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSFacilityConditionComponent.generated.h"

UENUM(BlueprintType)
enum class EJTSFacilityFact : uint8 { Flag, ImportantItem, BlockedEntrance, DefeatedEncounter, Coordinate };
UENUM(BlueprintType)
enum class EJTSConditionMode : uint8 { All, Any, AtLeast };
USTRUCT(BlueprintType)
struct FJTSFacilityRequirement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EJTSFacilityFact Source = EJTSFacilityFact::Flag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNegate = false;
};
/** Shared condition evaluation. Empty All succeeds; empty Any/AtLeast fails. */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSFacilityConditionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJTSFacilityConditionComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FJTSFacilityRequirement> Requirements;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EJTSConditionMode Mode = EJTSConditionMode::All;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 MinimumSatisfied = 1;
    UFUNCTION(BlueprintPure) bool IsSatisfied() const;
    static bool EvaluateCount(int32 Satisfied, int32 Total, EJTSConditionMode Mode, int32 Minimum);
};
