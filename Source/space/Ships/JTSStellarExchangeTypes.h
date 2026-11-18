#pragma once
#include "CoreMinimal.h"
#include "JTSStellarExchangeTypes.generated.h"

USTRUCT(BlueprintType)
struct SPACE_API FJTSSharedStellarPrize
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid Token;
	UPROPERTY(BlueprintReadOnly) FName ItemId;
	UPROPERTY() FString RecipientId;
};

/** Durable queue for undelivered paid results; never exposed as a public inventory. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSStellarExchangeSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FJTSSharedStellarPrize> Stock;
	UPROPERTY(BlueprintReadOnly) TArray<FName> DiscoveredCores;
	UPROPERTY(BlueprintReadOnly) FName LastResult;
	UPROPERTY(BlueprintReadOnly) int32 Sequence = 0;
};
