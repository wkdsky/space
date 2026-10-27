#include "space/UI/FrontEnd/JTSFrontEndShellWidget.h"
#include "space/UI/FrontEnd/JTSMenuActionButton.h"
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
 TArray<UWidget*> Widgets; WidgetTree->GetAllWidgets(Widgets); Actions.Reset();
 for (UWidget* W:Widgets) if (auto* B=Cast<UJTSMenuActionButton>(W))
 { B->OnAction.RemoveAll(this); B->OnAction.AddUObject(this,&UJTSFrontEndShellWidget::DispatchAction); Actions.Add(B); }
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
  Draft=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile->Appearance;
  if (auto* C=Cast<AJTSPlayerController>(GetOwningPlayer())) C->ServerSetReady(false);
  Preview();
 }
 else for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It) It->ShowView(TEXT("Home"));
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
 if (E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Right) { Back(); return FReply::Handled(); }
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
 for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It) { It->ShowView(TEXT("Full")); It->PreviewAppearance(Draft); }
 // Lobby preview is local until Apply; cancel restores the authoritative appearance.
 if (bLobby) if (auto* Pawn=GetOwningPlayerPawn()) if (auto* C=Pawn->FindComponentByClass<UJTSAvatarAppearanceComponent>()) C->Apply(Draft);
}
AJTSPlayerState* UJTSFrontEndShellWidget::PlayerAt(int32 Index) const
{
 auto* GS=GetWorld()->GetGameState<AJTSGameState>(); if (!GS) return nullptr;
 TArray<AJTSPlayerState*> Players;
 for (APlayerState* P:GS->PlayerArray) if (auto* J=Cast<AJTSPlayerState>(P)) Players.Add(J);
 Players.Sort([](const AJTSPlayerState& A,const AJTSPlayerState& B){return A.GetPlayerId()<B.GetPlayerId();});
 return Players.IsValidIndex(Index) ? Players[Index] : nullptr;
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
 else if (A==TEXT("Suit") || A==TEXT("Eyes") || A==TEXT("Mouth") || A==TEXT("Helmet") || A==TEXT("Upper") || A==TEXT("Lower"))
 {
  if (I<0 || I>=5) return;
  if (A==TEXT("Suit")) Draft.Suit=I; else if (A==TEXT("Eyes")) Draft.Eyes=I; else if (A==TEXT("Mouth")) Draft.Mouth=I; else if (A==TEXT("Helmet")) Draft.Helmet=I; else if (A==TEXT("Upper")) Draft.Upper=I; else Draft.Lower=I;
  Preview();
 }
 else if (A==TEXT("HeadView")) { for (TActorIterator<AJTSEntryPresentationStage> It(GetWorld());It;++It) It->ShowView(TEXT("Head")); }
 else if (A==TEXT("FullView")) Preview();
 else if (A==TEXT("Random")) { Draft.Suit=FMath::RandRange(0,4); Draft.Eyes=FMath::RandRange(0,4); Draft.Mouth=FMath::RandRange(0,4); Draft.Helmet=FMath::RandRange(0,4); Draft.Upper=FMath::RandRange(0,4); Draft.Lower=FMath::RandRange(0,4); Preview(); }
 else if (A==TEXT("Reset")) { Draft=FJTSAvatarAppearance(); Preview(); }
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
 else if (A==TEXT("CloseJoining") && C) C->RequestCloseJoining();
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
  { C->ServerSetAppearance(GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile->Appearance); bProfileSent=true; }
 }
 // Camera stays fixed in lobby on each local client, including remote clients.
 if (bLobby) for (TActorIterator<AJTSPreLaunchLobbyStage> It(GetWorld());It;++It) { GetOwningPlayer()->SetViewTarget(*It); break; }
 Refresh();
}
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
  SetText(*FString::Printf(TEXT("CrewLabel%d"),I),Player ? FString::Printf(TEXT("%s%s\n%s%s"),Player->IsExpeditionHost() ? TEXT("★ ") : TEXT(""),*Player->GetPlayerName().Left(14),Player->IsReady() ? TEXT("已准备") : TEXT("未准备"),Voice->IsPlayerTalking(Player->GetVoiceIdentityString()) ? TEXT(" · 正在说话") : TEXT("")) : TEXT("等待队员\n邀请好友或分享房间码"));
 }
 SetText(TEXT("LobbyCode"),FString::Printf(TEXT("房间码  %s  ·  %d/%d  ·  %s"),*Online->GetCurrentJoinCode(),Count,Online->GetCurrentMaximumPlayers(),GS && !GS->IsAcceptingNewPlayers() ? TEXT("加入已关闭") : TEXT("可加入")));
 SetText(TEXT("ReadyLabel"),Me && Me->IsReady() ? TEXT("取消准备") : TEXT("准备就绪"));
 SetText(TEXT("MicLabel"),Voice->IsInputMuted() ? TEXT("麦克风已静音") : TEXT("麦克风开启"));
 SetText(TEXT("LobbyHint"),GS && GS->GetLaunchDeadline()>0 ? FString::Printf(TEXT("即将出发  %.0f"),FMath::Max(0.,FMath::CeilToDouble(GS->GetLaunchDeadline()-GS->GetSynchronizedServerTimeSeconds()))) : TEXT("所有队员（包括房主）准备后即可出发 · 单人也可开始"));
 for (UJTSMenuActionButton* B:Actions)
 {
  bool Enabled=!BusyNow;
  if (B->Action==TEXT("Delete")) Enabled &= SaveSummaries.IsValidIndex(B->Index-1) && SaveSummaries[B->Index-1].bOccupied;
  if (B->Action==TEXT("Listing")) Enabled &= Listings.IsValidIndex(ListingOffset+B->Index);
  if (B->Action==TEXT("Launch")) Enabled &= Me && Me->IsExpeditionHost() && Count>0 && AllReady && GS && GS->GetLaunchDeadline()==0;
  if (B->Action==TEXT("CloseJoining")) Enabled &= Me && Me->IsExpeditionHost() && GS && GS->IsAcceptingNewPlayers();
  if (B->Action==TEXT("Kick")) { auto* Target=PlayerAt(B->Index); Enabled &= Me && Me->IsExpeditionHost() && Target && !Target->IsExpeditionHost(); }
  if (B->Action==TEXT("MutePlayer")) Enabled &= PlayerAt(B->Index)!=nullptr;
  if (B->Action==TEXT("Ready") || B->Action==TEXT("Appearance")) Enabled &= !bLobby || (GS && GS->GetLaunchDeadline()==0);
  if (B->Action==TEXT("VideoKeep") || B->Action==TEXT("VideoRevert")) Enabled=SettingsModel->IsVideoPending();
  B->SetIsEnabled(Enabled);
  bool Selected=(B->Action==TEXT("Suit") && Draft.Suit==B->Index)||(B->Action==TEXT("Eyes") && Draft.Eyes==B->Index)||(B->Action==TEXT("Mouth") && Draft.Mouth==B->Index)||(B->Action==TEXT("Helmet") && Draft.Helmet==B->Index)||(B->Action==TEXT("Upper") && Draft.Upper==B->Index)||(B->Action==TEXT("Lower") && Draft.Lower==B->Index);
  B->SetBackgroundColor(Selected ? FLinearColor(1,.4f,.08f,1) : FLinearColor::White);
 }
}
