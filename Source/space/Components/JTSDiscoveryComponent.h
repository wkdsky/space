#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSDiscoveryTypes.h"
#include "JTSDiscoveryComponent.generated.h"

/** Replicated team knowledge. Host expedition snapshots own the durable copy. */
UCLASS(ClassGroup=(Expedition), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSDiscoveryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSDiscoveryComponent();
	static UJTSDiscoveryComponent* Find(const UObject* Context);
	static bool IsKnown(const UObject* Context, EJTSItemId Id, FName Species=NAME_None);
	static FText DisplayName(const UObject* Context, EJTSItemId Id, FName Species=NAME_None);
	static bool IsKnown(const UObject* Context, const FJTSItemInstance& Item) { return IsKnown(Context,Item.ItemId,Item.SpecimenId); }
	static FText DisplayName(const UObject* Context, const FJTSItemInstance& Item) { return DisplayName(Context,Item.ItemId,Item.SpecimenId); }
	static void StampPickup(const UObject* Context, FJTSItemInstance& Item, float Span, int32 Variant);
	bool Knows(EJTSItemId Id, FName Species=NAME_None) const;
	void SetCatalog(UJTSAnalysisCatalog* Value);
	bool Analyze(const FJTSItemInstance& Item);
	void ObservePickup(const FJTSItemInstance& Item);
	const FJTSAnalyzedTypeRecord* GetRecord(EJTSItemId Id, FName Species=NAME_None) const;
	UJTSAnalysisCatalog* GetCatalog() const { return Catalog; }
	const TArray<FJTSAnalyzedTypeRecord>& GetRecords() const { return Records; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
	virtual void BeginPlay() override;
private:
	void Persist();
	UPROPERTY(Replicated) TObjectPtr<UJTSAnalysisCatalog> Catalog;
	UPROPERTY(Replicated) TArray<FJTSAnalyzedTypeRecord> Records;
};
