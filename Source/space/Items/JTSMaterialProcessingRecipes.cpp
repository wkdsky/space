#include "space/Items/JTSMaterialProcessingRecipes.h"

const FJTSMaterialRecipe* UJTSMaterialProcessingRecipes::FindRecipe(FName Id) const
{
	return Recipes.FindByPredicate([Id](const FJTSMaterialRecipe& Recipe) { return Recipe.Id == Id; });
}

FString JTSMaterialProcessing::ResourceLabel(EJTSResourceType Type)
{
	switch (Type)
	{
	case EJTSResourceType::Fuel: return TEXT("燃料");
	case EJTSResourceType::Water: return TEXT("水");
	case EJTSResourceType::Food: return TEXT("食物");
	case EJTSResourceType::Rock: return TEXT("石头");
	case EJTSResourceType::Ore: return TEXT("矿物");
	case EJTSResourceType::Organic: return TEXT("有机质");
	case EJTSResourceType::MoonAntCorpse: return TEXT("生物尸体");
	case EJTSResourceType::Crystal: return TEXT("晶体");
	case EJTSResourceType::InorganicCarbon: return TEXT("无机碳");
	default: return TEXT("材料");
	}
}

FString JTSMaterialProcessing::FormatHalfUnits(int64 HalfUnits)
{
	const FString Sign = HalfUnits < 0 ? TEXT("-") : TEXT("");
	const int64 Value = FMath::Abs(HalfUnits);
	return Sign + LexToString(Value / 2) + (Value % 2 ? TEXT(".5") : TEXT(""));
}

FString JTSMaterialProcessing::FormatQuantities(const TArray<FJTSMaterialQuantity>& Quantities, int32 Batches)
{
	TArray<FString> Parts;
	for (const FJTSMaterialQuantity& Quantity : Quantities)
		Parts.Add(FormatHalfUnits(int64(Quantity.HalfUnits) * Batches) + TEXT(" ") + ResourceLabel(Quantity.ResourceType));
	return FString::Join(Parts, TEXT(" + "));
}
