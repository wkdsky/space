#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JTSPlanetSettlementEnemy.generated.h"

class AJTSPlanetAnchor;

/** An authored settlement can spawn any Actor implementing this contract. */
UINTERFACE(Blueprintable)
class UJTSPlanetSettlementEnemy : public UInterface
{
	GENERATED_BODY()
};

class SPACE_API IJTSPlanetSettlementEnemy
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Planet|Enemy")
	bool InitializeForSettlement(AJTSPlanetAnchor* Planet, FVector HomeLocation, FVector GroundLocation);

	/** Presentation hook; called by the server when an attack begins. */
	UFUNCTION(BlueprintNativeEvent, Category = "Planet|Enemy")
	void OnSettlementAttackStarted();
};
