#pragma once
#include "CoreMinimal.h"
#include "space/Items/JTSReadableDocument.h"
#include "JTSMerchant.generated.h"
UCLASS(Blueprintable)
class SPACE_API AJTSMerchant : public AJTSReadableDocument
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName GiftStateId;
    UPROPERTY(EditAnywhere) FName DockModuleId;
    UPROPERTY(EditAnywhere) FName GiftCoordinateId;
    UPROPERTY(EditAnywhere) FName GiftImportantItemId;
    UPROPERTY(EditAnywhere) TObjectPtr<UJTSReadableDocumentData> RepeatDialogue;
    virtual FText GetInteractionPrompt_Implementation(APawn*) const override;
    virtual void Interact_Implementation(APawn* Pawn) override;
};
