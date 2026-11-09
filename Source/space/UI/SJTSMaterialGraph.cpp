#include "space/UI/SJTSMaterialGraph.h"
#include "space/Components/JTSMaterialProcessorComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
	const FVector2D NodeSize(224, 148);
	const FLinearColor InputColor(.22f, .65f, .88f);
	const FLinearColor ProgramColor(.83f, .61f, .25f);
	const FLinearColor OutputColor(.26f, .77f, .56f);
	FLinearColor ColorFor(EJTSMaterialNodeKind Kind)
	{
		return Kind == EJTSMaterialNodeKind::Input ? InputColor : Kind == EJTSMaterialNodeKind::Output ? OutputColor : ProgramColor;
	}
	bool Inside(FVector2D P, FVector2D Origin, FVector2D Size)
	{ return P.X >= Origin.X && P.Y >= Origin.Y && P.X <= Origin.X + Size.X && P.Y <= Origin.Y + Size.Y; }
}

void SJTSMaterialGraph::Construct(const FArguments&)
{
	SetClipping(EWidgetClipping::ClipToBounds);
}

void SJTSMaterialGraph::Initialize(UJTSMaterialProcessorComponent* InProcessor)
{
	Processor = InProcessor;
	if (InProcessor && !InProcessor->GetSavedGraph().Nodes.IsEmpty()) Graph = InProcessor->GetSavedGraph();
	else
	{
		ResetGraph();
		if (InProcessor && InProcessor->RecipeBook && !InProcessor->RecipeBook->Recipes.IsEmpty())
			AddProgram(InProcessor->RecipeBook->Recipes[0].Id);
	}
	FrameGraph();
}

FJTSMaterialNode* SJTSMaterialGraph::FindNode(int32 Id)
{ return Graph.Nodes.FindByPredicate([Id](const auto& Node) { return Node.Id == Id; }); }
const FJTSMaterialNode* SJTSMaterialGraph::FindNode(int32 Id) const
{ return Graph.Nodes.FindByPredicate([Id](const auto& Node) { return Node.Id == Id; }); }

