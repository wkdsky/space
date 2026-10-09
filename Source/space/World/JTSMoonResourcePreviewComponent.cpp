#include "space/World/JTSMoonResourcePreviewComponent.h"

#include "Engine/World.h"
#if WITH_EDITOR
#include "MeshElementCollector.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneView.h"

namespace
{
	class FJTSMoonResourcePreviewProxy final : public FPrimitiveSceneProxy
	{
	public:
		explicit FJTSMoonResourcePreviewProxy(const UJTSMoonResourcePreviewComponent* Component)
			: FPrimitiveSceneProxy(Component), Lines(Component->BatchedLines)
		{
			bWillEverBeLit = false;
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static uint8 Unique;
			return reinterpret_cast<SIZE_T>(&Unique);
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
			const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			for (int32 Index = 0; Index < Views.Num(); ++Index)
			{
				if (!(VisibilityMap & (1u << Index))) continue;
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(Index);
				for (const FBatchedLine& Line : Lines)
				{
					PDI->DrawLine(Line.Start, Line.End, Line.Color, Line.DepthPriority, Line.Thickness, 0, true);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Relevance;
			Relevance.bDrawRelevance = IsShown(View) && !View->Family->EngineShowFlags.Game;
			Relevance.bDynamicRelevance = true;
			Relevance.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Relevance;
		}

		virtual uint32 GetMemoryFootprint() const override
		{
			return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize();
		}

	private:
		TArray<FBatchedLine> Lines;
	};
}
#endif

UJTSMoonResourcePreviewComponent::UJTSMoonResourcePreviewComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEditorOnly = true;
	PrimaryComponentTick.bCanEverTick = false;
	SetHiddenInGame(true);
}

FPrimitiveSceneProxy* UJTSMoonResourcePreviewComponent::CreateSceneProxy()
{
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld() && !BatchedLines.IsEmpty()) return new FJTSMoonResourcePreviewProxy(this);
#endif
	return nullptr;
}
