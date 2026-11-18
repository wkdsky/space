#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "JTSShipDockDefinition.generated.h"

class AJTSShipDockActor;

UENUM(BlueprintType)
enum class EJTSShipDockService : uint8
{
	Custom,
	MaterialProcessing,
	WeaponModification,
	ShipTerminal,
	StellarExchange,
	Analysis
};

/** Blueprint/data selects the module, its art and its own hull hardpoint. Array order is immaterial. */
UCLASS(BlueprintType)
class SPACE_API UJTSShipDockDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ModuleId;
	/** Required baseline equipment also migrates into existing ship loadouts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequired = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<AJTSShipDockActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform MountTransform;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ParentComponentName;
};
