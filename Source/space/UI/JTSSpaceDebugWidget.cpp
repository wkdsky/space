#include "JTSSpaceDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Systems/JTSDebugViewSubsystem.h"
#include "space/World/JTSSpaceDebugTravelManager.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FText& Text, int32 Size)
	{
		auto* Label=Tree->ConstructWidget<UTextBlock>();Label->SetText(Text);
		auto Font=Label->GetFont();Font.Size=Size;Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(.78f,.85f,.90f)));return Label;
	}
	void SetButtonState(UButton* Button, UTextBlock* Label, bool Active)
	{
		const FLinearColor Fill=Active?FLinearColor(.035f,.28f,.22f):FLinearColor(.035f,.065f,.085f);
		const FLinearColor Edge=Active?FLinearColor(.16f,.85f,.6f):FLinearColor(.17f,.25f,.30f);
		FButtonStyle Style;
		Style.SetNormal(FSlateRoundedBoxBrush(Fill,6.f,Edge,1.f));
		Style.SetHovered(FSlateRoundedBoxBrush(Fill+FLinearColor(.04f,.06f,.07f,0),6.f,Edge*1.3f,1.f));
		Style.SetPressed(FSlateRoundedBoxBrush(Fill*.75f,6.f,Edge,1.f));
		Style.SetDisabled(FSlateRoundedBoxBrush(Fill,6.f,Edge,1.f));
		Style.SetNormalPadding(FMargin(8,6));Style.SetPressedPadding(FMargin(8,7,8,5));
		Button->SetStyle(Style);Button->SetRenderOpacity(Button->GetIsEnabled()||Active?1.f:.4f);
		Label->SetColorAndOpacity(FSlateColor(Active?FLinearColor(.65f,1.f,.83f):FLinearColor(.78f,.85f,.90f)));
	}
	void Section(UWidgetTree* Tree, UVerticalBox* Content, const FText& Name)
	{
		Content->AddChildToVerticalBox(MakeLabel(Tree,Name,12))->SetPadding(FMargin(0,18,0,8));
	}
}

void UJTSDebugPlanetButton::Configure(AJTSPlayerController* Controller,FName InPlanetId,bool Default)
{ Player=Controller;PlanetId=InPlanetId;bSetDefault=Default;InitIsFocusable(false);OnClicked.AddUniqueDynamic(this,&ThisClass::Activate); }
void UJTSDebugPlanetButton::Activate()
{ if (Player.IsValid()) Player->ServerRequestDebugPlanet(PlanetId,bSetDefault); }
void UJTSDebugPermitButton::Configure(AJTSPlayerController* Controller,FName InPermitId)
{ Player=Controller;PermitId=InPermitId;InitIsFocusable(false);OnClicked.AddUniqueDynamic(this,&ThisClass::Activate); }
void UJTSDebugPermitButton::Activate()
{ if (Player.IsValid()) Player->ServerRequestDebugAccessPermit(PermitId); }

