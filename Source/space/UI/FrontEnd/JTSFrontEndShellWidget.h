#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "space/UI/FrontEnd/JTSAvatarTypes.h"
#include "space/Core/JTSExpeditionTypes.h"
#include "JTSFrontEndShellWidget.generated.h"

/** Routes editor-authored pages and presents subsystem state. Contains no layout or art paths. */
UCLASS()
class SPACE_API UJTSFrontEndShellWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FrontEnd") bool bLobby = false;
 UPROPERTY(EditDefaultsOnly, Category="FrontEnd") TObjectPtr<UJTSAvatarCatalog> AvatarCatalog;
 UFUNCTION(BlueprintCallable) void DispatchAction(FName Action,int32 Index=0);
 UFUNCTION(BlueprintCallable) void Navigate(FName Page);
 UFUNCTION(BlueprintPure) FName GetCurrentPage() const { return CurrentPage; }
 UFUNCTION(BlueprintPure) UWidget* FindControl(FName Name) const { return GetWidgetFromName(Name); }
 /** Outline drawn on the selected option or swatch; the button's authored brush is kept otherwise. */
 UPROPERTY(EditDefaultsOnly, Category="FrontEnd") FLinearColor SelectionOutline=FLinearColor(0.f,.8f,1.f,1.f);
 UPROPERTY(EditDefaultsOnly, Category="FrontEnd") float SelectionOutlineWidth=2.f;
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 void Back();
 void Refresh();
 void Preview();
 void RefreshAvatarViews();
 void EditAvatar();
 void RenameAvatar(const FString& Name);
 bool CanEditAppearance() const;
 void SetText(FName Name,const FString& Text);
 FString Input(FName Name) const;
 void Confirm(FName Action,int32 Index,const FString& Message);
 bool Busy() const;
 class AJTSPlayerState* PlayerAt(int32 Index) const;
 UFUNCTION() void OperationFinished(bool bSucceeded,const FString& Message);
 FColor& PartColor();
 void SyncColorControls();
 void ApplyHSV();
 void RefreshColorReadout();
 UFUNCTION() void HueChanged(float Value);
 UFUNCTION() void SaturationChanged(float Value);
 UFUNCTION() void BrightnessChanged(float Value);
 TArray<FButtonStyle> AuthoredStyles;
 TArray<bool> SelectedStates;
 int32 ColorPart=0;
 float Hue=0,Saturation=0,Brightness=1;
 bool bSyncingColor=false;
 bool bHeadView=false;
 UPROPERTY(Transient) TObjectPtr<class UWidgetSwitcher> Pages;
 UPROPERTY(Transient) TObjectPtr<class UJTSFrontEndSettingsModel> SettingsModel;
 UPROPERTY(Transient) TObjectPtr<UWidget> Confirmation;
 UPROPERTY(Transient) TArray<TObjectPtr<class UJTSMenuActionButton>> Actions;
 UPROPERTY(Transient) TArray<TObjectPtr<class UJTSFrontEndAvatarWidget>> AvatarViews;
 TMap<FName,TWeakObjectPtr<UWidget>> Focus;
 TWeakObjectPtr<UWidget> ModalReturnFocus;
 TArray<FName> History;
 TArray<FJTSExpeditionSaveSummary> SaveSummaries;
 FName CurrentPage,PendingAction;
 FName ConnectionReturn;
 int32 PendingIndex=0,SaveSlot=1,MaximumPlayers=4,SelectedListing=INDEX_NONE,ListingOffset=0;
 bool bResume=false,bPrivate=false,bProfileSent=false;
 FJTSAvatarAppearance Draft;
 float RefreshElapsed=0;
 FString Status;
};
