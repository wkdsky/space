#include "space/UI/FrontEnd/JTSFrontEndShellWidget.h"
#include "space/UI/FrontEnd/JTSMenuActionButton.h"
#include "space/UI/FrontEnd/JTSFrontEndAvatarWidget.h"
#include "space/UI/FrontEnd/JTSFrontEndSettingsModel.h"
#include "space/Systems/JTSLocalProfileSubsystem.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/Systems/JTSOnlineSessionSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Core/JTSGameState.h"
#include "space/World/JTSEntryPresentationStage.h"
#include "space/World/JTSPreLaunchLobbyStage.h"
#include "space/Components/JTSAvatarAppearanceComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/PanelWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Border.h"
#include "space/UI/FrontEnd/JTSGradientBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformApplicationMisc.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

namespace
{
 const TArray<FName> SettingNames={TEXT("Window"),TEXT("Resolution"),TEXT("Quality"),TEXT("Vsync"),TEXT("FPS"),TEXT("Master"),TEXT("Music"),TEXT("Effects"),TEXT("Voice"),TEXT("Mic"),TEXT("Input"),TEXT("Output"),TEXT("Sensitivity"),TEXT("Invert"),TEXT("UIScale"),TEXT("Motion")};
}
void UJTSFrontEndShellWidget::NativeConstruct()
{
 Super::NativeConstruct(); SetIsFocusable(true);
 if (!GetOwningPlayer() || !GetGameInstance() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 Pages=Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("FrontEndPages")));
 Confirmation=GetWidgetFromName(TEXT("ConfirmationLayer"));
 if (Confirmation) Confirmation->SetVisibility(ESlateVisibility::Collapsed);
 SettingsModel=NewObject<UJTSFrontEndSettingsModel>(this); SettingsModel->Initialize(GetGameInstance()); SettingsModel->SaveLocal();
 TArray<UWidget*> Widgets; WidgetTree->GetAllWidgets(Widgets); Actions.Reset(); AuthoredStyles.Reset(); SelectedStates.Reset();
 AvatarViews.Reset();
 for (UWidget* W:Widgets) if (auto* View=Cast<UJTSFrontEndAvatarWidget>(W))
 {
  View->OnEditAvatar.RemoveAll(this); View->OnEditAvatar.AddUObject(this,&UJTSFrontEndShellWidget::EditAvatar);
  View->OnRenameAvatar.RemoveAll(this); View->OnRenameAvatar.AddUObject(this,&UJTSFrontEndShellWidget::RenameAvatar);
  AvatarViews.Add(View);
 }
 for (UWidget* W:Widgets) if (auto* B=Cast<UJTSMenuActionButton>(W))
 {
  B->OnAction.RemoveAll(this); B->OnAction.AddUObject(this,&UJTSFrontEndShellWidget::DispatchAction);
  Actions.Add(B); AuthoredStyles.Add(B->GetStyle()); SelectedStates.Add(false);
  // Swatch fill comes from the catalog so palette edits need no widget changes.
  if (B->Action==TEXT("Swatch") && AvatarCatalog && AvatarCatalog->Colors.IsValidIndex(B->Index)) B->SetBackgroundColor(AvatarCatalog->Colors[B->Index]);
 }
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("HueSlider")))) S->OnValueChanged.AddUniqueDynamic(this,&UJTSFrontEndShellWidget::HueChanged);
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("SaturationSlider")))) S->OnValueChanged.AddUniqueDynamic(this,&UJTSFrontEndShellWidget::SaturationChanged);
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("BrightnessSlider")))) S->OnValueChanged.AddUniqueDynamic(this,&UJTSFrontEndShellWidget::BrightnessChanged);
 auto* Online=GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>();
 Online->OnSessionOperationFinished.AddUniqueDynamic(this,&UJTSFrontEndShellWidget::OperationFinished);
 History.Reset(); Navigate(bLobby ? TEXT("Lobby") : TEXT("Home")); History.Reset();
 Status=Online->GetLastError(); Refresh();
}
void UJTSFrontEndShellWidget::NativeDestruct()
{
 if (SettingsModel) { SettingsModel->RevertVideo(); SettingsModel->SaveLocal(); }
 if (GetGameInstance()) GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()->OnSessionOperationFinished.RemoveDynamic(this,&UJTSFrontEndShellWidget::OperationFinished);
 for (UJTSMenuActionButton* B:Actions) if (B) B->OnAction.RemoveAll(this);
 for (UJTSFrontEndAvatarWidget* View:AvatarViews) if (View) { View->SetAvatars({}); View->OnEditAvatar.RemoveAll(this); View->OnRenameAvatar.RemoveAll(this); }
 Super::NativeDestruct();
}
void UJTSFrontEndShellWidget::SetText(FName Name,const FString& Value) { if (auto* T=Cast<UTextBlock>(GetWidgetFromName(Name))) T->SetText(FText::FromString(Value)); }
FString UJTSFrontEndShellWidget::Input(FName Name) const { const auto* T=Cast<UEditableTextBox>(GetWidgetFromName(Name)); return T ? T->GetText().ToString().TrimStartAndEnd() : FString(); }
bool UJTSFrontEndShellWidget::Busy() const
{
 const auto S=GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>()->GetOperationState();
 return S!=EJTSSessionOperationState::Idle && S!=EJTSSessionOperationState::Failed;
}
void UJTSFrontEndShellWidget::Navigate(FName Page)
{
 if (!GetOwningPlayer() || !SettingsModel || !GetWorld() || GetWorld()->bIsTearingDown) return;
 if (!Pages || Page==CurrentPage) return;
 auto* Next=GetWidgetFromName(Page==TEXT("Settings") ? FName(TEXT("SettingsPage")) : Page); if (!Next) return;
 if (!CurrentPage.IsNone())
 {
  for (UJTSMenuActionButton* B:Actions) if (B->HasKeyboardFocus()) { Focus.Add(CurrentPage,B); break; }
  History.Add(CurrentPage);
 }
 CurrentPage=Page; Pages->SetActiveWidget(Next); Status.Reset();
 if (Page==TEXT("Saves")) SaveSummaries=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()->GetSaveSlotSummaries();
 if (Page==TEXT("Appearance"))
 {
  bHeadView=false;
  Draft=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile->Appearance;
  if (auto* C=Cast<AJTSPlayerController>(GetOwningPlayer())) C->ServerSetReady(false);
  SyncColorControls(); Preview();
 }
 else
 {
  for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It) It->ShowView(TEXT("Home"));
  for (TActorIterator<AJTSPreLaunchLobbyStage> It(GetWorld());It;++It) It->RestoreCamera();
 }
 Refresh();
 if (auto* Saved=Focus.Find(Page); Saved && Saved->IsValid()) Saved->Get()->SetKeyboardFocus();
 else for (UJTSMenuActionButton* B:Actions) if (B->IsVisible() && B->GetIsEnabled())
 {
  UWidget* Parent=B->GetParent(); while (Parent && Parent!=Next) Parent=Parent->GetParent();
  if (Parent==Next) { B->SetKeyboardFocus(); break; }
 }
}
void UJTSFrontEndShellWidget::Back()
{
 if (!PendingAction.IsNone()) { PendingAction=NAME_None; if (Confirmation) Confirmation->SetVisibility(ESlateVisibility::Collapsed); if (ModalReturnFocus.IsValid()) ModalReturnFocus->SetKeyboardFocus(); return; }
 if (SettingsModel->IsVideoPending()) { SettingsModel->RevertVideo(); return; }
 if (Busy()) { Status=TEXT("正在处理连接，请等待结果。"); Refresh(); return; }
 if (CurrentPage==TEXT("Appearance"))
 {
  Draft=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile->Appearance; Preview();
 }
 if (History.Num()) { const FName Previous=History.Pop(); CurrentPage=NAME_None; Navigate(Previous); }
 else if (bLobby) Confirm(TEXT("Leave"),0,TEXT("离开远征队？房主离开会关闭本次房间。"));
}
FReply UJTSFrontEndShellWidget::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if (!SettingsModel || !GetOwningPlayer()) return Super::NativeOnPreviewKeyDown(G,E);
 if (E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Right)
 {
  for (UJTSFrontEndAvatarWidget* View:AvatarViews) if (View && View->IsEditingName()) { View->CancelNameEdit(); View->SetKeyboardFocus(); return FReply::Handled(); }
  Back(); return FReply::Handled();
 }
 if (!PendingAction.IsNone() && (E.GetKey()==EKeys::Tab || E.GetKey()==EKeys::Left || E.GetKey()==EKeys::Right || E.GetKey()==EKeys::Gamepad_DPad_Left || E.GetKey()==EKeys::Gamepad_DPad_Right))
 {
  auto* Cancel=GetWidgetFromName(TEXT("CancelConfirmation")); auto* Accept=GetWidgetFromName(TEXT("AcceptConfirmation"));
  if (Cancel && Accept) (Cancel->HasKeyboardFocus() ? Accept : Cancel)->SetKeyboardFocus();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(G,E);
}
void UJTSFrontEndShellWidget::Confirm(FName A,int32 I,const FString& Message)
{
 for (UJTSMenuActionButton* B:Actions) if (B->HasKeyboardFocus()) { ModalReturnFocus=B; break; }
 PendingAction=A; PendingIndex=I; SetText(TEXT("ConfirmationMessage"),Message);
 if (Confirmation) Confirmation->SetVisibility(ESlateVisibility::Visible);
 if (auto* Cancel=GetWidgetFromName(TEXT("CancelConfirmation"))) Cancel->SetKeyboardFocus();
}
void UJTSFrontEndShellWidget::Preview()
{
 for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It) { It->ShowView(bHeadView ? TEXT("Head") : TEXT("Full")); It->PreviewAppearance(Draft); }
 RefreshColorReadout();
 // Lobby preview is local until Apply; cancel restores the authoritative appearance.
 if (bLobby) if (auto* Pawn=GetOwningPlayerPawn())
 {
  if (auto* C=Pawn->FindComponentByClass<UJTSAvatarAppearanceComponent>()) C->Apply(Draft);
  for (TActorIterator<AJTSPreLaunchLobbyStage> It(GetWorld());It;++It) It->ShowAppearance(Pawn,bHeadView);
 }
}
AJTSPlayerState* UJTSFrontEndShellWidget::PlayerAt(int32 Index) const
{
 auto* GS=GetWorld()->GetGameState<AJTSGameState>(); if (!GS) return nullptr;
 TArray<AJTSPlayerState*> Players;
 for (APlayerState* P:GS->PlayerArray) if (auto* J=Cast<AJTSPlayerState>(P)) Players.Add(J);
 Players.Sort([](const AJTSPlayerState& A,const AJTSPlayerState& B){return A.GetPlayerId()<B.GetPlayerId();});
 return Players.IsValidIndex(Index) ? Players[Index] : nullptr;
}
bool UJTSFrontEndShellWidget::CanEditAppearance() const
{
 if (!PendingAction.IsNone() || Busy()) return false;
 const auto* GS=GetWorld()->GetGameState<AJTSGameState>();
 return !bLobby || (GS && GS->GetLaunchDeadline()==0);
}
void UJTSFrontEndShellWidget::EditAvatar()
{
 if ((CurrentPage==TEXT("Home") || CurrentPage==TEXT("Lobby")) && CanEditAppearance())
 {
  for (UJTSFrontEndAvatarWidget* View:AvatarViews)
  {
   UWidget* Parent=View->GetParent(); while (Parent && Parent!=Pages->GetActiveWidget()) Parent=Parent->GetParent();
   if (Parent) { Focus.Add(CurrentPage,View); break; }
  }
  DispatchAction(TEXT("Appearance"));
 }
}
void UJTSFrontEndShellWidget::RefreshAvatarViews()
{
 TArray<FJTSFrontEndAvatarTarget> Targets;
 const auto* Me=GetOwningPlayer()->GetPlayerState<AJTSPlayerState>();
 if (CurrentPage==TEXT("Home"))
 {
  for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It)
  {
   FJTSFrontEndAvatarTarget Target; Target.Mesh=It->PreviewMesh;
   const auto* Player=GetOwningPlayer()->PlayerState.Get();
   const FString& SavedName=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile->DisplayName;
   Target.Name=!SavedName.IsEmpty() ? SavedName : Player && !Player->GetPlayerName().IsEmpty() ? Player->GetPlayerName() : TEXT("我的宇航员");
   Target.Status=TEXT("你  ·  悬停人物编辑外观"); Target.bEditable=CanEditAppearance();
   Targets.Add(Target); break;
  }
 }
 else if (CurrentPage==TEXT("Lobby"))
 {
  const auto* GS=GetWorld()->GetGameState<AJTSGameState>();
  for (int32 I=0;I<4;++I) if (auto* Player=PlayerAt(I)) if (auto* Pawn=Player->GetPawn())
  {
   FJTSFrontEndAvatarTarget Target; Target.Mesh=Pawn->FindComponentByClass<USkeletalMeshComponent>();
   Target.Name=Player->GetPlayerName(); Target.bEditable=Player==Me && CanEditAppearance();
   Target.Status=FString::Printf(TEXT("%s%s · %s"),Player==Me ? TEXT("你 · ") : TEXT(""),Player->IsExpeditionHost() ? TEXT("队长") : TEXT("队员"),GS && GS->GetLaunchDeadline()>0 ? TEXT("即将出发") : Player->IsReady() ? TEXT("已准备") : TEXT("未准备"));
   Targets.Add(Target);
  }
 }
 for (UJTSFrontEndAvatarWidget* View:AvatarViews)
 {
  // Inactive switcher pages do not tick, so clear them explicitly on navigation.
  UWidget* Parent=View->GetParent(); while (Parent && Parent!=Pages) { if (Parent==Pages->GetActiveWidget()) break; Parent=Parent->GetParent(); }
  View->SetAvatars(Parent==Pages->GetActiveWidget() ? Targets : TArray<FJTSFrontEndAvatarTarget>());
 }
}
void UJTSFrontEndShellWidget::RenameAvatar(const FString& Name)
{
 if (!CanEditAppearance()) return;
 auto* Local=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>();
 const FString Previous=Local->Profile->DisplayName;
 Local->Profile->DisplayName=Name;
 if (!Local->Save()) { Local->Profile->DisplayName=Previous; Status=TEXT("名字保存失败，请重试。"); }
 else if (auto* Controller=Cast<AJTSPlayerController>(GetOwningPlayer())) Controller->ServerSetDisplayName(Name);
 Refresh();
}
void UJTSFrontEndShellWidget::DispatchAction(FName A,int32 I)
{
 if (!GetOwningPlayer() || !GetGameInstance() || !GetWorld() || GetWorld()->bIsTearingDown || !SettingsModel) return;
 auto* Online=GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>();
 auto* Expedition=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
 auto* Local=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>();
 auto* Voice=GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>();
 auto* C=Cast<AJTSPlayerController>(GetOwningPlayer());
 if (A==TEXT("Back") || A==TEXT("Cancel")) { Back(); return; }
 if (A==TEXT("Confirm"))
 {
  FName Pending=PendingAction; int32 Index=PendingIndex; PendingAction=NAME_None;
  if (Confirmation) Confirmation->SetVisibility(ESlateVisibility::Collapsed);
  if (ModalReturnFocus.IsValid()) ModalReturnFocus->SetKeyboardFocus();
  if (Pending==TEXT("Delete")) { Expedition->DeleteExpeditionSlot(Index); SaveSummaries=Expedition->GetSaveSlotSummaries(); Refresh(); }
  else if (Pending==TEXT("Quit")) Online->RequestApplicationQuit();
  else if (Pending==TEXT("Leave")) Online->LeaveExpedition();
  else if (Pending==TEXT("Kick") && C) C->RequestKickPlayer(PlayerAt(Index));
  return;
 }
 if (!PendingAction.IsNone() || Busy()) return;
 if (A==TEXT("Appearance") && !CanEditAppearance()) return;
 if (A==TEXT("Home") || A==TEXT("Saves") || A==TEXT("Join") || A==TEXT("Appearance") || A==TEXT("Settings") || A==TEXT("Credits")) Navigate(A);
 else if (A==TEXT("Slot"))
 {
  SaveSlot=I; bResume=Expedition->HasSaveInSlot(I);
  if (auto* Name=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("ExpeditionName"))))
   Name->SetText(FText::FromString(bResume && SaveSummaries.IsValidIndex(I-1) ? SaveSummaries[I-1].DisplayName : FString::Printf(TEXT("远征 %02d"),I)));
  Navigate(TEXT("Setup"));
 }
 else if (A==TEXT("Delete")) Confirm(A,I,TEXT("永久删除此远征存档？此操作无法撤销。"));
 else if (A==TEXT("Players")) MaximumPlayers=FMath::Clamp(MaximumPlayers+I,1,4);
 else if (A==TEXT("Privacy")) bPrivate=I!=0;
 else if (A==TEXT("Create")) { ConnectionReturn=TEXT("Setup"); Navigate(TEXT("Connecting")); Status=TEXT("正在创建远征队…"); Online->HostExpedition(SaveSlot,Input(TEXT("ExpeditionName")),bResume,Input(TEXT("CustomCode")),Input(TEXT("HostPassword")),MaximumPlayers,bPrivate ? EJTSLobbyVisibility::Private : EJTSLobbyVisibility::Public); }
 else if (A==TEXT("Search")) { SelectedListing=INDEX_NONE; ListingOffset=0; Online->FindExpeditions(Input(TEXT("JoinCode"))); Status=TEXT("正在搜索可加入的远征队…"); }
 else if (A==TEXT("Listing")) SelectedListing=ListingOffset+I;
 else if (A==TEXT("ListingPage")) { ListingOffset=FMath::Clamp(ListingOffset+I*4,0,FMath::Max(0,((Online->GetDiscoveredListings().Num()-1)/4)*4)); SelectedListing=INDEX_NONE; }
 else if (A==TEXT("Connect")) { if (SelectedListing!=INDEX_NONE) { ConnectionReturn=TEXT("Join"); Navigate(TEXT("Connecting")); Online->JoinExpedition(SelectedListing,Input(TEXT("JoinPassword"))); } else Status=TEXT("请先选择一个远征队。"); }
 else if (A==TEXT("Suit") || A==TEXT("Eyes") || A==TEXT("Mouth"))
 {
  if (A==TEXT("Suit") && I>=0 && I<5) Draft.Suit=I;
  else if (A==TEXT("Eyes") && I>=0 && I<5) Draft.Eyes=I;
  else if (A==TEXT("Mouth") && I>=0 && I<5) Draft.Mouth=I;
  Preview();
 }
 else if (A==TEXT("ColorPart") && I>=0 && I<3) { ColorPart=I; SyncColorControls(); }
 else if (A==TEXT("Swatch") && AvatarCatalog && AvatarCatalog->Colors.IsValidIndex(I)) { PartColor()=AvatarCatalog->Colors[I].ToFColor(true); SyncColorControls(); Preview(); }
 else if (A==TEXT("HeadView")) { bHeadView=true; Preview(); }
 else if (A==TEXT("FullView")) { bHeadView=false; Preview(); }
 else if (A==TEXT("Random")) { Draft.Suit=FMath::RandRange(0,4); Draft.Eyes=FMath::RandRange(0,4); Draft.Mouth=FMath::RandRange(0,4); Draft.HelmetColor=FColor::MakeRandomColor(); Draft.UpperColor=FColor::MakeRandomColor(); Draft.LowerColor=FColor::MakeRandomColor(); SyncColorControls(); Preview(); }
 else if (A==TEXT("Reset")) { Draft=FJTSAvatarAppearance(); SyncColorControls(); Preview(); }
 else if (A==TEXT("ApplyAppearance"))
 {
  const auto Previous=Local->Profile->Appearance; Local->Profile->Appearance=Draft;
  if (!Local->Save()) { Local->Profile->Appearance=Previous; Status=TEXT("外观保存失败，请重试。"); }
  else { if (C) C->ServerSetAppearance(Draft); Back(); }
 }
 else if (A==TEXT("VideoApply")) SettingsModel->ApplyVideo();
 else if (A==TEXT("VideoKeep")) SettingsModel->KeepVideo();
 else if (A==TEXT("VideoRevert")) SettingsModel->RevertVideo();
 else if (SettingNames.Contains(A)) SettingsModel->Change(A,I);
 else if (A==TEXT("Ready") && C) C->RequestSetReady();
 else if (A==TEXT("Launch") && C) C->ServerRequestStartExpedition();
 else if (A==TEXT("CloseJoining") && C)
 {
  if (const auto* GS=GetWorld()->GetGameState<AJTSGameState>()) C->RequestSetJoiningOpen(!GS->IsAcceptingNewPlayers());
 }
 else if (A==TEXT("Copy")) { FPlatformApplicationMisc::ClipboardCopy(*Online->GetCurrentJoinCode()); Status=TEXT("房间码已复制。"); }
 else if (A==TEXT("Mute")) Voice->SetInputMuted(!Voice->IsInputMuted());
 else if (A==TEXT("MutePlayer")) { if (auto* P=PlayerAt(I)) Voice->SetPlayerMuted(P->GetVoiceIdentityString(),!Voice->IsPlayerMuted(P->GetVoiceIdentityString())); }
 else if (A==TEXT("Kick") && C) Confirm(A,I,TEXT("将此队员移出远征队？"));
 else if (A==TEXT("Leave")) Confirm(A,I,TEXT("离开远征队？房主离开将关闭房间，其他队员返回主菜单。"));
 else if (A==TEXT("Quit")) Confirm(A,I,TEXT("退出 Jump to Space？"));
 Refresh();
}
void UJTSFrontEndShellWidget::OperationFinished(bool bSucceeded,const FString& Message)
{
 if (!bSucceeded && CurrentPage==TEXT("Connecting")) { CurrentPage=NAME_None; if (History.Num()) History.Pop(); Navigate(ConnectionReturn); }
 Status=Message; Refresh();
}
void UJTSFrontEndShellWidget::NativeTick(const FGeometry& G,float D)
{
 Super::NativeTick(G,D);
 if (!SettingsModel || !GetOwningPlayer() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 SettingsModel->Tick(); RefreshElapsed+=D;
 if (RefreshElapsed<.2f) return; RefreshElapsed=0;
 if (bLobby && !bProfileSent)
 {
  if (auto* C=Cast<AJTSPlayerController>(GetOwningPlayer()); C && C->GetPlayerState<AJTSPlayerState>())
  {
   const auto* Profile=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
   C->ServerSetAppearance(Profile->Appearance);
   if (!Profile->DisplayName.IsEmpty()) C->ServerSetDisplayName(Profile->DisplayName);
   bProfileSent=true;
  }
 }
 // Camera stays fixed in lobby on each local client, including remote clients.
 if (bLobby) for (TActorIterator<AJTSPreLaunchLobbyStage> It(GetWorld());It;++It) { GetOwningPlayer()->SetViewTarget(*It); break; }
 Refresh();
}
FColor& UJTSFrontEndShellWidget::PartColor() { return ColorPart==0 ? Draft.HelmetColor : ColorPart==1 ? Draft.UpperColor : Draft.LowerColor; }
// HSB is edited in sRGB byte space so slider positions match what the player sees.
void UJTSFrontEndShellWidget::SyncColorControls()
{
 const FLinearColor HSV=FLinearColor(PartColor().R/255.f,PartColor().G/255.f,PartColor().B/255.f).LinearRGBToHSV();
 Hue=HSV.R; Saturation=HSV.G; Brightness=HSV.B;
 // USlider::SetValue broadcasts OnValueChanged; keep the exact part color instead of re-quantizing it.
 TGuardValue<bool> Guard(bSyncingColor,true);
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("HueSlider")))) S->SetValue(Hue/360.f);
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("SaturationSlider")))) S->SetValue(Saturation);
 if (auto* S=Cast<USlider>(GetWidgetFromName(TEXT("BrightnessSlider")))) S->SetValue(Brightness);
 const FColor Exact=PartColor(); ApplyHSV(); PartColor()=Exact;
}
void UJTSFrontEndShellWidget::ApplyHSV()
{
 auto Display=[](float H,float S,float V){ const FLinearColor C=FLinearColor(H,S,V).HSVToLinearRGB(); return FLinearColor::FromSRGBColor(FColor(FMath::RoundToInt(C.R*255),FMath::RoundToInt(C.G*255),FMath::RoundToInt(C.B*255))); };
 const FLinearColor C=FLinearColor(Hue,Saturation,Brightness).HSVToLinearRGB();
 PartColor()=FColor(FMath::RoundToInt(C.R*255),FMath::RoundToInt(C.G*255),FMath::RoundToInt(C.B*255));
 if (auto* Bar=Cast<UJTSGradientBar>(GetWidgetFromName(TEXT("HueBar"))))
 { TArray<FLinearColor> Stops; for (int32 I=0;I<=6;++I) Stops.Add(Display(I*60.f,1,1)); Bar->SetStops(Stops); }
 if (auto* Bar=Cast<UJTSGradientBar>(GetWidgetFromName(TEXT("SaturationBar")))) Bar->SetStops({Display(Hue,0,Brightness),Display(Hue,1,Brightness)});
 if (auto* Bar=Cast<UJTSGradientBar>(GetWidgetFromName(TEXT("BrightnessBar")))) Bar->SetStops({FLinearColor::Black,Display(Hue,Saturation,1)});
 RefreshColorReadout();
}
void UJTSFrontEndShellWidget::RefreshColorReadout()
{
 const FColor Colors[]={Draft.HelmetColor,Draft.UpperColor,Draft.LowerColor};
 for (int32 I=0;I<3;++I)
  if (auto* Chip=Cast<UBorder>(GetWidgetFromName(*FString::Printf(TEXT("PartColor%d"),I)))) Chip->SetBrushColor(FLinearColor::FromSRGBColor(Colors[I]));
 SetText(TEXT("HueValue"),FString::Printf(TEXT("%.0f°"),Hue));
 SetText(TEXT("SaturationValue"),FString::Printf(TEXT("%.0f%%"),Saturation*100.f));
 SetText(TEXT("BrightnessValue"),FString::Printf(TEXT("%.0f%%"),Brightness*100.f));
}
void UJTSFrontEndShellWidget::HueChanged(float Value) { if (bSyncingColor) return; Hue=Value*360.f; ApplyHSV(); Preview(); }
void UJTSFrontEndShellWidget::SaturationChanged(float Value) { if (bSyncingColor) return; Saturation=Value; ApplyHSV(); Preview(); }
void UJTSFrontEndShellWidget::BrightnessChanged(float Value) { if (bSyncingColor) return; Brightness=Value; ApplyHSV(); Preview(); }
void UJTSFrontEndShellWidget::Refresh()
{
 if (!SettingsModel || !GetOwningPlayer() || !GetGameInstance() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 auto* Online=GetGameInstance()->GetSubsystem<UJTSOnlineSessionSubsystem>();
 auto* Expedition=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>();
 auto* Voice=GetGameInstance()->GetSubsystem<UJTSVoiceSubsystem>();
 auto* P=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
 const bool BusyNow=Busy();
 SetText(TEXT("StatusLine"),Status);
 SetText(TEXT("ProviderStatus"),Online->IsUsingLanFallback() ? TEXT("局域网联机 · 1–4 人") : TEXT("在线联机 · 1–4 人"));
 for (int32 I=1;I<=4;++I)
 {
  const auto Meta=SaveSummaries.IsValidIndex(I-1) ? SaveSummaries[I-1] : FJTSExpeditionSaveSummary();
  SetText(*FString::Printf(TEXT("SlotLabel%d"),I),Meta.bOccupied ? FString::Printf(TEXT("%02d  %s\n继续远征  ·  %.0f 分钟"),I,*Meta.DisplayName,Meta.PlaytimeSeconds/60.0) : FString::Printf(TEXT("%02d  空存档\n创建新的远征"),I));
 }
 SetText(TEXT("SetupTitle"),bResume ? TEXT("继续远征 · 组建队伍") : TEXT("新远征 · 组建队伍"));
 SetText(TEXT("PlayerLimit"),FString::Printf(TEXT("%d / 4"),MaximumPlayers));
 SetText(TEXT("PrivacyLabel"),bPrivate ? TEXT("私密 · 通过房间码加入") : TEXT("公开 · 显示在远征队列表"));
 auto Listings=Online->GetDiscoveredListings();
 SetText(TEXT("ListingStatus"),FString::Printf(TEXT("发现 %d 支远征队  ·  第 %d 页"),Listings.Num(),ListingOffset/4+1));
 for (int32 I=0;I<4;++I)
 {
  const int32 N=ListingOffset+I;
  SetText(*FString::Printf(TEXT("ListingLabel%d"),I),Listings.IsValidIndex(N) ? FString::Printf(TEXT("%s%s  ·  %d/%d%s\n房间码 %s"),SelectedListing==N ? TEXT("● ") : TEXT(""),*Listings[N].HostName,Listings[N].CurrentPlayers,Listings[N].MaximumPlayers,Listings[N].bPasswordProtected ? TEXT("  ·  需要密码") : TEXT(""),*Listings[N].JoinCode) : TEXT("暂无可加入的远征队"));
 }
 for (const FName S:SettingNames) SetText(*FString::Printf(TEXT("Value_%s"),*S.ToString()),SettingsModel->Label(S));
 SetText(TEXT("VideoCountdown"),SettingsModel->IsVideoPending() ? FString::Printf(TEXT("保留此显示设置？%.0f 秒后恢复"),FMath::CeilToFloat(SettingsModel->SecondsLeft())) : TEXT("更改显示设置后有 15 秒确认时间"));
 SetRenderScale(FVector2D(P->UIScale)); SetRenderTransformPivot(FVector2D(.5,.5));
 for (TActorIterator<ALevelSequenceActor> It(GetWorld());It;++It) if (It->ActorHasTag(TEXT("FrontEndLoop")) && It->GetSequencePlayer())
 { auto* Player=It->GetSequencePlayer(); if (P->bReducedMotion && Player->IsPlaying()) Player->Pause(); else if (!P->bReducedMotion && !Player->IsPlaying()) Player->Play(); }
 auto* GS=GetWorld()->GetGameState<AJTSGameState>();
 const auto* Me=GetOwningPlayer()->GetPlayerState<AJTSPlayerState>();
 bool AllReady=true; int32 Count=0;
 for (int32 I=0;I<4;++I)
 {
  auto* Player=PlayerAt(I); if (Player) { ++Count; AllReady &= Player->IsReady(); }
  SetText(*FString::Printf(TEXT("CrewLabel%d"),I),Player ? FString::Printf(TEXT("%02d · %s\n%s%s"),I+1,Player==Me ? TEXT("本机队员") : Player->IsExpeditionHost() ? TEXT("队长") : TEXT("队员"),Player->IsReady() ? TEXT("已准备") : TEXT("未准备"),Voice->IsPlayerTalking(Player->GetVoiceIdentityString()) ? TEXT(" · 正在说话") : TEXT("")) : TEXT("等待队员\n邀请好友或分享房间码"));
 }
 SetText(TEXT("LobbyCode"),FString::Printf(TEXT("房间码  %s  ·  %d/%d  ·  %s"),*Online->GetCurrentJoinCode(),Count,Online->GetCurrentMaximumPlayers(),GS && !GS->IsAcceptingNewPlayers() ? TEXT("加入已关闭") : TEXT("可加入")));
 SetText(TEXT("ReadyLabel"),Me && Me->IsReady() ? TEXT("取消准备") : TEXT("准备就绪"));
 SetText(TEXT("MicLabel"),Voice->IsInputMuted() ? TEXT("麦克风已静音") : TEXT("麦克风开启"));
 SetText(TEXT("LobbyHint"),GS && GS->GetLaunchDeadline()>0 ? FString::Printf(TEXT("即将出发  %.0f"),FMath::Max(0.,FMath::CeilToDouble(GS->GetLaunchDeadline()-GS->GetSynchronizedServerTimeSeconds()))) : TEXT("所有队员（包括房主）准备后即可出发 · 单人也可开始"));
 RefreshAvatarViews();
 for (int32 N=0;N<Actions.Num();++N)
 {
  UJTSMenuActionButton* B=Actions[N];
  bool Enabled=!BusyNow;
  if (B->Action==TEXT("Delete")) Enabled &= SaveSummaries.IsValidIndex(B->Index-1) && SaveSummaries[B->Index-1].bOccupied;
  if (B->Action==TEXT("Listing")) Enabled &= Listings.IsValidIndex(ListingOffset+B->Index);
  if (B->Action==TEXT("Launch")) Enabled &= Me && Me->IsExpeditionHost() && Count>0 && AllReady && GS && GS->GetLaunchDeadline()==0;
  if (B->Action==TEXT("CloseJoining"))
  {
   Enabled &= Me && Me->IsExpeditionHost() && GS && GS->IsWaitingToStart() && GS->GetLaunchDeadline()==0;
   SetText(TEXT("Lobby_CloseJoining_Label"),GS && !GS->IsAcceptingNewPlayers() ? TEXT("重新开放加入") : TEXT("关闭加入"));
  }
  if (B->Action==TEXT("Kick")) { auto* Target=PlayerAt(B->Index); Enabled &= Me && Me->IsExpeditionHost() && Target && !Target->IsExpeditionHost(); }
  if (B->Action==TEXT("MutePlayer")) Enabled &= PlayerAt(B->Index)!=nullptr;
  if (B->Action==TEXT("Ready") || B->Action==TEXT("Appearance")) Enabled &= !bLobby || (GS && GS->GetLaunchDeadline()==0);
  if (B->Action==TEXT("VideoKeep") || B->Action==TEXT("VideoRevert")) Enabled=SettingsModel->IsVideoPending();
  B->SetIsEnabled(Enabled);
  const bool Selected=(B->Action==TEXT("Suit") && Draft.Suit==B->Index)||(B->Action==TEXT("Eyes") && Draft.Eyes==B->Index)||(B->Action==TEXT("Mouth") && Draft.Mouth==B->Index)||(B->Action==TEXT("ColorPart") && ColorPart==B->Index)
   ||(B->Action==TEXT("HeadView") && bHeadView)||(B->Action==TEXT("FullView") && !bHeadView)
   ||(B->Action==TEXT("Swatch") && AvatarCatalog && AvatarCatalog->Colors.IsValidIndex(B->Index) && AvatarCatalog->Colors[B->Index].ToFColor(true)==PartColor());
  if (SelectedStates[N]!=Selected)
  {
   SelectedStates[N]=Selected; FButtonStyle Style=AuthoredStyles[N];
   if (Selected) for (FSlateBrush* Brush:{&Style.Normal,&Style.Hovered,&Style.Pressed}) { Brush->OutlineSettings.Color=SelectionOutline; Brush->OutlineSettings.Width=SelectionOutlineWidth; }
   B->SetStyle(Style);
  }
 }
}
