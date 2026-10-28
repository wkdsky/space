#include "space/UI/FrontEnd/JTSFrontEndAvatarWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "space/Systems/JTSLocalProfileSubsystem.h"
#include "space/Player/JTSPlayerState.h"

TSharedRef<SWidget> UJTSFrontEndAvatarWidget::RebuildWidget()
{
 Initialize();
 if (!WidgetTree->RootWidget)
 {
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("AvatarCanvas"));
  Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  WidgetTree->RootWidget=Root;
  NameEditor=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("AvatarNameEditor"));
  Root->AddChildToCanvas(NameEditor)->SetSize(FVector2D(320,48));
  NameEditor->SetVisibility(ESlateVisibility::Collapsed);
  NameEditor->SetSelectAllTextWhenFocused(true);
  NameEditor->SetRevertTextOnEscape(true);
  NameEditor->OnTextCommitted.AddUniqueDynamic(this,&UJTSFrontEndAvatarWidget::NameCommitted);
 }
 return Super::RebuildWidget();
}

void UJTSFrontEndAvatarWidget::NativeConstruct()
{
 Super::NativeConstruct();
 SetIsFocusable(true);
 ForceVolatile(true); // Projected labels follow animation even beneath an invalidation panel.
 if (NameEditor)
 {
  // Slate retains the style address, so it must live for the lifetime of this widget.
  NameEditorStyle.TextStyle.SetFont(NameFont);
  NameEditor->SetWidgetStyle(NameEditorStyle);
 }
}

void UJTSFrontEndAvatarWidget::NativeDestruct()
{
 CancelNameEdit(); ClearHighlight();
 OnEditAvatar.Clear();
 OnRenameAvatar.Clear();
 Super::NativeDestruct();
}

void UJTSFrontEndAvatarWidget::SetAvatars(const TArray<FJTSFrontEndAvatarTarget>& InAvatars)
{
 Avatars=InAvatars;
 if (!Avatars.IsValidIndex(EditingAvatar) || !Avatars[EditingAvatar].bEditable) CancelNameEdit();
 if (!Avatars.IsValidIndex(HoveredAvatar) || !Avatars[HoveredAvatar].bEditable) SetHovered(INDEX_NONE);
 if (!Avatars.IsValidIndex(PressedAvatar) || !Avatars[PressedAvatar].bEditable) PressedAvatar=INDEX_NONE;
 SetIsFocusable(EditableAvatar()!=INDEX_NONE);
 bool KeepHighlight=false;
 for (const auto& A:Avatars) KeepHighlight |= A.bEditable && A.Mesh==HighlightedMesh.Get();
 if (!KeepHighlight) ClearHighlight();
}

bool UJTSFrontEndAvatarWidget::Project(const FGeometry& G,const FVector& World,FVector2D& Local) const
{
 FVector2D Viewport;
 if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,Viewport,false)) return false;
 const FGeometry View=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
 Local=G.AbsoluteToLocal(View.LocalToAbsolute(Viewport));
 return true;
}

int32 UJTSFrontEndAvatarWidget::HitAvatar(const FGeometry& G,const FVector2D& Screen) const
{
 auto* PC=GetOwningPlayer();
 if (!PC) return INDEX_NONE;
 FVector2D Pixel,Viewport;
 USlateBlueprintLibrary::AbsoluteToViewport(this,Screen,Pixel,Viewport);
 FVector Start,Direction;
 if (!PC->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Start,Direction)) return INDEX_NONE;
 const FVector End=Start+Direction*100000.f;
 for (int32 I=0;I<Avatars.Num();++I)
 {
  const auto& A=Avatars[I];
  auto* Mesh=A.Mesh.Get();
  if (!A.bEditable || !IsValid(Mesh) || !Mesh->IsVisible()) continue;
  const float Scale=Mesh->GetComponentScale().GetAbsMax();
  for (const auto& Region:PointerRegions)
  {
   if (!Mesh->DoesSocketExist(Region.StartBone) || !Mesh->DoesSocketExist(Region.EndBone)) continue;
   FVector RayPoint,BodyPoint;
   FMath::SegmentDistToSegmentSafe(Start,End,Mesh->GetSocketLocation(Region.StartBone),Mesh->GetSocketLocation(Region.EndBone),RayPoint,BodyPoint);
   if (FVector::DistSquared(RayPoint,BodyPoint)<=FMath::Square(Region.Radius*Scale)) return I;
  }
 }
 return INDEX_NONE;
}

