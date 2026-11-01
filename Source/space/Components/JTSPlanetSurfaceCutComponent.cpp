#include "space/Components/JTSPlanetSurfaceCutComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "TimerManager.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "Operations/MeshBoolean.h"
#include "Operations/MinimalHoleFiller.h"
#include "MeshBoundaryLoops.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

using namespace UE::Geometry;

namespace
{
	// Render vertices are split at UV/normal seams. Weld positions while retaining corner attributes.
	bool ReadMesh(UStaticMesh* Asset, FDynamicMesh3& Mesh, const TArray<int32>& MaterialMap)
	{
		if (!Asset || !Asset->bAllowCPUAccess || !Asset->GetRenderData()) return false;
		Mesh.Clear(); Mesh.EnableAttributes();
		Mesh.Attributes()->EnableMaterialID(); Mesh.Attributes()->SetNumUVLayers(1);
		auto* Normals = Mesh.Attributes()->PrimaryNormals(); auto* UVs = Mesh.Attributes()->PrimaryUV();
		TMap<FVector, int32> Welded;
		const auto& LOD = Asset->GetRenderData()->LODResources[0];
		for (int32 Section = 0; Section < LOD.Sections.Num(); ++Section)
		{
			TArray<FVector> Positions, SourceNormals; TArray<int32> Indices;
			TArray<FVector2D> SourceUVs; TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Asset, 0, Section, Positions, Indices, SourceNormals, SourceUVs, Tangents);
			TArray<int32> VIDs, NIDs, UIDs;
			for (int32 I = 0; I < Positions.Num(); ++I)
			{
				int32* Existing = Welded.Find(Positions[I]);
				const int32 VID = Existing ? *Existing : Mesh.AppendVertex(Positions[I]);
				if (!Existing) Welded.Add(Positions[I], VID);
				VIDs.Add(VID); NIDs.Add(Normals->AppendElement(FVector3f(SourceNormals[I]))); UIDs.Add(UVs->AppendElement(FVector2f(SourceUVs[I])));
			}
			const int32 Slot = LOD.Sections[Section].MaterialIndex;
			for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
			{
				const int32 A = Indices[I], B = Indices[I+1], C = Indices[I+2];
				const int32 TID = Mesh.AppendTriangle(VIDs[A], VIDs[B], VIDs[C]);
				if (TID < 0) return false;
				Normals->SetTriangle(TID, FIndex3i(NIDs[A], NIDs[B], NIDs[C])); UVs->SetTriangle(TID, FIndex3i(UIDs[A], UIDs[B], UIDs[C]));
				Mesh.Attributes()->GetMaterialID()->SetValue(TID, MaterialMap.IsValidIndex(Slot) ? MaterialMap[Slot] : 0);
			}
		}
		if (!Mesh.IsClosed())
		{
			FMergeCoincidentMeshEdges Weld(&Mesh); Weld.MergeVertexTolerance = 0.00001; Weld.OnlyUniquePairs = true; Weld.Apply();
		}
		return Mesh.TriangleCount() > 0 && Mesh.IsClosed();
	}
}

struct FJTSPlanetSurfaceCutState
{
	struct FCut { TWeakObjectPtr<AActor> Owner; TWeakObjectPtr<UStaticMesh> Volume; FTransform Transform; bool bApplied = false; };
	TArray<FCut> Cuts;
	FDynamicMesh3 Original;
	TUniquePtr<FDynamicMeshAABBTree3> Tree;
	TWeakObjectPtr<UStaticMeshComponent> Source;
	ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
	bool bVisible = true;
};

void FJTSPlanetSurfaceCutStateDeleter::operator()(FJTSPlanetSurfaceCutState* Value) const { delete Value; }
UJTSPlanetSurfaceCutComponent::UJTSPlanetSurfaceCutComponent() { PrimaryComponentTick.bCanEverTick = false; State.Reset(new FJTSPlanetSurfaceCutState()); }
UJTSPlanetSurfaceCutComponent::~UJTSPlanetSurfaceCutComponent() = default;

bool UJTSPlanetSurfaceCutComponent::EnsureSource()
{
	if (State->Source.IsValid()) return true;
	auto* Planet = Cast<AJTSPlanetAnchor>(GetOwner());
	auto* Source = Planet ? Cast<UStaticMeshComponent>(Planet->GetGameplaySurfaceComponent()) : nullptr;
	if ((!Source || !Source->GetStaticMesh()) && Planet && Planet->GetGameplaySurfaceActor())
	{
		TArray<UStaticMeshComponent*> Components; Planet->GetGameplaySurfaceActor()->GetComponents(Components);
		for (auto* Candidate : Components) if (Candidate->GetStaticMesh()) { Source = Candidate; break; }
	}
	if (!Source || !Source->GetStaticMesh() || !CutSourceMesh) return false;
	TArray<int32> Materials; for (int32 I = 0; I < CutSourceMesh->GetStaticMaterials().Num(); ++I) Materials.Add(I);
	if (!ReadMesh(CutSourceMesh, State->Original, Materials))
	{
		UE_LOG(LogTemp, Error, TEXT("Surface cut source %s must be closed and CPU-readable"), *GetNameSafe(CutSourceMesh)); return false;
	}
	State->Source = Source; State->Collision = Source->GetCollisionEnabled(); State->bVisible = Source->IsVisible();
	State->Tree = MakeUnique<FDynamicMeshAABBTree3>(&State->Original, true);
	return true;
}

