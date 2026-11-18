#include "space/UI/JTSStellarExchangeWidget.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSStellarExchangeComponent.h"
#include "space/Items/JTSStellarLootTable.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

int32 UJTSStellarExchangeWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,
 FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const
{
 Super::NativePaint(Args,G,Clip,Out,Layer,Style,bEnabled);
 const float Height=bInteractive?520.f:148.f;
 const FVector2D Scale=G.GetLocalSize()/FVector2D(620,Height);
 const FLinearColor Ink(.018f,.008f,.045f,1),Pink(1,.08f,.43f,1),Gold(1,.72f,.12f,1),Cyan(.15f,.95f,1,1),White(1,.96f,.84f,1);
 auto Box=[&](float X,float Y,float W,float H,FLinearColor C)
 {
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(W,H)*Scale,FSlateLayoutTransform(FVector2D(X,Y)*Scale)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);
 };
 auto Text=[&](float X,float Y,const FString& S,int32 Size,FLinearColor C)
 {
  FSlateDrawElement::MakeText(Out,Layer+2,G.ToPaintGeometry(FVector2D(610,40)*Scale,FSlateLayoutTransform(FVector2D(X,Y)*Scale)),S,FCoreStyle::GetDefaultFontStyle("Bold",FMath::Max(9,FMath::RoundToInt(Size*Scale.Y))),ESlateDrawEffect::None,C);
 };
 const auto* E=Ship.IsValid()?Ship->GetStellarExchange():nullptr;
 const auto* T=Ship.IsValid()?Ship->GetStellarLootTable():nullptr;
 const bool Rolling=E&&E->IsRolling();
 auto Label=[T](FName Id) { const auto* Entry=T?T->FindEntry(Id):nullptr;return Entry?Entry->DisplayName.ToString():FString(TEXT("ALPHA")); };
 Box(0,0,620,Height,Ink);Box(0,0,620,3,Pink);Box(0,Height-3,620,3,Gold);
 // The complete event name occupies one fixed line at the top left of the physical screen.
 Text(14,8,TEXT("阿尔法快乐拉杆（活动专用）"),20,Gold);
 const float Time=GetWorld()?GetWorld()->GetTimeSeconds():0;
 for(int32 I=0;I<30;++I)
 {
  const bool Lit=(I+FMath::FloorToInt(Time*5))%4<2;
  Box(12+I*20,36,9,3,Lit?Pink:Cyan*.22f);
 }
 const float ReelY=bInteractive?116:48,ReelH=bInteractive?152:69;
 for(int32 C=0;C<3;++C)
 {
  const float X=14+C*199;
  Box(X-2,ReelY-2,191,ReelH+4,C==1?Gold:Pink);
  Box(X,ReelY,187,ReelH,FLinearColor(.055f,.025f,.10f,1));
  Box(X+2,ReelY+ReelH*.53f,183,1,Cyan*.45f);
  FName Id=E?E->GetState().LastResult:NAME_None;
  if(Rolling&&T&&!T->Entries.IsEmpty()) Id=T->Entries[E->GetReelIndex(C,T->Entries.Num())].ItemId;
  const int32 FontSize=bInteractive?20:17;
  Text(X+9,ReelY+ReelH*.33f,Label(Id).Left(9),FontSize,White);
  if(Rolling)
  {
   const float Phase=FMath::Frac(E->GetRollElapsed()*(12.f-C*1.3f));
   Box(X+7,ReelY+4+Phase*(ReelH-8),173,2,Gold.CopyWithNewOpacity(.35f));
  }
 }
 Text(14,bInteractive?289:124,Rolling?TEXT("★  GOOD LUCK  ★"):TEXT("PULL TO PLAY   /   拉动右侧拉杆"),bInteractive?19:12,Rolling?Pink:Cyan);
 if(bInteractive)
 {
  Text(14,60,TEXT("ALPHA  /  PULL · SPIN · WIN"),17,Pink);
  Text(14,330,TEXT("奖励自动放入你的星际核心／配件格"),20,White);
  Text(14,365,TEXT("同类优先堆叠 · 满格时从出奖口弹到地面"),17,Cyan);
  FString Cost=TEXT("每次消耗");
  if(T)for(const auto& C:T->SpinCosts)if(C.Amount>0)Cost+=FString::Printf(TEXT("  %s %d"),*UEnum::GetDisplayValueAsText(C.ResourceType).ToString(),C.Amount);
  Text(14,427,Cost,16,Gold);
  Text(14,474,TEXT("★  HAVE A HAPPY FLIGHT  ★"),17,Pink);
 }
 return Layer+3;
}
FReply UJTSStellarExchangeWidget::NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&)
{
 return FReply::Unhandled();
}
