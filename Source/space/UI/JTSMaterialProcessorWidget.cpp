#include "space/UI/JTSMaterialProcessorWidget.h"
#include "space/UI/SJTSMaterialGraph.h"
#include "space/Components/JTSMaterialProcessorComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Player/JTSPlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UJTSMaterialProcessorWidget::SetContext(AJTSSpacecraftActor* Ship, AJTSPlayerController* Player)
{
	const bool bChanged = Spacecraft.Get() != Ship;
	Spacecraft = Ship; Controller = Player;
	if (GraphCanvas && bChanged) GraphCanvas->Initialize(Ship ? Ship->GetMaterialProcessor() : nullptr);
	RefreshPalette();
}

void UJTSMaterialProcessorWidget::NotifyResult(bool bSucceeded, const FString& Message)
{
	bPending = false; bLastSucceeded = bSucceeded; ResultMessage = Message;
}

FString UJTSMaterialProcessorWidget::StockText() const
{
	if (!Spacecraft.IsValid()) return TEXT("飞船仓库不可用");
	TArray<FString> Parts;
	for (const auto Type : { EJTSResourceType::MoonAntCorpse, EJTSResourceType::Water, EJTSResourceType::InorganicCarbon, EJTSResourceType::Food, EJTSResourceType::Fuel })
		Parts.Add(JTSMaterialProcessing::ResourceLabel(Type) + TEXT(" ") + JTSMaterialProcessing::FormatHalfUnits(Spacecraft->GetResourceHalfUnits(Type)));
	return TEXT("共享仓库   ") + FString::Join(Parts, TEXT("     "));
}

FString UJTSMaterialProcessorWidget::PreviewText() const
{
	const auto* Processor = Spacecraft.IsValid() ? Spacecraft->GetMaterialProcessor() : nullptr;
	if (!Processor || !GraphCanvas) return TEXT("处理程序不可用");
	TMap<EJTSResourceType, int64> Deltas;
	FString Error;
	if (!Processor->Preview(GraphCanvas->GetGraph(), Deltas, Error)) return Error;
	TArray<FString> Costs, Products;
	for (uint8 I = 0; I <= uint8(EJTSResourceType::InorganicCarbon); ++I)
	{
		const auto Type = EJTSResourceType(I);
		const int64 Delta = Deltas.FindRef(Type);
		if (!Delta) continue;
		(Delta < 0 ? Costs : Products).Add(JTSMaterialProcessing::FormatHalfUnits(FMath::Abs(Delta)) + TEXT(" ") + JTSMaterialProcessing::ResourceLabel(Type));
	}
	return TEXT("整条流程净消耗：") + (Costs.IsEmpty() ? TEXT("无") : FString::Join(Costs, TEXT(" + ")))
		+ TEXT("\n整条流程净产出：") + (Products.IsEmpty() ? TEXT("无") : FString::Join(Products, TEXT(" + ")));
}

bool UJTSMaterialProcessorWidget::CanExecute() const
{
	if (bPending || !Controller.IsValid() || !Spacecraft.IsValid() || !GraphCanvas) return false;
	const auto* Processor = Spacecraft->GetMaterialProcessor();
	TMap<EJTSResourceType, int64> Deltas; FString Error;
	return Processor && Processor->Preview(GraphCanvas->GetGraph(), Deltas, Error);
}

FReply UJTSMaterialProcessorWidget::Submit(bool bExecute)
{
	if (!bPending && Controller.IsValid() && Spacecraft.IsValid() && GraphCanvas)
	{
		bPending = true; ResultMessage = TEXT("等待服务器处理…");
		Controller->ServerRequestMaterialProcessing(Spacecraft.Get(), GraphCanvas->GetGraph(), bExecute);
	}
	return FReply::Handled();
}

