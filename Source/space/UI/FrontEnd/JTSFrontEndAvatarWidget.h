#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/EditableTextBox.h"
#include "JTSFrontEndAvatarWidget.generated.h"

class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Small, editor-authored capsules follow the presentation skeleton without physics bodies. */
USTRUCT(BlueprintType)
struct FJTSAvatarPointerRegion
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) FName StartBone;
 UPROPERTY(EditAnywhere) FName EndBone;
 UPROPERTY(EditAnywhere) float Radius=10.f;
};

USTRUCT(BlueprintType)
struct FJTSFrontEndAvatarTarget
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) TObjectPtr<USkeletalMeshComponent> Mesh;
 UPROPERTY(BlueprintReadWrite) FString Name;
 UPROPERTY(BlueprintReadWrite) FString Status;
 UPROPERTY(BlueprintReadWrite) bool bEditable=false;
};

DECLARE_MULTICAST_DELEGATE(FJTSOnEditAvatar);
DECLARE_MULTICAST_DELEGATE_OneParam(FJTSOnRenameAvatar,const FString&);

/** Local scene interaction and projected nameplates. Layout, skeleton and art are Blueprint-owned. */
UCLASS()
class SPACE_API UJTSFrontEndAvatarWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) void SetAvatars(const TArray<FJTSFrontEndAvatarTarget>& InAvatars);
 UFUNCTION(BlueprintPure) int32 GetHoveredAvatar() const { return HoveredAvatar; }
 UFUNCTION(BlueprintPure) bool IsEditingName() const { return EditingAvatar!=INDEX_NONE; }
 UFUNCTION(BlueprintCallable) void CancelNameEdit();
 UFUNCTION(BlueprintCallable) void CommitNameEdit();
 UFUNCTION(BlueprintCallable) void BeginNameEdit(int32 Index);
 UPROPERTY(EditAnywhere, Category="Avatar") TArray<FJTSAvatarPointerRegion> PointerRegions;
 UPROPERTY(EditAnywhere, Category="Avatar") FName NameAnchor;
 UPROPERTY(EditAnywhere, Category="Avatar") FVector NameOffset=FVector(0,0,48);
 UPROPERTY(EditAnywhere, Category="Avatar") FSlateFontInfo NameFont;
 UPROPERTY(EditAnywhere, Category="Avatar") FSlateFontInfo DetailFont;
 UPROPERTY(EditAnywhere, Category="Avatar") FLinearColor Accent=FLinearColor(.15f,.8f,.85f,1.f);
 UPROPERTY(EditAnywhere, Category="Avatar") TObjectPtr<UMaterialInterface> HoverMaterial;
 UPROPERTY(EditAnywhere, Category="Avatar",meta=(ClampMin="1",ClampMax="255")) int32 OutlineStencil=250;
 UPROPERTY(EditAnywhere, Category="Avatar",meta=(ClampMin="1",ClampMax="6")) float OutlineWidth=2.5f;
 UPROPERTY(EditAnywhere, Category="Avatar") FEditableTextBoxStyle NameEditorStyle;
 UPROPERTY(EditAnywhere, Category="Avatar") TObjectPtr<class USoundBase> FocusSound;
 UPROPERTY(EditAnywhere, Category="Avatar") TObjectPtr<class USoundBase> ClickSound;
 FJTSOnEditAvatar OnEditAvatar;
 FJTSOnRenameAvatar OnRenameAvatar;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
 virtual FReply NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnFocusReceived(const FGeometry& Geometry,const FFocusEvent& Event) override;
 virtual void NativeOnFocusLost(const FFocusEvent& Event) override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
 virtual FCursorReply NativeOnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) override;
private:
 bool Project(const FGeometry& Geometry,const FVector& World,FVector2D& Local) const;
 int32 HitAvatar(const FGeometry& Geometry,const FVector2D& Screen) const;
 int32 HitNameplate(const FGeometry& Geometry,const FVector2D& Screen) const;
 void UpdateHovered(const FGeometry& Geometry,const FVector2D& Screen);
 UFUNCTION() void NameCommitted(const FText& Text,ETextCommit::Type Commit);
 int32 EditableAvatar() const;
 int32 ActiveAvatar() const;
 void SetHovered(int32 Index);
 void ClearHighlight();
 void PlayFeedback(class USoundBase* Sound) const;
 UPROPERTY(Transient) TArray<FJTSFrontEndAvatarTarget> Avatars;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HoverMID;
 UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameEditor;
 TWeakObjectPtr<class UCameraComponent> HighlightCamera;
 TWeakObjectPtr<USkeletalMeshComponent> HighlightedMesh;
 bool bOriginalCustomDepth=false;
 int32 OriginalStencil=0;
 uint8 OriginalStencilMask=0;
 int32 EditingAvatar=INDEX_NONE;
 bool bChangingEditor=false;
 bool bHoveredName=false;
 bool bPressedName=false;
 int32 HoveredAvatar=INDEX_NONE;
 int32 PressedAvatar=INDEX_NONE;
 bool bKeyboardFocus=false;
 float Highlight=0.f;
};