TSharedRef<SWidget> UJTSSpaceDebugWidget::RebuildWidget()
{
	if (Content) return Super::RebuildWidget();
	auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Root->SetClipping(EWidgetClipping::ClipToBounds);
	Sidebar=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("DebugSidebar"));
	Sidebar->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.009f,.019f,.03f,.97f),12.f,FLinearColor(.13f,.22f,.27f),1.f));
	Sidebar->SetPadding(FMargin(20,18));
	auto* BodySlot=Root->AddChildToCanvas(Sidebar);BodySlot->SetAnchors(FAnchors(0,0,1,1));BodySlot->SetOffsets(FMargin(0,0,36,0));
	auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Sidebar->SetContent(Body);
	auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0,0,0,12));
	Header->AddChildToHorizontalBox(MakeLabel(WidgetTree,NSLOCTEXT("SpaceDebug","SidebarTitle","调试"),20))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto* Close=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("DebugSidebarClose"));
	auto* CloseLabel=MakeLabel(WidgetTree,FText::FromString(TEXT("×")),16);Close->SetContent(CloseLabel);SetButtonState(Close,CloseLabel,false);
	Close->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleSidebar);Header->AddChildToHorizontalBox(Close);
	auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Body->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Content=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Content);
	Body->AddChildToVerticalBox(MakeLabel(WidgetTree,NSLOCTEXT("SpaceDebug","SidebarControls","按住 Alt 操作"),11))->SetPadding(FMargin(0,14,0,0));
	SidebarTab=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("DebugSidebarToggle"));
	SidebarTabLabel=MakeLabel(WidgetTree,FText::FromString(TEXT("调\n试")),12);SidebarTabLabel->SetJustification(ETextJustify::Center);SidebarTab->SetContent(SidebarTabLabel);
	SetButtonState(SidebarTab,SidebarTabLabel,false);SidebarTab->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleSidebar);
	SidebarTab->SetToolTipText(NSLOCTEXT("SpaceDebug","SidebarTabTip","按住 Alt 点击展开"));
	auto* TabSlot=Root->AddChildToCanvas(SidebarTab);TabSlot->SetAnchors(FAnchors(1,.5f));TabSlot->SetPosition(FVector2D(-36,-38));TabSlot->SetSize(FVector2D(36,76));
	Sidebar->SetVisibility(ESlateVisibility::Collapsed);Refresh();return Super::RebuildWidget();
}
void UJTSSpaceDebugWidget::NativeConstruct() { Super::NativeConstruct();Refresh(); }
void UJTSSpaceDebugWidget::NativeTick(const FGeometry& Geometry,float Dt)
{
	Super::NativeTick(Geometry,Dt);
	SidebarAlpha=FMath::FInterpConstantTo(SidebarAlpha,bSidebarOpen?1.f:0.f,Dt,6.f);
	if (Sidebar)
	{
		Sidebar->SetVisibility(SidebarAlpha>0||bSidebarOpen?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
		Sidebar->SetRenderTranslation(FVector2D((1.f-SidebarAlpha)*360.f,0));Sidebar->SetRenderOpacity(SidebarAlpha);
	}
	RefreshElapsed+=Dt;if (RefreshElapsed>=.2f) { RefreshElapsed=0;Refresh(); }
}
void UJTSSpaceDebugWidget::ToggleSidebar()
{
	bSidebarOpen=!bSidebarOpen;
	SidebarTabLabel->SetText(bSidebarOpen?FText::FromString(TEXT("›")):FText::FromString(TEXT("调\n试")));
	Refresh();
}
bool UJTSSpaceDebugWidget::IsPointerOverPanel(const FVector2D& Position) const
{
	return (SidebarTab && SidebarTab->IsVisible() && SidebarTab->GetCachedGeometry().IsUnderLocation(Position))
		|| (Sidebar && Sidebar->IsVisible() && SidebarAlpha>.01f && Sidebar->GetCachedGeometry().IsUnderLocation(Position));
}
void UJTSSpaceDebugWidget::ToggleDarkVisibility()
{ if (auto* Local=GetOwningLocalPlayer()) if (auto* View=Local->GetSubsystem<UJTSDebugViewSubsystem>()) View->SetDarkVisibilityEnabled(!View->IsDarkVisibilityEnabled());Refresh(); }
void UJTSSpaceDebugWidget::ToggleInvulnerability()
{ if (auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->ServerSetDebugInvulnerable(!PC->IsDebugInvulnerable());Refresh(); }
void UJTSSpaceDebugWidget::ToggleOneHitKill()
{ if (auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->ServerSetDebugOneHitKill(!PC->IsDebugOneHitKillEnabled());Refresh(); }

void UJTSSpaceDebugWidget::BuildPanel(const AJTSSpaceDebugTravelManager& Debug)
{
	Content->ClearChildren();Buttons.Reset();PlanetLabels.Reset();PermitButtons.Reset();PermitLabels.Reset();
	Section(WidgetTree,Content,NSLOCTEXT("SpaceDebug","AssistanceSection","辅助"));
	auto* Toggles=WidgetTree->ConstructWidget<UUniformGridPanel>();Toggles->SetSlotPadding(FMargin(3));Content->AddChildToVerticalBox(Toggles);
	const auto AddToggle=[&](FName Name,const FText& Text,int Column,TObjectPtr<UTextBlock>& Label)
	{
		auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);Label=MakeLabel(WidgetTree,Text,13);Label->SetJustification(ETextJustify::Center);Button->SetContent(Label);
		auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetMinDesiredWidth(90);Size->SetHeightOverride(46);Size->SetContent(Button);Toggles->AddChildToUniformGrid(Size,0,Column);return Button;
	};
	DarkVisibilityButton=AddToggle(TEXT("DarkVisibilityToggle"),NSLOCTEXT("SpaceDebug","NightVision","夜视"),0,DarkVisibilityLabel);DarkVisibilityButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleDarkVisibility);
	InvulnerabilityButton=AddToggle(TEXT("PlayerInvulnerabilityToggle"),NSLOCTEXT("SpaceDebug","Invulnerability","无敌"),1,InvulnerabilityLabel);InvulnerabilityButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleInvulnerability);
	OneHitKillButton=AddToggle(TEXT("OneHitKillToggle"),NSLOCTEXT("SpaceDebug","OneHitKill","秒杀"),2,OneHitKillLabel);OneHitKillButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleOneHitKill);
	Section(WidgetTree,Content,NSLOCTEXT("SpaceDebug","PermitSection","许可"));
	auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer());
	for (const auto& Entry:Debug.GetAccessPermits())
	{
		auto* Button=WidgetTree->ConstructWidget<UJTSDebugPermitButton>(UJTSDebugPermitButton::StaticClass(),*FString::Printf(TEXT("DebugPermit_%s"),*Entry.ItemId.ToString()));Button->Configure(PC,Entry.ItemId);
		auto* Label=MakeLabel(WidgetTree,Entry.DisplayName,14);Button->SetContent(Label);Button->SetToolTipText(NSLOCTEXT("SpaceDebug","ProgramPermitTip","获得许可并写入通讯器"));
		auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(44);Size->SetContent(Button);Content->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,3));
		PermitButtons.Add(Button);PermitLabels.Add(Label);
	}
	Section(WidgetTree,Content,NSLOCTEXT("SpaceDebug","PlanetSection","星球"));
	for (const auto& Entry:Debug.GetDestinations())
	{
		auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,3));
		for (int32 Action=0;Action<2;++Action)
		{
			auto* Button=WidgetTree->ConstructWidget<UJTSDebugPlanetButton>();Button->Configure(PC,Entry.PlanetId,Action==1);
			auto* Label=MakeLabel(WidgetTree,Action?FText::FromString(TEXT("★")):Entry.DisplayName,14);Label->SetJustification(ETextJustify::Center);Button->SetContent(Label);
			Button->SetToolTipText(Action?NSLOCTEXT("SpaceDebug","DefaultSidebarTip","设为下次默认星球"):NSLOCTEXT("SpaceDebug","TravelSidebarTip","全队与飞船前往"));
			auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(44);if (Action) Size->SetWidthOverride(44);Size->SetContent(Button);
			auto* RowSlot=Row->AddChildToHorizontalBox(Size);RowSlot->SetPadding(FMargin(Action?6:0,0,0,0));if (!Action) RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Buttons.Add(Button);PlanetLabels.Add(Label);
		}
	}
	StatusText=MakeLabel(WidgetTree,FText::GetEmpty(),11);StatusText->SetAutoWrapText(true);Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0,14,0,0));
}
void UJTSSpaceDebugWidget::Refresh()
{
	if (!Manager.IsValid()) Manager=AJTSSpaceDebugTravelManager::Find(this);
	const auto* Debug=Manager.Get();const bool Available=Debug&&Debug->IsAvailable();
	auto* Local=GetOwningLocalPlayer();auto* View=Local?Local->GetSubsystem<UJTSDebugViewSubsystem>():nullptr;
	if (!Available && View) View->SetDarkVisibilityEnabled(false);
	SetRenderOpacity(Available?1.f:0.f);if (SidebarTab) SidebarTab->SetVisibility(Available?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	if (!Available) { bSidebarOpen=false;SidebarAlpha=0;if (Sidebar) Sidebar->SetVisibility(ESlateVisibility::Collapsed);return; }
	if (!Content) return;
	if (Buttons.Num()!=Debug->GetDestinations().Num()*2 || PermitButtons.Num()!=Debug->GetAccessPermits().Num() || !StatusText) BuildPanel(*Debug);
	auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer());const auto* State=GetWorld()->GetGameState<AJTSGameState>();
	DarkVisibilityButton->SetIsEnabled(View&&View->CanUseDarkVisibility());SetButtonState(DarkVisibilityButton,DarkVisibilityLabel,View&&View->IsDarkVisibilityEnabled());
	InvulnerabilityButton->SetIsEnabled(PC!=nullptr);SetButtonState(InvulnerabilityButton,InvulnerabilityLabel,PC&&PC->IsDebugInvulnerable());
	OneHitKillButton->SetIsEnabled(PC!=nullptr);SetButtonState(OneHitKillButton,OneHitKillLabel,PC&&PC->IsDebugOneHitKillEnabled());
	for (int32 I=0;I<PermitButtons.Num();++I)
	{
		const auto& Entry=Debug->GetAccessPermits()[I];PermitButtons[I]->SetIsEnabled(Debug->CanUse(PC));SetButtonState(PermitButtons[I],PermitLabels[I],State&&State->HasImportantItem(Entry.ItemId));
	}
	for (int32 I=0;I<Buttons.Num();++I)
	{
		Buttons[I]->SetIsEnabled(Debug->CanUse(PC));const FName Id=Debug->GetDestinations()[I/2].PlanetId;
		SetButtonState(Buttons[I],PlanetLabels[I],I%2?Debug->GetDefaultPlanetId()==Id:State&&FName(*State->GetCurrentPlanetId())==Id);
	}
	StatusText->SetText(Debug->GetStatus());StatusText->SetVisibility(Debug->GetStatus().IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
}