void SJTSMaterialGraph::ResetGraph()
{
	Graph = {};
	FJTSMaterialNode Input; Input.Id = 0; Input.Kind = EJTSMaterialNodeKind::Input; Input.Position = FVector2D(30, 85);
	FJTSMaterialNode Output; Output.Id = 1; Output.Kind = EJTSMaterialNodeKind::Output; Output.Position = FVector2D(650, 85);
	Graph.Nodes = { Input, Output };
	FJTSMaterialLink Link; Link.From = 0; Link.To = 1; Graph.Links.Add(Link);
	Selected = Dragged = WiringFrom = INDEX_NONE; bPanning = false; Pan = FVector2D::ZeroVector; Zoom = 1;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SJTSMaterialGraph::AddProgram(FName Recipe)
{
	if (Graph.Nodes.Num() >= 32) return;
	int32 NewId = 0;
	for (const auto& Node : Graph.Nodes) NewId = FMath::Max(NewId, Node.Id + 1);
	FJTSMaterialNode* Output = Graph.Nodes.FindByPredicate([](const auto& N) { return N.Kind == EJTSMaterialNodeKind::Output; });
	if (!Output) return;
	const int32 OutputId = Output->Id;
	FJTSMaterialNode New; New.Id = NewId; New.RecipeId = Recipe;
	New.Position = Graph.Nodes.Num() == 2 ? FVector2D(340, 85) : Output->Position;
	if (Graph.Nodes.Num() > 2) Output->Position.X += 310;
	bool bInserted = false;
	for (auto& Link : Graph.Links)
		if (Link.To == OutputId) { Link.To = NewId; bInserted = true; break; }
	Graph.Nodes.Add(New);
	if (bInserted) { FJTSMaterialLink Link; Link.From = NewId; Link.To = OutputId; Graph.Links.Add(Link); }
	Selected = NewId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SJTSMaterialGraph::Disconnect(int32 Id, bool bOutput)
{
	Graph.Links.RemoveAll([Id, bOutput](const auto& Link) { return bOutput ? Link.From == Id : Link.To == Id; });
}

void SJTSMaterialGraph::DeleteSelected()
{
	const auto* Node = FindNode(Selected);
	if (!Node || Node->Kind != EJTSMaterialNodeKind::Program) return;
	Graph.Links.RemoveAll([this](const auto& Link) { return Link.From == Selected || Link.To == Selected; });
	Graph.Nodes.RemoveAll([this](const auto& N) { return N.Id == Selected; });
	Selected = Dragged = WiringFrom = INDEX_NONE;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SJTSMaterialGraph::FrameGraph()
{
	if (Graph.Nodes.IsEmpty()) return;
	FVector2D Min(32768, 32768), Max(-32768, -32768);
	for (const auto& Node : Graph.Nodes)
	{
		Min.X = FMath::Min(Min.X, Node.Position.X); Min.Y = FMath::Min(Min.Y, Node.Position.Y);
		Max.X = FMath::Max(Max.X, Node.Position.X + NodeSize.X); Max.Y = FMath::Max(Max.Y, Node.Position.Y + NodeSize.Y);
	}
	FVector2D View = GetCachedGeometry().GetLocalSize();
	if (View.X < 100 || View.Y < 100) View = FVector2D(990, 330);
	Zoom = FMath::Clamp(float(FMath::Min((View.X - 60) / (Max.X - Min.X), (View.Y - 70) / (Max.Y - Min.Y))), .25f, 1.f);
	Pan = (View - (Max - Min) * Zoom) * .5 - Min * Zoom;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SJTSMaterialGraph::ToGraph(const FGeometry& Geometry, FVector2D Screen) const
{ return (Geometry.AbsoluteToLocal(Screen) - Pan) / Zoom; }

int32 SJTSMaterialGraph::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
	FSlateWindowElementList& E, int32 Layer, const FWidgetStyle&, bool) const
{
	const auto* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto Box = [&](FVector2D P, FVector2D Size, FLinearColor C, int32 L)
	{
		FSlateDrawElement::MakeBox(E, L, Geometry.ToPaintGeometry(FVector2f(Size * Zoom), FSlateLayoutTransform(FVector2f(Pan + P * Zoom))), Brush, ESlateDrawEffect::None, C);
	};
	auto Text = [&](FVector2D P, const FString& S, FLinearColor C, int32 FontSize, int32 L)
	{
		FSlateDrawElement::MakeText(E, L, Geometry.ToPaintGeometry(FVector2f(1, 1), FSlateLayoutTransform(Zoom, FVector2f(Pan + P * Zoom))),
			S, FCoreStyle::GetDefaultFontStyle("Regular", FontSize), ESlateDrawEffect::None, C);
	};
	FSlateDrawElement::MakeBox(E, Layer, Geometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, FLinearColor(.008f, .017f, .025f));
	const FVector2D View = Geometry.GetLocalSize();
	const float Step = 28 * Zoom;
	for (float X = FMath::Fmod(Pan.X, Step); X < View.X; X += Step)
		FSlateDrawElement::MakeLines(E, Layer + 1, Geometry.ToPaintGeometry(), { FVector2f(X, 0), FVector2f(X, View.Y) }, ESlateDrawEffect::None, FLinearColor(.033f, .055f, .067f), false);
	for (float Y = FMath::Fmod(Pan.Y, Step); Y < View.Y; Y += Step)
		FSlateDrawElement::MakeLines(E, Layer + 1, Geometry.ToPaintGeometry(), { FVector2f(0, Y), FVector2f(View.X, Y) }, ESlateDrawEffect::None, FLinearColor(.033f, .055f, .067f), false);
	auto Wire = [&](FVector2D A, FVector2D B, FLinearColor C)
	{
		const float T = FMath::Max(60.f, float(FMath::Abs(B.X - A.X) * .5)) * Zoom;
		FSlateDrawElement::MakeCubicBezierSpline(E, Layer + 2, Geometry.ToPaintGeometry(), FVector2f(Pan + A * Zoom),
			FVector2f(Pan + A * Zoom + FVector2D(T, 0)), FVector2f(Pan + B * Zoom - FVector2D(T, 0)),
			FVector2f(Pan + B * Zoom), FMath::Max(1.f, 2 * Zoom), ESlateDrawEffect::None, C);
	};
	for (const auto& Link : Graph.Links)
	{
		const auto* From = FindNode(Link.From); const auto* To = FindNode(Link.To);
		if (From && To) Wire(From->Position + FVector2D(224, 44), To->Position + FVector2D(0, 44), FLinearColor(.28f, .68f, .73f));
	}
	if (const auto* From = FindNode(WiringFrom)) Wire(From->Position + FVector2D(224, 44), WireEnd, FLinearColor(.93f, .86f, .48f));
	const auto* Book = Processor.IsValid() ? Processor->RecipeBook.Get() : nullptr;
	for (const auto& Node : Graph.Nodes)
	{
		const FVector2D P = Node.Position;
		const auto Color = ColorFor(Node.Kind);
		Box(P - FVector2D(2, 2), NodeSize + FVector2D(4, 4), Node.Id == Selected ? FLinearColor(.78f, .87f, .92f) : FLinearColor(.1f, .18f, .23f), Layer + 3);
		Box(P, NodeSize, FLinearColor(.025f, .047f, .063f), Layer + 4);
		Box(P, FVector2D(224, 32), Color * .35f, Layer + 5);
		const FJTSMaterialRecipe* Recipe = Book ? Book->FindRecipe(Node.RecipeId) : nullptr;
		const FString Title = Node.Kind == EJTSMaterialNodeKind::Input ? TEXT("仓库输入") : Node.Kind == EJTSMaterialNodeKind::Output ? TEXT("仓库输出") : Recipe ? Recipe->DisplayName.ToString() : TEXT("未配置程序");
		Text(P + FVector2D(12, 7), Title, FLinearColor::White, 16, Layer + 6);
		if (Node.Kind != EJTSMaterialNodeKind::Input) Box(P + FVector2D(-6, 38), FVector2D(12, 12), Color, Layer + 7);
		if (Node.Kind != EJTSMaterialNodeKind::Output) Box(P + FVector2D(218, 38), FVector2D(12, 12), Color, Layer + 7);
		if (Node.Kind == EJTSMaterialNodeKind::Program && Recipe)
		{
			Text(P + FVector2D(12, 50), TEXT("输入  ") + JTSMaterialProcessing::FormatQuantities(Recipe->Inputs, Node.Batches), FLinearColor(.67f, .79f, .83f), 12, Layer + 6);
			Text(P + FVector2D(12, 78), TEXT("输出  ") + JTSMaterialProcessing::FormatQuantities(Recipe->Outputs, Node.Batches), FLinearColor(.39f, .9f, .67f), 12, Layer + 6);
			Text(P + FVector2D(12, 115), TEXT("批数  ") + LexToString(Node.Batches), FLinearColor::White, 14, Layer + 6);
			Box(P + FVector2D(150, 109), FVector2D(26, 28), FLinearColor(.12f, .21f, .27f), Layer + 5);
			Box(P + FVector2D(184, 109), FVector2D(26, 28), FLinearColor(.12f, .21f, .27f), Layer + 5);
			Text(P + FVector2D(158, 111), TEXT("−"), FLinearColor::White, 17, Layer + 6);
			Text(P + FVector2D(191, 111), TEXT("+"), FLinearColor::White, 17, Layer + 6);
		}
		else
		{
			Text(P + FVector2D(12, 58), Node.Kind == EJTSMaterialNodeKind::Input ? TEXT("取用飞船共享仓库的材料") : TEXT("将全部产物存回共享仓库"), FLinearColor(.67f, .79f, .83f), 13, Layer + 6);
			Text(P + FVector2D(12, 91), Node.Kind == EJTSMaterialNodeKind::Input ? TEXT("按连线顺序处理") : TEXT("完整流程一次结算"), FLinearColor(.43f, .59f, .67f), 12, Layer + 6);
		}
	}
	return Layer + 8;
}

FReply SJTSMaterialGraph::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& Event)
{
	const auto Key = Event.GetEffectingButton();
	const FVector2D P = ToGraph(G, Event.GetScreenSpacePosition());
	if (Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton)
	{
		for (int32 I = Graph.Nodes.Num() - 1; I >= 0; --I)
		{
			auto& Node = Graph.Nodes[I];
			const bool bOutputPin = Node.Kind != EJTSMaterialNodeKind::Output && FVector2D::Distance(P, Node.Position + FVector2D(224, 44)) <= 14;
			const bool bInputPin = Node.Kind != EJTSMaterialNodeKind::Input && FVector2D::Distance(P, Node.Position + FVector2D(0, 44)) <= 14;
			if (bOutputPin || bInputPin)
			{
				Selected = Node.Id;
				if (Key == EKeys::RightMouseButton || Event.IsShiftDown()) Disconnect(Node.Id, bOutputPin);
				else if (bOutputPin) { WiringFrom = Node.Id; WireEnd = P; }
				else
				{
					const auto* Link = Graph.Links.FindByPredicate([&](const auto& L) { return L.To == Node.Id; });
					if (Link) { WiringFrom = Link->From; Disconnect(Node.Id, false); WireEnd = P; }
				}
				return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
			}
			if (Key == EKeys::LeftMouseButton && Inside(P, Node.Position, NodeSize))
			{
				Selected = Node.Id;
				if (Node.Kind == EJTSMaterialNodeKind::Program && Inside(P, Node.Position + FVector2D(150, 109), FVector2D(60, 28)))
					Node.Batches = FMath::Clamp(Node.Batches + (P.X - Node.Position.X >= 184 ? 1 : -1) * (Event.IsShiftDown() ? 10 : 1), 1, 1000);
				else { Dragged = Node.Id; DragOffset = P - Node.Position; }
				return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
			}
		}
	}
	if (Key == EKeys::MiddleMouseButton || Key == EKeys::RightMouseButton)
	{ bPanning = true; LastMouse = G.AbsoluteToLocal(Event.GetScreenSpacePosition()); return FReply::Handled().CaptureMouse(SharedThis(this)); }
	if (Key == EKeys::LeftMouseButton) { Selected = INDEX_NONE; return FReply::Handled().SetUserFocus(SharedThis(this)); }
	return FReply::Unhandled();
}

FReply SJTSMaterialGraph::OnMouseMove(const FGeometry& G, const FPointerEvent& Event)
{
	const FVector2D Local = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (bPanning) { Pan += Local - LastMouse; LastMouse = Local; }
	else if (auto* Node = FindNode(Dragged))
	{
		const FVector2D P = ToGraph(G, Event.GetScreenSpacePosition()) - DragOffset;
		Node->Position = FVector2D(FMath::Clamp(P.X, -32000., 32000.), FMath::Clamp(P.Y, -32000., 32000.));
	}
	else if (WiringFrom != INDEX_NONE) WireEnd = ToGraph(G, Event.GetScreenSpacePosition());
	else return FReply::Unhandled();
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SJTSMaterialGraph::OnMouseButtonUp(const FGeometry& G, const FPointerEvent& Event)
{
	if (!HasMouseCapture()) return FReply::Unhandled();
	if (WiringFrom != INDEX_NONE && Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D P = ToGraph(G, Event.GetScreenSpacePosition());
		for (const auto& Node : Graph.Nodes)
		{
			if (Node.Id == WiringFrom || Node.Kind == EJTSMaterialNodeKind::Input
				|| FVector2D::Distance(P, Node.Position + FVector2D(0, 44)) > 18) continue;
			Disconnect(WiringFrom, true); Disconnect(Node.Id, false);
			FJTSMaterialLink Link; Link.From = WiringFrom; Link.To = Node.Id; Graph.Links.Add(Link); break;
		}
	}
	Dragged = WiringFrom = INDEX_NONE; bPanning = false;
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SJTSMaterialGraph::OnMouseWheel(const FGeometry& G, const FPointerEvent& Event)
{
	const FVector2D Local = G.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FVector2D Before = (Local - Pan) / Zoom;
	Zoom = FMath::Clamp(Zoom * FMath::Pow(1.12f, Event.GetWheelDelta()), .25f, 1.5f);
	Pan = Local - Before * Zoom;
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SJTSMaterialGraph::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Delete || Event.GetKey() == EKeys::BackSpace) { DeleteSelected(); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::F) { FrameGraph(); return FReply::Handled(); }
	return FReply::Unhandled();
}

void SJTSMaterialGraph::OnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	SLeafWidget::OnMouseCaptureLost(Event);
	Dragged = WiringFrom = INDEX_NONE; bPanning = false;
}