int32 UJTSFrontEndAvatarWidget::HitNameplate(const FGeometry& G,const FVector2D& Screen) const
{
 const FVector2D Pointer=G.AbsoluteToLocal(Screen);
 for (int32 I=0;I<Avatars.Num();++I)
 {
  const auto& A=Avatars[I]; FVector2D Anchor;
  if (A.bEditable && IsValid(A.Mesh) && A.Mesh->IsVisible()
      && Project(G,A.Mesh->GetSocketLocation(NameAnchor)+NameOffset,Anchor)
      && FSlateRect(Anchor.X-160,Anchor.Y-62,Anchor.X+160,Anchor.Y+12).ContainsPoint(Pointer)) return I;
 }
 return INDEX_NONE;
}
void UJTSFrontEndAvatarWidget::UpdateHovered(const FGeometry& G,const FVector2D& Screen)
{
 const int32 Name=HitNameplate(G,Screen);
 bHoveredName=Name!=INDEX_NONE;
 SetHovered(bHoveredName ? Name : HitAvatar(G,Screen));
}

int32 UJTSFrontEndAvatarWidget::EditableAvatar() const
{
 for (int32 I=0;I<Avatars.Num();++I) if (Avatars[I].bEditable && IsValid(Avatars[I].Mesh)) return I;
 return INDEX_NONE;
}

int32 UJTSFrontEndAvatarWidget::ActiveAvatar() const
{
 return IsEditingName() ? INDEX_NONE : bKeyboardFocus ? EditableAvatar() : HoveredAvatar;
}

void UJTSFrontEndAvatarWidget::PlayFeedback(USoundBase* Sound) const
{
 if (!Sound || !GetGameInstance()) return;
 const auto* Local=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>();
 const float Volume=Local && Local->Profile ? Local->Profile->MasterVolume*Local->Profile->EffectsVolume : 1.f;
 UGameplayStatics::PlaySound2D(this,Sound,Volume);
}

void UJTSFrontEndAvatarWidget::SetHovered(int32 Index)
{
 if (HoveredAvatar==Index) return;
 HoveredAvatar=Index;
 if (Index!=INDEX_NONE) PlayFeedback(FocusSound);
}

void UJTSFrontEndAvatarWidget::ClearHighlight()
{
 if (HighlightedMesh.IsValid())
 {
  HighlightedMesh->SetRenderCustomDepth(bOriginalCustomDepth);
  HighlightedMesh->SetCustomDepthStencilValue(OriginalStencil);
  HighlightedMesh->SetCustomDepthStencilWriteMask(static_cast<ERendererStencilMask>(OriginalStencilMask));
 }
 if (HighlightCamera.IsValid() && HoverMID) HighlightCamera->RemoveBlendable(HoverMID);
 HighlightCamera.Reset(); HighlightedMesh.Reset(); Highlight=0.f;
}

void UJTSFrontEndAvatarWidget::NativeTick(const FGeometry& G,float Delta)
{
 Super::NativeTick(G,Delta);
 // Only this small presentation layer updates each frame; no world scans or physics scene queries.
 if (IsEditingName() && NameEditor && Avatars.IsValidIndex(EditingAvatar) && IsValid(Avatars[EditingAvatar].Mesh))
 {
  FVector2D Anchor;
  if (Project(G,Avatars[EditingAvatar].Mesh->GetSocketLocation(NameAnchor)+NameOffset,Anchor))
   CastChecked<UCanvasPanelSlot>(NameEditor->Slot)->SetPosition(Anchor-FVector2D(160,62));
 }
 if (IsHovered() && !bKeyboardFocus && !IsEditingName() && FSlateApplication::IsInitialized()) UpdateHovered(G,FSlateApplication::Get().GetCursorPos());
 const int32 Active=ActiveAvatar();
 auto* Mesh=Avatars.IsValidIndex(Active) ? Avatars[Active].Mesh.Get() : nullptr;
 if (Mesh && Mesh!=HighlightedMesh.Get())
 {
  ClearHighlight();
  auto* ViewTarget=GetOwningPlayer() ? GetOwningPlayer()->GetViewTarget() : nullptr;
  auto* Camera=ViewTarget ? ViewTarget->FindComponentByClass<UCameraComponent>() : nullptr;
  if (HoverMaterial && Camera)
  {
   if (!HoverMID) HoverMID=UMaterialInstanceDynamic::Create(HoverMaterial,this);
   HighlightedMesh=Mesh; HighlightCamera=Camera;
   bOriginalCustomDepth=Mesh->bRenderCustomDepth; OriginalStencil=Mesh->CustomDepthStencilValue;
   OriginalStencilMask=static_cast<uint8>(Mesh->CustomDepthStencilWriteMask);
   Mesh->SetCustomDepthStencilValue(OutlineStencil);
   Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_255);
   Mesh->SetRenderCustomDepth(true);
   HoverMID->SetVectorParameterValue(TEXT("Tint"),Accent);
   HoverMID->SetScalarParameterValue(TEXT("Width"),OutlineWidth);
   HoverMID->SetScalarParameterValue(TEXT("Stencil"),OutlineStencil);
   Camera->AddOrUpdateBlendable(HoverMID,1.f);
  }
 }
 Highlight=FMath::FInterpTo(Highlight,Mesh ? 1.f : 0.f,Delta,16.f);
 if (HoverMID) HoverMID->SetScalarParameterValue(TEXT("Strength"),Highlight);
 if (!Mesh && Highlight<.01f) ClearHighlight();
}

int32 UJTSFrontEndAvatarWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
 Layer=Super::NativePaint(Args,G,Cull,Out,Layer,Style,bEnabled);
 const auto FontMeasure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const FSlateRoundedBoxBrush Plate(FLinearColor(.012f,.024f,.034f,.9f),10.f);
 const FSlateRoundedBoxBrush Line(Accent,2.f);
 for (int32 I=0;I<Avatars.Num();++I)
 {
  const auto& A=Avatars[I];
  if (!IsValid(A.Mesh) || !A.Mesh->IsVisible()) continue;
  FVector2D Anchor;
  // Paint geometry is window-relative; projection and pointer events use desktop space.
  if (!Project(GetTickSpaceGeometry(),A.Mesh->GetSocketLocation(NameAnchor)+NameOffset,Anchor)) continue;
  if (Anchor.X<0 || Anchor.X>G.GetLocalSize().X || Anchor.Y<70 || Anchor.Y>G.GetLocalSize().Y) continue;
  const bool Active=ActiveAvatar()==I;
  const bool Editing=EditingAvatar==I;
  const FVector2D Pos(Anchor.X-160,Anchor.Y-62);
  auto Geometry=[&](FVector2D P,FVector2D Size){return G.ToPaintGeometry(Size,FSlateLayoutTransform(P));};
  if (!Editing)
  {
   FSlateDrawElement::MakeBox(Out,Layer+1,Geometry(Pos,FVector2D(320,74)),&Plate,ESlateDrawEffect::None,Plate.GetTint(Style));
   FSlateDrawElement::MakeBox(Out,Layer+2,Geometry(Pos+FVector2D(Active ? 28 : 70,0),FVector2D(Active ? 264 : 180,Active ? 3 : 1)),&Line,ESlateDrawEffect::None,Accent*FLinearColor(1,1,1,Active ? 1.f : .3f));
  }
  auto Text=[&](const FString& Value,const FSlateFontInfo& Font,float Y,FLinearColor Tint)
  {
   const FVector2D Size=FontMeasure->Measure(Value,Font);
   FSlateDrawElement::MakeText(Out,Layer+3,Geometry(FVector2D(Anchor.X-Size.X*.5f,Pos.Y+Y),Size),Value,Font,ESlateDrawEffect::None,Tint*Style.GetColorAndOpacityTint());
  };
  FString Name=A.Name.IsEmpty() ? TEXT("宇航员") : A.Name;
  if (FontMeasure->Measure(Name,NameFont).X>288)
  {
   while (!Name.IsEmpty() && FontMeasure->Measure(Name+TEXT("…"),NameFont).X>288) Name.LeftChopInline(1);
   Name+=TEXT("…");
  }
  if (!Editing) Text(Name,NameFont,10,FLinearColor(.92f,.96f,1.f,1));
  Text(Editing ? TEXT("回车保存 · Esc 取消") : Active ? bHoveredName ? TEXT("修改名字 · 点击") : TEXT("编辑外观 · 点击 / 确认") : A.Status,DetailFont,Editing ? 53 : 43,Active || Editing ? Accent : FLinearColor(.56f,.66f,.72f,1));
 }
 return Layer+3;
}

FReply UJTSFrontEndAvatarWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
 bKeyboardFocus=false;
 if (!IsEditingName()) UpdateHovered(G,E.GetScreenSpacePosition());
 return FReply::Unhandled();
}
void UJTSFrontEndAvatarWidget::NativeOnMouseLeave(const FPointerEvent& E)
{
 SetHovered(INDEX_NONE); PressedAvatar=INDEX_NONE; bHoveredName=false;
 Super::NativeOnMouseLeave(E);
}
FReply UJTSFrontEndAvatarWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
 PressedAvatar=HitNameplate(G,E.GetScreenSpacePosition()); bPressedName=PressedAvatar!=INDEX_NONE;
 if (!bPressedName) PressedAvatar=HitAvatar(G,E.GetScreenSpacePosition());
 if (E.GetEffectingButton()==EKeys::LeftMouseButton && PressedAvatar!=INDEX_NONE) return FReply::Handled().SetUserFocus(TakeWidget(),EFocusCause::Mouse).CaptureMouse(TakeWidget());
 PressedAvatar=INDEX_NONE; return FReply::Unhandled();
}
FReply UJTSFrontEndAvatarWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
 const bool Activate=E.GetEffectingButton()==EKeys::LeftMouseButton && PressedAvatar!=INDEX_NONE
  && (bPressedName ? HitNameplate(G,E.GetScreenSpacePosition()) : HitAvatar(G,E.GetScreenSpacePosition()))==PressedAvatar;
 const int32 Clicked=PressedAvatar;
 PressedAvatar=INDEX_NONE;
 if (Activate)
 {
  PlayFeedback(ClickSound); ClearHighlight(); SetHovered(INDEX_NONE);
  if (bPressedName) BeginNameEdit(Clicked); else OnEditAvatar.Broadcast();
  return FReply::Handled().ReleaseMouseCapture();
 }
 return FReply::Unhandled().ReleaseMouseCapture();
}
FReply UJTSFrontEndAvatarWidget::NativeOnFocusReceived(const FGeometry& G,const FFocusEvent& E)
{
 bKeyboardFocus=E.GetCause()!=EFocusCause::Mouse;
 if (bKeyboardFocus && EditableAvatar()!=INDEX_NONE) PlayFeedback(FocusSound);
 return FReply::Handled();
}
void UJTSFrontEndAvatarWidget::NativeOnFocusLost(const FFocusEvent& E)
{
 bKeyboardFocus=false; PressedAvatar=INDEX_NONE;
 Super::NativeOnFocusLost(E);
}
FReply UJTSFrontEndAvatarWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if (IsEditingName() && !E.IsRepeat() && (E.GetKey()==EKeys::Enter || E.GetKey()==EKeys::Gamepad_FaceButton_Bottom))
 { CommitNameEdit(); return FReply::Handled(); }
 if (E.GetKey()==EKeys::F2 && !E.IsRepeat()) { BeginNameEdit(EditableAvatar()); return FReply::Handled(); }
 if (EditableAvatar()!=INDEX_NONE && !E.IsRepeat() && (E.GetKey()==EKeys::Enter || E.GetKey()==EKeys::SpaceBar || E.GetKey()==EKeys::Gamepad_FaceButton_Bottom))
 { PlayFeedback(ClickSound); ClearHighlight(); bKeyboardFocus=false; OnEditAvatar.Broadcast(); return FReply::Handled(); }
 return Super::NativeOnKeyDown(G,E);
}
FCursorReply UJTSFrontEndAvatarWidget::NativeOnCursorQuery(const FGeometry& G,const FPointerEvent& E)
{
 if (HitNameplate(G,E.GetScreenSpacePosition())!=INDEX_NONE) return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
 return HitAvatar(G,E.GetScreenSpacePosition())!=INDEX_NONE ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

void UJTSFrontEndAvatarWidget::BeginNameEdit(int32 Index)
{
 if (!NameEditor || !Avatars.IsValidIndex(Index) || !Avatars[Index].bEditable) return;
 ClearHighlight(); SetHovered(INDEX_NONE); bKeyboardFocus=false;
 EditingAvatar=Index;
 NameEditor->SetError(FText::GetEmpty());
 NameEditor->SetText(FText::FromString(Avatars[Index].Name));
 NameEditor->SetVisibility(ESlateVisibility::Visible);
 NameEditor->SetKeyboardFocus();
}
void UJTSFrontEndAvatarWidget::CancelNameEdit()
{
 TGuardValue<bool> Guard(bChangingEditor,true);
 EditingAvatar=INDEX_NONE;
 if (NameEditor) NameEditor->SetVisibility(ESlateVisibility::Collapsed);
}
void UJTSFrontEndAvatarWidget::CommitNameEdit()
{
 if (NameEditor) NameCommitted(NameEditor->GetText(),ETextCommit::OnEnter);
}
void UJTSFrontEndAvatarWidget::NameCommitted(const FText& Text,ETextCommit::Type Commit)
{
 if (bChangingEditor || !IsEditingName()) return;
 if (Commit==ETextCommit::OnCleared) { CancelNameEdit(); return; }
 FString Name;
 if (!AJTSPlayerState::NormalizeDisplayName(Text.ToString(),Name))
 {
  NameEditor->SetError(FText::FromString(TEXT("名字需要 1–20 个字符，请勿使用换行或控制字符")));
  NameEditor->SetKeyboardFocus(); return;
 }
 CancelNameEdit(); OnRenameAvatar.Broadcast(Name);
 if (Commit==ETextCommit::OnEnter) SetKeyboardFocus();
}
