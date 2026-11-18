#include "space/UI/JTSFieldGuideWidget.h"
#include "space/Components/JTSShipScannerComponent.h"
#include "space/Components/JTSDiscoveryComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Core/JTSGameState.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
const FLinearColor Teal(.17f,.87f,.8f),Muted(.42f,.57f,.62f),White(.89f,.95f,.95f),Amber(1,.65f,.24f),BG(.008f,.021f,.03f,.98f);
struct FGuidePainter
{
	const FGeometry& G; FSlateWindowElementList& Out; int32 Layer; float Scale; FVector2D Origin;
	void Box(float X,float Y,float W,float H,FLinearColor C) const
	{ FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H)*Scale,FSlateLayoutTransform(Origin+FVector2D(X,Y)*Scale)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C); }
	void Text(float X,float Y,const FString& S,int32 Size,FLinearColor C) const
	{ FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(950,45)*Scale,FSlateLayoutTransform(Origin+FVector2D(X,Y)*Scale)),S,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(8,FMath::RoundToInt(Size*Scale))),ESlateDrawEffect::None,C); }
	void Lines(float X,float Y,const FString& S,int32 Width,int32 Size,FLinearColor C,int32 MaxLines=4) const
	{
		TArray<FString> Paragraphs; S.ParseIntoArrayLines(Paragraphs,false); int32 Row=0;
		for(const FString& Paragraph:Paragraphs) for(int32 I=0;I<Paragraph.Len() && Row<MaxLines;I+=Width,++Row) Text(X,Y+Row*(Size+10),Paragraph.Mid(I,Width),Size,C);
	}
};
FGuidePainter Paint(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer)
{
	const float S=FMath::Min(G.GetLocalSize().X/1000.f,G.GetLocalSize().Y/660.f);
	return {G,Out,Layer,S,(G.GetLocalSize()-FVector2D(1000,660)*S)*.5f};
}
}
int32 UJTSScanStatusWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
	Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);
	const auto* S=Scanner.Get(); if(!S || !S->IsZoneActive()) return Layer;
	const auto* PC=GetOwningPlayer();
	if(PC && PC->GetPawn() && FVector::DistSquared(PC->GetPawn()->GetActorLocation(),S->GetGroundFrame().GetLocation())>FMath::Square(1800.f)) return Layer;
	FGuidePainter P{G,Out,Layer,1,FVector2D::ZeroVector}; const auto& Scan=S->GetScan();
	P.Box(0,0,340,94,BG); P.Box(0,0,3,94,Teal);
	const bool Has=!Scan.Sample.IsEmpty();
	P.Text(16,9,Has?UJTSDiscoveryComponent::DisplayName(S,Scan.Sample).ToString():TEXT("飞船扫描区"),19,White);
	const FString Message=Scan.bStorageBlocked?TEXT("收纳已满 · 物品保留在地面"):Scan.bComplete?TEXT("已识别并入库 · [J] 查看图鉴"):Has?FString::Printf(TEXT("正在扫描   %d%%"),FMath::RoundToInt(S->GetScanProgress()*100)):TEXT("将物品放进光圈 · 自动扫描入库");
	P.Text(16,40,Message,14,Scan.bStorageBlocked?Amber:Muted);
	P.Box(16,73,308,3,FLinearColor(.04f,.12f,.15f)); P.Box(16,73,308*S->GetScanProgress(),3,Scan.bStorageBlocked?Amber:Teal);
	return Layer+2;
}
void UJTSFieldGuideWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized(); SetIsFocusable(true); SetVisibility(ESlateVisibility::Visible); ForceVolatile(true);
}
UJTSAnalysisCatalog* UJTSFieldGuideWidget::GetCatalog() const
{ const auto* D=UJTSDiscoveryComponent::Find(this); return D?D->GetCatalog():nullptr; }
AJTSSpacecraftActor* UJTSFieldGuideWidget::GetShip() const
{
	const auto* State=GetWorld()?GetWorld()->GetGameState<AJTSGameState>():nullptr;
	return State?State->GetActiveSpacecraft():nullptr;
}
TArray<int32> UJTSFieldGuideWidget::VisibleEntries() const
{
	TArray<int32> Rows; const auto* Cat=GetCatalog(); if(!Cat) return Rows;
	for(int32 I=0;I<Cat->Entries.Num();++I)
	{
		const auto& E=Cat->Entries[I]; const bool Creature=E.ItemId==EJTSItemId::MoonAntCorpse;
		EJTSResourceType Type; const bool Resource=UJTSItemDefinitionLibrary::TryGetResourceType(E.ItemId,Type);
		if(Filter==0 || (Filter==1 && Resource && !Creature) || (Filter==2 && Creature) || (Filter==3 && !Resource)) Rows.Add(I);
	}
	return Rows;
}
FVector2D UJTSFieldGuideWidget::DesignPoint(const FGeometry& G,FVector2D Screen) const
{
	const float S=FMath::Max(.01f,FMath::Min(G.GetLocalSize().X/1000.f,G.GetLocalSize().Y/660.f));
	return (G.AbsoluteToLocal(Screen)-(G.GetLocalSize()-FVector2D(1000,660)*S)*.5f)/S;
}
int32 UJTSFieldGuideWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
	Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);
	FGuidePainter Dim{G,Out,Layer,1,FVector2D::ZeroVector}; Dim.Box(0,0,G.GetLocalSize().X,G.GetLocalSize().Y,FLinearColor(0,0,0,.55f));
	auto P=Paint(G,Out,Layer+1); P.Box(20,20,960,620,BG); P.Box(20,20,960,3,Teal);
	P.Text(48,40,TEXT("EXPEDITION / DISCOVERY"),12,Teal); P.Text(48,64,bCargo?TEXT("飞船收纳"):TEXT("远征图鉴"),30,White);
	P.Text(780,49,TEXT("[J / Esc] 返回"),15,Muted);
	P.Box(48,120,174,36,bCargo?FLinearColor(.02f,.08f,.1f):FLinearColor(.03f,.2f,.2f)); P.Text(78,128,TEXT("发现档案"),16,Teal);
	P.Box(232,120,174,36,bCargo?FLinearColor(.03f,.2f,.2f):FLinearColor(.02f,.08f,.1f)); P.Text(262,128,TEXT("飞船收纳"),16,Teal);
	const auto* D=UJTSDiscoveryComponent::Find(this); const auto* Cat=GetCatalog();
	if(bCargo)
	{
		P.Text(48,177,TEXT("素材直接进入飞船资源库存；装备保存在这里。"),16,Muted);
		const auto* Ship=GetShip(); const auto* Scanner=Ship?Ship->Scanner.Get():nullptr;
		if(Scanner)
		{
			const auto& Items=Scanner->GetCargo(); int32 Used=0; for(const auto& I:Items) if(!I.IsEmpty()) ++Used;
			P.Text(720,177,FString::Printf(TEXT("%d / %d 格"),Used,Scanner->CargoCapacity),16,Teal);
			for(int32 Row=0;Row<7 && FirstRow+Row<Items.Num();++Row)
			{
				const int32 I=FirstRow+Row; const auto& Item=Items[I]; const float Y=218+Row*48;
				P.Box(48,Y,904,40,FLinearColor(.02f,.065f,.08f));
				P.Text(62,Y+9,Item.IsEmpty()?TEXT("—"):UJTSDiscoveryComponent::DisplayName(this,Item).ToString()+(Item.IsStellar()?FString():FString::Printf(TEXT(" ×%d"),Item.StackCount)),16,Item.IsEmpty()?Muted:White);
				if(!Item.IsEmpty() && Item.IsStellar()) P.Text(778,Y+1,FJTSItemInstance::StackBadge(Item.StackCount),17,Teal);
				if(!Item.IsEmpty()) { P.Box(830,Y+4,108,32,FLinearColor(.03f,.18f,.18f)); P.Text(864,Y+10,TEXT("领取"),15,Teal); }
			}
			if(!Used) P.Text(48,250,TEXT("这里还很宽敞。"),24,Muted);
		}
		P.Text(48,570,TEXT("靠近中央舱屏幕领取 · 全队共享 · 滚轮翻页"),14,Muted);
	}
	else if(Cat)
	{
		const TCHAR* Filters[]={TEXT("全部"),TEXT("素材"),TEXT("生物"),TEXT("装备")};
		for(int32 I=0;I<4;++I) { P.Box(48+I*73,174,66,30,Filter==I?FLinearColor(.035f,.16f,.18f):FLinearColor(.015f,.05f,.07f)); P.Text(62+I*73,180,Filters[I],14,Filter==I?Teal:Muted); }
		const auto Rows=VisibleEntries();
		for(int32 Row=0;Row<8 && FirstRow+Row<Rows.Num();++Row)
		{
			const int32 I=Rows[FirstRow+Row]; const auto& E=Cat->Entries[I]; const bool Known=UJTSDiscoveryComponent::IsKnown(this,E.ItemId,E.SpecimenId);
			P.Box(48,218+Row*43,292,37,Selected==I?FLinearColor(.03f,.19f,.19f):FLinearColor(.015f,.05f,.065f));
			P.Text(62,227+Row*43,Known?E.Name.ToString():TEXT("???"),17,Known?White:Muted);
			if(Known) P.Text(310,227+Row*43,TEXT("·"),16,Teal);
		}
		P.Text(48,580,FString::Printf(TEXT("%d 个档案 · 滚轮浏览"),Rows.Num()),13,Muted);
		P.Box(366,174,1,410,FLinearColor(.04f,.13f,.15f));
		const int32 Index=Selected!=INDEX_NONE?Selected:Rows.IsEmpty()?INDEX_NONE:Rows[0];
		if(Cat->Entries.IsValidIndex(Index))
		{
			const auto& E=Cat->Entries[Index]; const bool Known=UJTSDiscoveryComponent::IsKnown(this,E.ItemId,E.SpecimenId);
			P.Text(402,185,Known?TEXT("IDENTIFIED / 已识别"):TEXT("UNIDENTIFIED / 未知样本"),13,Known?Teal:Amber);
			P.Text(402,224,Known?E.Name.ToString():TEXT("???"),34,White);
			if(Known)
			{
				FString Sources; if(D) if(const auto* R=D->GetRecord(E.ItemId,E.SpecimenId)) for(FName Id:R->Sources)
				{ if(!Sources.IsEmpty()) Sources+=TEXT(" · "); const auto* Name=Cat->PlanetNames.Find(Id); Sources+=Name?Name->ToString():Id.ToString(); }
				P.Text(402,285,TEXT("来源"),13,Muted); P.Lines(402,309,Sources.IsEmpty()?TEXT("尚无采集地点记录"):Sources,27,18,White,2);
				for(int32 I=0;I<E.Tags.Num() && I<6;++I)
				{
					const float X=402+(I%3)*174,Y=390+(I/3)*40;
					P.Box(X,Y,162,32,FLinearColor(.028f,.13f,.15f)); P.Text(X+12,Y+6,TEXT("[")+E.Tags[I].Label.ToString()+TEXT("]"),16,Teal);
				}
				P.Lines(402,488,E.Description.ToString(),26,18,White,3);
				P.Text(402,580,TEXT("悬停标签查看用途 · 来源随实际采集更新"),12,Muted);
			}
			else { P.Lines(402,310,TEXT("把物品放入中央舱前的光圈。\n飞船会自动扫描并收进库存。"),25,19,Muted); P.Lines(402,448,TEXT("识别一次，就能认出大小不同的同类物品。\n生物需要带回尸体或尸块。"),27,16,Muted); }
		}
	}
	else P.Text(48,220,TEXT("正在同步远征档案…"),20,Muted);
	P.Text(48,613,Feedback.IsEmpty()?TEXT("扫描成果全队共享，每位队员均可随时查阅。"):Feedback.ToString().Left(52),13,Feedback.IsEmpty()?Muted:Amber);
	return Layer+3;
}
FReply UJTSFieldGuideWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& Event)
{
	if(Event.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
	const auto P=DesignPoint(G,Event.GetScreenSpacePosition());
	if(P.X>=770 && P.Y>=35 && P.Y<=90) { if(auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->CloseSpaceShop(); return FReply::Handled(); }
	if(P.Y>=120 && P.Y<=156 && P.X>=48 && P.X<=406) { bCargo=P.X>=232; FirstRow=0; Selected=INDEX_NONE; Feedback=FText::GetEmpty(); return FReply::Handled(); }
	if(bCargo)
	{
		const int32 Row=FMath::FloorToInt((P.Y-218)/48); auto* Ship=GetShip(); const auto* S=Ship?Ship->Scanner.Get():nullptr;
		if(S && P.X>=830 && P.X<=938 && Row>=0 && Row<7 && S->GetCargo().IsValidIndex(FirstRow+Row))
			if(auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->ServerRetrieveScannerCargo(Ship,FirstRow+Row,S->GetCargo()[FirstRow+Row].InstanceId);
	}
	else
	{
		if(P.Y>=174 && P.Y<=204 && P.X>=48 && P.X<340) { Filter=FMath::Clamp(FMath::FloorToInt((P.X-48)/73),0,3); FirstRow=0; Selected=INDEX_NONE; return FReply::Handled(); }
		const auto Rows=VisibleEntries(); const int32 Row=FMath::FloorToInt((P.Y-218)/43);
		if(P.X>=48 && P.X<=340 && Row>=0 && Row<8 && Rows.IsValidIndex(FirstRow+Row)) Selected=Rows[FirstRow+Row];
	}
	return FReply::Handled();
}
FReply UJTSFieldGuideWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& Event)
{
	FText Tip; const auto P=DesignPoint(G,Event.GetScreenSpacePosition()); const auto* Cat=GetCatalog();
	const auto Rows=VisibleEntries(); const int32 Index=Selected!=INDEX_NONE?Selected:Rows.IsEmpty()?INDEX_NONE:Rows[0];
	if(!bCargo && Cat && Cat->Entries.IsValidIndex(Index))
	{
		const auto& E=Cat->Entries[Index];
		const int32 Col=FMath::FloorToInt((P.X-402)/174),Row=FMath::FloorToInt((P.Y-390)/40),Tag=Row*3+Col;
		if(UJTSDiscoveryComponent::IsKnown(this,E.ItemId,E.SpecimenId) && Col>=0 && Col<3 && Row>=0 && Row<2
			&& FMath::Fmod(P.X-402,174.f)<=162 && FMath::Fmod(P.Y-390,40.f)<=32 && E.Tags.IsValidIndex(Tag)) Tip=E.Tags[Tag].Explanation;
	}
	SetToolTipText(Tip); return FReply::Handled();
}
void UJTSFieldGuideWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{ SetToolTipText(FText::GetEmpty()); Super::NativeOnMouseLeave(Event); }
FReply UJTSFieldGuideWidget::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& Event)
{
	const auto* Ship=GetShip(); const int32 Count=bCargo?(Ship?Ship->Scanner->GetCargo().Num():0):VisibleEntries().Num();
	FirstRow=FMath::Clamp(FirstRow-(Event.GetWheelDelta()>0?2:-2),0,FMath::Max(0,Count-(bCargo?7:8))); return FReply::Handled();
}
FReply UJTSFieldGuideWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& Event)
{
	if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::J)
	{ if(auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->CloseSpaceShop(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(G,Event);
}

void UJTSScanStatusWidget::SetScanner(UJTSShipScannerComponent* Value) { Scanner=Value; ForceVolatile(true); }