void UJTSMaterialProcessorWidget::RefreshPalette()
{
	if (!Palette) return;
	Palette->ClearChildren();
	const auto* Processor = Spacecraft.IsValid() ? Spacecraft->GetMaterialProcessor() : nullptr;
	if (!Processor || !Processor->RecipeBook) return;
	for (const auto& Recipe : Processor->RecipeBook->Recipes)
	{
		const FName Id = Recipe.Id;
		Palette->AddSlot().AutoHeight().Padding(0, 0, 0, 9)
		[
			SNew(SButton).ContentPadding(FMargin(12, 10))
			.ToolTipText(FText::FromString(JTSMaterialProcessing::FormatQuantities(Recipe.Inputs) + TEXT(" → ") + JTSMaterialProcessing::FormatQuantities(Recipe.Outputs)))
			.OnClicked_Lambda([this, Id]() { if (GraphCanvas) { GraphCanvas->AddProgram(Id); GraphCanvas->FrameGraph(); ResultMessage.Reset(); } return FReply::Handled(); })
			[ SNew(STextBlock).Text(Recipe.DisplayName).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)) ]
		];
	}
}

TSharedRef<SWidget> UJTSMaterialProcessorWidget::RebuildWidget()
{
	SAssignNew(GraphCanvas, SJTSMaterialGraph);
	GraphCanvas->Initialize(Spacecraft.IsValid() ? Spacecraft->GetMaterialProcessor() : nullptr);
	const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 13);
	TSharedRef<SWidget> Result = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().Padding(12, 8, 12, 10)
	[ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(StockText()); }).Font(FCoreStyle::GetDefaultFontStyle("Regular", 16)).ColorAndOpacity(FLinearColor(.55f, .85f, .86f)) ]
	+ SVerticalBox::Slot().FillHeight(1)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(SBorder).Padding(12).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.02f, .04f, .058f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
				[ SNew(STextBlock).Text(FText::FromString(TEXT("添加处理程序"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 17)) ]
				+ SVerticalBox::Slot().FillHeight(1)
				[ SNew(SScrollBox) + SScrollBox::Slot() [ SAssignNew(Palette, SVerticalBox) ] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
				[ SNew(STextBlock).Text(FText::FromString(TEXT("拖动节点 · 拖动端口连线\n右键端口断线 · Delete 删除\n中键移动 · 滚轮缩放 · F 全览\n批数 +/−，Shift 每次调整 10"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(FLinearColor(.5f, .64f, .7f)) ]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[ SNew(SButton).Text(FText::FromString(TEXT("新建流程"))).OnClicked_Lambda([this]() { GraphCanvas->ResetGraph(); ResultMessage.Reset(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[ SNew(SButton).Text(FText::FromString(TEXT("删除选中"))).OnClicked_Lambda([this]() { GraphCanvas->DeleteSelected(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth()
				[ SNew(SButton).Text(FText::FromString(TEXT("全览"))).OnClicked_Lambda([this]() { GraphCanvas->FrameGraph(); return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[ SNew(STextBlock).Text(FText::FromString(TEXT("仓库输入 → 处理程序 → 仓库输出"))).Font(Font).ColorAndOpacity(FLinearColor(.55f, .7f, .75f)) ]
			]
			+ SVerticalBox::Slot().FillHeight(1) [ GraphCanvas.ToSharedRef() ]
			+ SVerticalBox::Slot().AutoHeight().Padding(12, 9)
			[ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(PreviewText()); }).Font(Font).ColorAndOpacity(FLinearColor(.65f, .9f, .75f)).AutoWrapText(true) ]
		]
	]
	+ SVerticalBox::Slot().AutoHeight().Padding(12, 8, 12, 6)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 12, 0)
		[ SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(ResultMessage); }).Font(Font).AutoWrapText(true)
			.ColorAndOpacity_Lambda([this]() { return bLastSucceeded ? FLinearColor(.48f, .86f, .7f) : FLinearColor(.98f, .5f, .35f); }) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
		[ SNew(SButton).Text(FText::FromString(TEXT("保存流程"))).ContentPadding(FMargin(18, 10)).IsEnabled_Lambda([this]() { return !bPending; }).OnClicked_Lambda([this]() { return Submit(false); }) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).Text(FText::FromString(TEXT("执行一次"))).ContentPadding(FMargin(30, 10)).ButtonColorAndOpacity(FLinearColor(.09f, .4f, .36f))
			.IsEnabled_Lambda([this]() { return CanExecute(); }).OnClicked_Lambda([this]() { return Submit(true); }) ]
	];
	RefreshPalette();
	return Result;
}

void UJTSMaterialProcessorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	GraphCanvas.Reset(); Palette.Reset();
}