bool UJTSPlanetSurfaceCutComponent::ProjectOriginalSurface(FVector Point, FJTSPlanetSurfaceHit& Hit) const
{
	if (!State->Tree || !State->Source.IsValid()) return false;
	const auto* Planet = Cast<AJTSPlanetAnchor>(GetOwner()); if (!Planet) return false;
	const auto* Source = State->Source.Get(); const FTransform Pose = Source->GetComponentTransform();
	const FVector Up = Planet->GetRadialUpVector(Point), Center = Planet->GetPlanetCenter();
	const double Radius = Source->Bounds.SphereRadius + FVector::Distance(Source->Bounds.Origin, Center) + 1000;
	const FVector Start = Pose.InverseTransformPosition(Center + Up * Radius), End = Pose.InverseTransformPosition(Center - Up * Radius);
	const FRay3d Ray(Start, (End - Start).GetSafeNormal()); double Distance; int32 TID;
	if (!State->Tree->FindNearestHitTriangle(Ray, Distance, TID)) return false;
	Hit = FJTSPlanetSurfaceHit(); Hit.bBlockingHit = true;
	Hit.ImpactPoint = Pose.TransformPosition(Ray.PointAt(Distance));
	Hit.ImpactNormal = Pose.TransformVectorNoScale(State->Original.GetTriNormal(TID)).GetSafeNormal();
	if (FVector::DotProduct(Hit.ImpactNormal, Up) < 0) Hit.ImpactNormal *= -1;
	Hit.HitActor = Source->GetOwner(); Hit.HitComponent = State->Source.Get(); return true;
}

void UJTSPlanetSurfaceCutComponent::RegisterCut(AActor* CutOwner, UStaticMesh* Volume, const FTransform& WorldTransform)
{
	if (!IsValid(CutOwner) || !Volume || !EnsureSource()) return;
	for (auto& Cut : State->Cuts) if (Cut.Owner == CutOwner)
	{
		if (Cut.Volume == Volume && Cut.Transform.Equals(WorldTransform)) return;
		Cut.Volume = Volume; Cut.Transform = WorldTransform; Cut.bApplied = false; QueueRebuild(); return;
	}
	State->Cuts.Add({CutOwner, Volume, WorldTransform}); QueueRebuild();
}

void UJTSPlanetSurfaceCutComponent::RemoveCut(AActor* CutOwner)
{
	if (State->Cuts.RemoveAll([CutOwner](const auto& Cut) { return !Cut.Owner.IsValid() || Cut.Owner == CutOwner; })) QueueRebuild();
}

void UJTSPlanetSurfaceCutComponent::QueueRebuild()
{
	if (!GetWorld() || GetWorld()->bIsTearingDown || GetWorld()->GetTimerManager().TimerExists(RebuildTimer)) return;
	RebuildTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this] { RebuildNow(); }));
}

int32 UJTSPlanetSurfaceCutComponent::GetCutCount() const { return State->Cuts.Num(); }
bool UJTSPlanetSurfaceCutComponent::IsCutApplied(const AActor* CutOwner) const
{
	for (const auto& Cut : State->Cuts) if (Cut.Owner == CutOwner) return Cut.bApplied;
	return false;
}

