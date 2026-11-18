#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSFieldGuideWidget.generated.h"
class UJTSShipScannerComponent;
class UJTSAnalysisCatalog;
class AJTSSpacecraftActor;

UCLASS()
class SPACE_API UJTSScanStatusWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetScanner(UJTSShipScannerComponent* Value);
	virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
	TWeakObjectPtr<UJTSShipScannerComponent> Scanner;
};

/** Each local player owns a view of the expedition's shared knowledge. No scan-machine session. */
UCLASS()
class SPACE_API UJTSFieldGuideWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetFeedback(const FText& Value) { Feedback=Value; }
	UJTSAnalysisCatalog* GetCatalog() const;
	virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
	virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
	virtual void NativeOnMouseLeave(const FPointerEvent&) override;
	virtual FReply NativeOnMouseWheel(const FGeometry&,const FPointerEvent&) override;
	virtual FReply NativeOnKeyDown(const FGeometry&,const FKeyEvent&) override;
protected:
	virtual void NativeOnInitialized() override;
private:
	AJTSSpacecraftActor* GetShip() const;
	TArray<int32> VisibleEntries() const;
	FVector2D DesignPoint(const FGeometry&,FVector2D Screen) const;
	int32 Selected = INDEX_NONE;
	int32 FirstRow = 0;
	int32 Filter = 0;
	bool bCargo = false;
	FText Feedback;
};
