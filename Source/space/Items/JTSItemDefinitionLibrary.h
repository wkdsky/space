// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSItemDefinitionLibrary.generated.h"

class UJTSItemDefinition;

/** Central item-definition lookup. Data Assets are preferred; native defaults keep saved prototypes playable if an asset is absent. */
UCLASS()
class SPACE_API UJTSItemDefinitionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Items", meta = (WorldContext = "WorldContextObject"))
	static UJTSItemDefinition* GetItemDefinition(const UObject* WorldContextObject, EJTSItemId ItemId);

	UFUNCTION(BlueprintPure, Category = "Items")
	static FText GetItemDisplayName(EJTSItemId ItemId);

	UFUNCTION(BlueprintPure, Category = "Items")
	static bool TryGetResourceType(EJTSItemId ItemId, EJTSResourceType& OutResourceType);

	UFUNCTION(BlueprintPure, Category = "Items")
	static EJTSItemId GetItemIdForResource(EJTSResourceType ResourceType);

	static FJTSItemInstance MakeInstance(EJTSItemId ItemId, int32 Count = 1);
	static const TArray<EJTSItemId>& GetDefaultShopCatalog();
};
