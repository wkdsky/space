#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSDiscoveryTypes.generated.h"

class USkeletalMesh;
class UAnimSequence;

USTRUCT(BlueprintType)
struct SPACE_API FJTSAnalysisTag
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Explanation;
};

/** One identity for every size/visual variant of a material or species. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSAnalysisEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EJTSItemId ItemId = EJTSItemId::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpecimenId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJTSAnalysisTag> Tags;
	/** Manufactured supplies may be identified from the beginning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bInitiallyKnown = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMesh> SampleMesh;
	/** Reuse the living creature and its authored death animation; never a separate corpse asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMesh> SpecimenMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> DeathAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpecimenScale = 1;
	/** Large bodies separate along original rigid parts, with smaller groups where needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSeparateCorpseParts = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="25")) float MaxCorpsePartSpan = 80;
};

UCLASS(BlueprintType)
class SPACE_API UJTSAnalysisCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJTSAnalysisEntry> Entries;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FText> PlanetNames;
	const FJTSAnalysisEntry* Find(EJTSItemId Id, FName Species=NAME_None) const { return Entries.FindByPredicate([Id,Species](const FJTSAnalysisEntry& E){ return E.ItemId == Id && E.SpecimenId==Species; }); }
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSAnalyzedTypeRecord
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EJTSItemId ItemId = EJTSItemId::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpecimenId;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Sources;
};

/** Pending ground scan, also used to migrate a sample from an older expedition. */
USTRUCT()
struct SPACE_API FJTSStoredAnalysisSample
{
	GENERATED_BODY()
	UPROPERTY() FJTSItemInstance Item;
	UPROPERTY() float Elapsed = 0;
	UPROPERTY() float Duration = 0;
	UPROPERTY() bool bComplete = false;
};
