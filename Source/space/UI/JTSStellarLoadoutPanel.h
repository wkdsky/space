#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSStellarLoadoutPanel.generated.h"
class UJTSStellarLoadoutComponent;
class UJTSStellarAttachmentDialog;
class SWidget;
class SJTSStellarLoadoutView;

/** Character equipment: spare diamond and vertical weapon pairs. */
UCLASS()
class SPACE_API UJTSStellarLoadoutPanel : public UUserWidget
{
	GENERATED_BODY()
public:
	int32 GetSlotAtScreenPosition(FVector2D Screen) const;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
private:
	UJTSStellarLoadoutComponent* Loadout() const;
	void OpenDetails(int32 Index);
	void ResetDrag();
	TSharedPtr<SJTSStellarLoadoutView> SlotView;
	UPROPERTY(Transient) TObjectPtr<UJTSStellarAttachmentDialog> AttachmentDialog;
	int32 DraggedSlot = INDEX_NONE;
	FGuid DraggedId;
	FVector2D PressPosition;
	bool bDragging = false;
};
