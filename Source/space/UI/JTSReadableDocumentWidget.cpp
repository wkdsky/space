#include "space/UI/JTSReadableDocumentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "GameFramework/PlayerController.h"
#include "space/Items/JTSReadableDocument.h"
#include "space/Player/JTSPlayerController.h"
void UJTSReadableDocumentWidget::ShowDocument(UJTSReadableDocumentData* D)
{
    if (!D) return;
    SetIsFocusable(true);
    auto* Background=WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(.025f,.03f,.035f,.98f)); Background->SetPadding(FMargin(140,80)); WidgetTree->RootWidget=Background;
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Box);
    auto AddText=[&](const FText& T,int32 Size){ auto* W=WidgetTree->ConstructWidget<UTextBlock>(); W->SetText(T); auto Font=W->GetFont(); Font.Size=Size; W->SetFont(Font); W->SetAutoWrapText(true); return W; };
    Box->AddChildToVerticalBox(AddText(D->Title,28));
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Box->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Scroll->AddChild(AddText(D->Body,20));
    if (D->Image) { auto* Pic=WidgetTree->ConstructWidget<UImage>(); Pic->SetBrushFromTexture(D->Image,true); Scroll->AddChild(Pic); }
    auto* Button=WidgetTree->ConstructWidget<UButton>(); Button->AddChild(AddText(NSLOCTEXT("JTS","CloseDocument","关闭 [Esc]"),20)); Button->OnClicked.AddDynamic(this,&ThisClass::Close); Box->AddChildToVerticalBox(Button);
}
void UJTSReadableDocumentWidget::Close()
{
    if (auto* PC=Cast<AJTSPlayerController>(GetOwningPlayer())) PC->CloseReadableDocument();
}
FReply UJTSReadableDocumentWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E) { if (E.GetKey()==EKeys::Escape) { Close(); return FReply::Handled(); } return Super::NativeOnKeyDown(G,E); }
