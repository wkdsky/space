#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Items/JTSMaterialProcessingRecipes.h"
#include "JTSMaterialProcessorComponent.generated.h"

class AJTSSpacecraftActor;

/** Validates material-stream graphs and commits one atomic shared-storage transaction. No Tick. */
UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSMaterialProcessorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSMaterialProcessorComponent();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material Processor")
	TObjectPtr<UJTSMaterialProcessingRecipes> RecipeBook;

	const FJTSMaterialGraph& GetSavedGraph() const { return SavedGraph; }
	bool ValidateLayout(const FJTSMaterialGraph& Graph, FString& Error) const;
	bool Preview(const FJTSMaterialGraph& Graph, TMap<EJTSResourceType, int64>& OutDeltas, FString& Error) const;
	bool Submit(APawn* Player, const FJTSMaterialGraph& Graph, bool bExecute, FString& Message);
	void RestoreGraph(const FJTSMaterialGraph& Graph);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated) FJTSMaterialGraph SavedGraph;
};
