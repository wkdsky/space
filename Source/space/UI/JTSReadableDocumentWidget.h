#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSReadableDocumentWidget.generated.h"
class UJTSReadableDocumentData;
UCLASS()
class SPACE_API UJTSReadableDocumentWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowDocument(UJTSReadableDocumentData* Data);
    virtual FReply NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E) override;
private:
    UFUNCTION() void Close();
};