bool UJTSPlanetSurfaceCutComponent::RebuildNow()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RebuildTimer);
	State->Cuts.RemoveAll([](const auto& Cut) { return !Cut.Owner.IsValid() || !Cut.Volume.IsValid(); });
	if (State->Cuts.IsEmpty()) { RestoreOriginal(); OnCutsRebuilt.Broadcast(); return true; }
	if (!EnsureSource()) return false;
	FDynamicMesh3 Current(State->Original);
	TArray<UMaterialInterface*> Materials = State->Source->GetMaterials();
	const FTransform SourcePose = State->Source->GetComponentTransform();
	bool bAllApplied = true;
	for (auto& Cut : State->Cuts)
	{
		Cut.bApplied = false;
		TArray<int32> MaterialMap;
		for (const auto& Slot : Cut.Volume->GetStaticMaterials()) MaterialMap.Add(Materials.AddUnique(Slot.MaterialInterface));
		FDynamicMesh3 Tool;
		if (!ReadMesh(Cut.Volume.Get(), Tool, MaterialMap)) { bAllApplied = false; continue; }
		for (int32 VID : Tool.VertexIndicesItr())
		{
			// Keep the closed cutter rigid. Independently projecting its vertices onto
			// adjacent surface facets can fold its walls through each other.
			Tool.SetVertex(VID, SourcePose.InverseTransformPosition(Cut.Transform.TransformPosition(Tool.GetVertex(VID))));
		}
		FMeshNormals::InitializeOverlayToPerTriangleNormals(Tool.Attributes()->PrimaryNormals());
		FDynamicMesh3 Result;
		// A planet is much larger than a burrow. Retry fine intersections without
		// collapsing short edges, rather than discarding every other valid cut.
		for (int32 Attempt = 0; Attempt < 3; ++Attempt)
		{
			Result.Clear();
			FMeshBoolean Boolean(&Current, &Tool, &Result, FMeshBoolean::EBooleanOp::Difference);
			Boolean.SnapTolerance = Attempt == 0 ? 1.e-6 : (Attempt == 1 ? 1.e-8 : 1.e-7);
			Boolean.bCollapseDegenerateEdgesOnCut = Attempt == 0;
			Boolean.bSimplifyAlongNewEdges = false;
			Boolean.Compute();
			if (!Result.TriangleCount()) continue;
			if (!Result.IsClosed())
			{
				FMergeCoincidentMeshEdges Weld(&Result);
				Weld.MergeVertexTolerance = 1.e-5; Weld.OnlyUniquePairs = true; Weld.Apply();
				// Both inputs are closed: every remaining boundary was introduced by
				// this Boolean, including edges not retained in CreatedBoundaryEdges.
				FMeshBoundaryLoops Boundary(&Result, true);
				const double MaxRepairSpan = Tool.GetBounds().MaxDim() * .01;
				for (const FEdgeLoop& Loop : Boundary.Loops)
				{
					FAxisAlignedBox3d Bounds;
					for (int VID : Loop.Vertices) Bounds.Contain(Result.GetVertex(VID));
					if (Bounds.MaxDim() > MaxRepairSpan) continue;
					const int32 Neighbor = Result.GetEdgeT(Loop.Edges[0]).A;
					const int32 Material = Result.Attributes()->GetMaterialID()->GetValue(Neighbor);
					FMinimalHoleFiller Filler(&Result, Loop);
					if (!Filler.Fill()) continue;
					FDynamicMeshEditor Editor(&Result); Editor.SetTriangleNormals(Filler.NewTriangles);
					Editor.SetTriangleUVsFromProjection(Filler.NewTriangles, FFrame3d(Result.GetTriCentroid(Neighbor), Result.GetTriNormal(Neighbor)), 1.0f);
					for (int TID : Filler.NewTriangles) Result.Attributes()->GetMaterialID()->SetValue(TID, Material);
				}
			}
			if (Result.IsClosed()) break;
		}
		if (Result.TriangleCount() == 0 || !Result.IsClosed())
		{
			UE_LOG(LogTemp, Warning, TEXT("Surface cut rejected for %s at %s; preserving other cuts and suppressing its decoration"), *GetNameSafe(Cut.Owner.Get()), *Cut.Transform.GetLocation().ToString());
			bAllApplied = false; continue;
		}
		Current = MoveTemp(Result); Cut.bApplied = true;
	}
	if (!RuntimeSurface)
	{
		RuntimeSurface = NewObject<UDynamicMeshComponent>(State->Source->GetOwner(), TEXT("TemporaryCutSurface"));
		RuntimeSurface->SetupAttachment(State->Source.Get()); RuntimeSurface->SetRelativeTransform(FTransform::Identity);
		RuntimeSurface->SetCollisionObjectType(State->Source->GetCollisionObjectType());
		RuntimeSurface->SetCollisionResponseToChannels(State->Source->GetCollisionResponseToChannels());
		RuntimeSurface->SetCanEverAffectNavigation(false);
		RuntimeSurface->SetDeferredCollisionUpdatesEnabled(true, false);
		RuntimeSurface->SetComplexAsSimpleCollisionEnabled(true, false);
		RuntimeSurface->SetTangentsType(EDynamicMeshComponentTangentsMode::AutoCalculated);
		RuntimeSurface->RegisterComponent();
	}
	for (int32 I = 0; I < Materials.Num(); ++I) RuntimeSurface->SetMaterial(I, Materials[I]);
	RuntimeSurface->SetMesh(MoveTemp(Current)); RuntimeSurface->UpdateCollision(false);
	RuntimeSurface->SetCollisionEnabled(State->Collision); RuntimeSurface->SetVisibility(State->bVisible);
	State->Source->SetVisibility(false); State->Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnCutsRebuilt.Broadcast();
	return bAllApplied;
}

void UJTSPlanetSurfaceCutComponent::RestoreOriginal()
{
	if (RuntimeSurface) { RuntimeSurface->DestroyComponent(); RuntimeSurface = nullptr; }
	if (State->Source.IsValid()) { State->Source->SetVisibility(State->bVisible); State->Source->SetCollisionEnabled(State->Collision); }
}

void UJTSPlanetSurfaceCutComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RebuildTimer);
	RestoreOriginal(); State->Cuts.Reset(); Super::EndPlay(Reason);
}
