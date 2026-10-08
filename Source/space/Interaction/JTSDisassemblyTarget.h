#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "JTSDisassemblyTarget.generated.h"
class APawn;
/** Explicit engineering opt-in. Mission objects and player property have no default implementation. */
UINTERFACE(BlueprintType)
class SPACE_API UJTSDisassemblyTarget : public UInterface { GENERATED_BODY() };
class SPACE_API IJTSDisassemblyTarget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Stellar|Engineering") bool CanDisassemble(APawn* Operator) const;
	/** Return false without mutating the target when its normal yield transaction cannot complete. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Stellar|Engineering") bool Disassemble(APawn* Operator);
};
