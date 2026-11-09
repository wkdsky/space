#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSExpeditionNoticeWidget.generated.h"
class UTextBlock;
UCLASS()
class SPACE_API UJTSExpeditionNoticeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetMessage(const FText& Message) { PendingMessage = Message; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float Dt) override;
private:
	FText PendingMessage;
	float Age = 0;
};
