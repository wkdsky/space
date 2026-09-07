#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "JTSMeleeTarget.generated.h"

class APawn;

/** The small set of melee actions supported by the Moon prototype. */
UENUM(BlueprintType)
enum class EJTSMeleeAttackType : uint8
{
	Punch UMETA(DisplayName = "Punch"),
	Knife UMETA(DisplayName = "Knife"),
	Axe UMETA(DisplayName = "Axe")
};

/** Contract for lightweight Moon targets that can be aimed at and struck by the shared melee component. */
UINTERFACE(BlueprintType)
class SPACE_API UJTSMeleeTarget : public UInterface
{
	GENERATED_BODY()
};

class SPACE_API IJTSMeleeTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee")
	bool CanReceiveMeleeHit(APawn* AttackingPawn) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee")
	void ReceiveMeleeHit(APawn* AttackingPawn, EJTSMeleeAttackType AttackType);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee")
	FText GetMeleeTargetDisplayName() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee")
	FText GetMeleeTargetPrompt(APawn* AttackingPawn) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Melee")
	FVector GetMeleeTargetAnchorWorldLocation() const;
};
