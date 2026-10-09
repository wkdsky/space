#pragma once

#include "CoreMinimal.h"
#include "JTSMoonResourceDistribution.generated.h"

/** Content availability belongs to the PDA; abundance belongs to each placed spawner. */
UENUM(BlueprintType)
enum class EJTSMoonResourceKind : uint8
{
	SmallRock,
	MediumRock,
	LargeRock,
	MediumMetalRock,
	LargeMetalRock,
	MediumCrystalRock,
	LargeCrystalRock,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSMoonResourceKinds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks (Small)"))
	bool bSmallRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks (Medium)"))
	bool bMediumRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks (Large)"))
	bool bLargeRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks + Metal (Medium)"))
	bool bMediumMetalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks + Metal (Large)"))
	bool bLargeMetalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks + Crystal (Medium)"))
	bool bMediumCrystalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resources", meta = (DisplayName = "Rocks + Crystal (Large)"))
	bool bLargeCrystalRock = true;

	bool IsEnabled(EJTSMoonResourceKind Kind) const;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSMoonResourceDistribution
{
	GENERATED_BODY()

	// Availability mirrors the referenced PDA for native EditConditionHides evaluation.
	FJTSMoonResourceKinds EnabledKinds;
	void SetEnabledKinds(const FJTSMoonResourceKinds& Kinds);

	UPROPERTY(Transient) bool bEnableSmallRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks (Small) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableSmallRock", EditConditionHides, HideEditConditionToggle))
	float SmallRock = 30;
	UPROPERTY(Transient) bool bEnableMediumRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks (Medium) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableMediumRock", EditConditionHides, HideEditConditionToggle))
	float MediumRock = 15;
	UPROPERTY(Transient) bool bEnableLargeRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks (Large) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableLargeRock", EditConditionHides, HideEditConditionToggle))
	float LargeRock = 15;
	UPROPERTY(Transient) bool bEnableMediumMetalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks + Metal (Medium) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableMediumMetalRock", EditConditionHides, HideEditConditionToggle))
	float MediumMetalRock = 15;
	UPROPERTY(Transient) bool bEnableLargeMetalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks + Metal (Large) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableLargeMetalRock", EditConditionHides, HideEditConditionToggle))
	float LargeMetalRock = 10;
	UPROPERTY(Transient) bool bEnableMediumCrystalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks + Crystal (Medium) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableMediumCrystalRock", EditConditionHides, HideEditConditionToggle))
	float MediumCrystalRock = 10;
	UPROPERTY(Transient) bool bEnableLargeCrystalRock = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ratios", meta = (DisplayName = "Rocks + Crystal (Large) %", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "bEnableLargeCrystalRock", EditConditionHides, HideEditConditionToggle))
	float LargeCrystalRock = 5;

	/** Keep the edited percentage; redistribute its remainder proportionally across other enabled kinds. */
	void Normalize(FName EditedProperty = NAME_None);
	float GetPercentage(EJTSMoonResourceKind Kind) const;
	float GetTotalPercentage() const;
};
