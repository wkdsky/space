#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "space/Items/JTSResourceTypes.h"
#include "JTSMaterialProcessingRecipes.generated.h"

/** Fixed point: one tick is half a resource unit. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSMaterialQuantity
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EJTSResourceType ResourceType = EJTSResourceType::Water;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1")) int32 HalfUnits = 2;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSMaterialRecipe
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FJTSMaterialQuantity> Inputs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FJTSMaterialQuantity> Outputs;
};

/** Blueprint-selected programs; the server never accepts recipe quantities from clients. */
UCLASS(BlueprintType)
class SPACE_API UJTSMaterialProcessingRecipes : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJTSMaterialRecipe> Recipes;
	const FJTSMaterialRecipe* FindRecipe(FName Id) const;
};

UENUM(BlueprintType)
enum class EJTSMaterialNodeKind : uint8 { Input, Program, Output };

USTRUCT(BlueprintType)
struct SPACE_API FJTSMaterialNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Id = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EJTSMaterialNodeKind Kind = EJTSMaterialNodeKind::Program;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RecipeId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Batches = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Position = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSMaterialLink
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 From = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 To = 0;
};

/** A material stream: one warehouse input, ordered processing programs, one warehouse output. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSMaterialGraph
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FJTSMaterialNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FJTSMaterialLink> Links;
};

namespace JTSMaterialProcessing
{
	SPACE_API FString ResourceLabel(EJTSResourceType Type);
	SPACE_API FString FormatHalfUnits(int64 HalfUnits);
	SPACE_API FString FormatQuantities(const TArray<FJTSMaterialQuantity>& Quantities, int32 Batches = 1);
}
