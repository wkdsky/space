#pragma once

#include "Widgets/SLeafWidget.h"
#include "space/Items/JTSMaterialProcessingRecipes.h"

class UJTSMaterialProcessorComponent;

/** Runtime-only graph canvas, without dependencies on editor Blueprint/GraphEditor modules. */
class SJTSMaterialGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SJTSMaterialGraph) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&);
	void Initialize(UJTSMaterialProcessorComponent* Processor);
	void AddProgram(FName Recipe);
	void ResetGraph();
	void DeleteSelected();
	void FrameGraph();
	const FJTSMaterialGraph& GetGraph() const { return Graph; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(980, 350); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&,
		int32, const FWidgetStyle&, bool) const override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;

private:
	FVector2D ToGraph(const FGeometry& Geometry, FVector2D Screen) const;
	FJTSMaterialNode* FindNode(int32 Id);
	const FJTSMaterialNode* FindNode(int32 Id) const;
	void Disconnect(int32 Id, bool bOutput);
	FJTSMaterialGraph Graph;
	TWeakObjectPtr<UJTSMaterialProcessorComponent> Processor;
	FVector2D Pan = FVector2D::ZeroVector;
	FVector2D LastMouse = FVector2D::ZeroVector;
	FVector2D DragOffset = FVector2D::ZeroVector;
	FVector2D WireEnd = FVector2D::ZeroVector;
	float Zoom = 1.0f;
	int32 Selected = INDEX_NONE;
	int32 Dragged = INDEX_NONE;
	int32 WiringFrom = INDEX_NONE;
	bool bPanning = false;
};
