#include "JTSCorpseMeshComponent.h"
#include "space/Components/JTSDiscoveryComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"

namespace
{
	struct FTriangle { int32 Vertex[3]; int32 Material; };
	struct FPart { TArray<FTriangle> Triangles; FBox Bounds=FBox(ForceInit); };
	struct FGeometry
	{
		TWeakObjectPtr<USkeletalMesh> Mesh;
		TWeakObjectPtr<UAnimSequence> Pose;
		TArray<FVector> Positions, Normals, Tangents;
		TArray<FVector2D> UVs;
		TArray<FPart> Parts;
	};
	TMap<FString,TSharedPtr<FGeometry>> Cache;

	int32 Interpolate(FGeometry& G,int32 A,int32 B,float Alpha)
	{
		const int32 V=G.Positions.Add(FMath::Lerp(G.Positions[A],G.Positions[B],Alpha));
		G.Normals.Add(FMath::Lerp(G.Normals[A],G.Normals[B],Alpha).GetSafeNormal());
		G.Tangents.Add(FMath::Lerp(G.Tangents[A],G.Tangents[B],Alpha).GetSafeNormal());
		G.UVs.Add(FMath::Lerp(G.UVs[A],G.UVs[B],Alpha));
		return V;
	}

	void ClipTriangle(FGeometry& G,const FTriangle& T,int32 Axis,float Mid,bool bLeft,TArray<FTriangle>& Out)
	{
		TArray<int32,TInlineAllocator<4>> Polygon;
		for(int32 I=0;I<3;++I)
		{
			const int32 A=T.Vertex[I],B=T.Vertex[(I+1)%3];
			const float DA=G.Positions[A][Axis]-Mid,DB=G.Positions[B][Axis]-Mid;
			const bool InsideA=bLeft?DA<=0:DA>=0,InsideB=bLeft?DB<=0:DB>=0;
			if(InsideA) Polygon.Add(A);
			if(InsideA!=InsideB) Polygon.Add(Interpolate(G,A,B,DA/(DA-DB)));
		}
		for(int32 I=1;I+1<Polygon.Num();++I)
			if(FVector::CrossProduct(G.Positions[Polygon[I]]-G.Positions[Polygon[0]],G.Positions[Polygon[I+1]]-G.Positions[Polygon[0]]).SizeSquared()>1.e-8)
				Out.Add(FTriangle{{Polygon[0],Polygon[I],Polygon[I+1]},T.Material});
	}

	void AddParts(FGeometry& G, TArray<FTriangle>&& Triangles, float MaximumSpan, int32 Depth=0)
	{
		if (Triangles.IsEmpty()) return;
		FBox B(ForceInit);
		for (const auto& T:Triangles) for (int32 V:T.Vertex) B+=G.Positions[V];
		const FVector Size=B.GetSize();
		if (MaximumSpan>0 && Size.GetMax()>MaximumSpan+.01f && Depth<18)
		{
			const int32 Axis=Size.X>=Size.Y && Size.X>=Size.Z?0:Size.Y>=Size.Z?1:2;
			const float Mid=B.GetCenter()[Axis];
			TArray<FTriangle> A,C;
			for (const auto& T:Triangles)
			{
				const float D0=G.Positions[T.Vertex[0]][Axis]-Mid,D1=G.Positions[T.Vertex[1]][Axis]-Mid,D2=G.Positions[T.Vertex[2]][Axis]-Mid;
				if(FMath::Max3(D0,D1,D2)<=0) A.Add(T);
				else if(FMath::Min3(D0,D1,D2)>=0) C.Add(T);
				else { ClipTriangle(G,T,Axis,Mid,true,A); ClipTriangle(G,T,Axis,Mid,false,C); }
			}
			if (!A.IsEmpty() && !C.IsEmpty())
			{
				AddParts(G,MoveTemp(A),MaximumSpan,Depth+1); AddParts(G,MoveTemp(C),MaximumSpan,Depth+1); return;
			}
		}
		auto& P=G.Parts.AddDefaulted_GetRef(); P.Bounds=B; P.Triangles=MoveTemp(Triangles);
	}

	TSharedPtr<FGeometry> GetGeometry(const FJTSAnalysisEntry& Entry,UWorld* World)
	{
		if (!Entry.SpecimenMesh || !Entry.DeathAnimation || !World) return nullptr;
		const FString Key=FString::Printf(TEXT("%s|%s|%f|%d|%f"),*Entry.SpecimenMesh->GetPathName(),*Entry.DeathAnimation->GetPathName(),Entry.SpecimenScale,Entry.bSeparateCorpseParts,Entry.MaxCorpsePartSpan);
		if (auto* Found=Cache.Find(Key); Found && (*Found)->Mesh.IsValid() && (*Found)->Pose.IsValid()) return *Found;
		const auto* Render=Entry.SpecimenMesh->GetResourceForRendering();
		if (!Render || Render->LODRenderData.IsEmpty()) return nullptr;
		const auto& LOD=Render->LODRenderData[0];
		const auto* Weights=LOD.GetSkinWeightVertexBuffer();
		const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
		if (!Weights || !Indices || !LOD.StaticVertexBuffers.PositionVertexBuffer.GetAllowCPUAccess()
			|| !Weights->GetNeedsCPUAccess()) return nullptr;
		// Evaluate the same authored death clip on the original skeletal asset once. Works on a server
		// without a rendered skeletal scene proxy; avoids the render-thread flush of GetCPUSkinnedVertices.
		auto* Pose=NewObject<USkeletalMeshComponent>(World);
		Pose->SetSkeletalMesh(Entry.SpecimenMesh); Pose->SetVisibility(false);
		Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision); Pose->RegisterComponentWithWorld(World);
		Pose->PlayAnimation(Entry.DeathAnimation,false);
		Pose->SetPosition(Entry.DeathAnimation->GetPlayLength(),false);
		Pose->TickAnimation(0,false); Pose->RefreshBoneTransforms();
		TArray<FMatrix44f> Matrices; Pose->GetCurrentRefToLocalMatrices(Matrices,0);
		TArray<FVector3f> Skinned; USkinnedMeshComponent::ComputeSkinnedPositions(Pose,Skinned,Matrices,LOD,*Weights);
		auto G=MakeShared<FGeometry>(); G->Mesh=Entry.SpecimenMesh; G->Pose=Entry.DeathAnimation;
		const auto& VB=LOD.StaticVertexBuffers.StaticMeshVertexBuffer;
		G->Positions.SetNum(Skinned.Num()); G->Normals.SetNum(Skinned.Num()); G->Tangents.SetNum(Skinned.Num()); G->UVs.SetNum(Skinned.Num());
		TArray<int32> Bones; Bones.SetNumZeroed(Skinned.Num());
		for (const auto& Section:LOD.RenderSections)
		for (uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
		{
			G->Positions[V]=FVector(Skinned[V])*Entry.SpecimenScale;
			G->UVs[V]=FVector2D(VB.GetVertexUV(V,0));
			FVector3f N(0),T(0); uint16 Best=0;
			for (uint32 I=0;I<Weights->GetMaxBoneInfluences();++I)
			{
				const uint16 Weight=Weights->GetBoneWeight(V,I); if (!Weight) continue;
				const int32 Bone=Section.BoneMap[Weights->GetBoneIndex(V,I)];
				if (Weight>Best) { Best=Weight; Bones[V]=Bone; }
				N+=Matrices[Bone].TransformVector(VB.VertexTangentZ(V))*float(Weight);
				T+=Matrices[Bone].TransformVector(VB.VertexTangentX(V))*float(Weight);
			}
			G->Normals[V]=FVector(N.GetSafeNormal()); G->Tangents[V]=FVector(T.GetSafeNormal());
		}
		// Keep original bone groups, UVs and materials. Oversized groups split their existing surface
		// into portable pieces, interpolating boundary vertices without adding any replacement shape.
		TMap<int32,TArray<FTriangle>> Groups;
		for (const auto& Section:LOD.RenderSections)
		for (uint32 I=Section.BaseIndex;I<Section.BaseIndex+Section.NumTriangles*3;I+=3)
		{
			FTriangle T{{int32(Indices->Get(I)),int32(Indices->Get(I+1)),int32(Indices->Get(I+2))},Section.MaterialIndex};
			Groups.FindOrAdd(Entry.bSeparateCorpseParts?Bones[T.Vertex[0]]:0).Add(T);
		}
		TArray<int32> Keys; Groups.GetKeys(Keys); Keys.Sort();
		for (int32 Bone:Keys) AddParts(*G,MoveTemp(Groups[Bone]),Entry.bSeparateCorpseParts?Entry.MaxCorpsePartSpan:0);
		Pose->DestroyComponent(); Cache.Add(Key,G); return G;
	}
}

UJTSCorpseMeshComponent::UJTSCorpseMeshComponent(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick=false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision); SetGenerateOverlapEvents(false); SetCanEverAffectNavigation(false);
}
TArray<FBox> UJTSCorpseMeshComponent::GetPartBounds(const FJTSAnalysisEntry& Entry,UWorld* World)
{
	TArray<FBox> Result;
	if (auto G=GetGeometry(Entry,World)) for (const auto& P:G->Parts) Result.Add(P.Bounds);
	return Result;
}
bool UJTSCorpseMeshComponent::SetSample(const FJTSItemInstance& Item,const UJTSAnalysisCatalog* Catalog)
{
	if (!Catalog) if (auto* D=UJTSDiscoveryComponent::Find(this)) Catalog=D->GetCatalog();
	const auto* Entry=Catalog?Catalog->Find(Item.ItemId,Item.SpecimenId):nullptr;
	if (Item.IsEmpty() || !Entry || !Entry->SpecimenMesh)
	{
		SetVisibility(false); return false;
	}
	const int32 Part=Item.CorpsePartIndex==INDEX_NONE?0:Item.CorpsePartIndex;
	if (SourceMesh==Entry->SpecimenMesh && SourcePose==Entry->DeathAnimation && CurrentSpecies==Item.SpecimenId && CurrentPart==Part && GetNumSections()>0)
	{ SetVisibility(true); SetHiddenInGame(false); return true; }
	ClearAllMeshSections(); SourceBounds=FBox(ForceInit);
	auto G=GetGeometry(*Entry,GetWorld());
	if (!G || !G->Parts.IsValidIndex(Part)) { SetVisibility(false); return false; }
	const auto& P=G->Parts[Part]; SourceBounds=P.Bounds;
	TMap<int32,TArray<FTriangle>> Materials;
	for (const auto& T:P.Triangles) Materials.FindOrAdd(T.Material).Add(T);
	int32 Section=0;
	TArray<int32> MaterialIds; Materials.GetKeys(MaterialIds); MaterialIds.Sort();
	for (int32 Material:MaterialIds)
	{
		TArray<FVector> Vertices,Normals; TArray<FVector2D> UVs; TArray<FProcMeshTangent> Tangents; TArray<int32> Indices;
		TMap<int32,int32> Remap;
		for (const auto& T:Materials[Material]) for (int32 V:T.Vertex)
		{
			int32* M=Remap.Find(V);
			if (!M)
			{
				const int32 Index=Vertices.Add(G->Positions[V]); Normals.Add(G->Normals[V]); UVs.Add(G->UVs[V]); Tangents.Add(FProcMeshTangent(G->Tangents[V],false));
				M=&Remap.Add(V,Index);
			}
			Indices.Add(*M);
		}
		CreateMeshSection(Section,Vertices,Indices,Normals,UVs,TArray<FColor>(),Tangents,false);
		SetMaterial(Section,Entry->SpecimenMesh->GetMaterials()[Material].MaterialInterface); ++Section;
	}
	SourceMesh=Entry->SpecimenMesh; SourcePose=Entry->DeathAnimation; CurrentSpecies=Item.SpecimenId; CurrentPart=Part;
	SetHiddenInGame(false); SetVisibility(true); return true;
}
bool UJTSCorpseMeshComponent::RequiresTwoHandCarry(float MaxSpan, float MaxGripWidth) const
{
	if (!SourceBounds.IsValid) return true;
	const FVector Size = SourceBounds.GetSize();
	const float Middle = Size.X + Size.Y + Size.Z - Size.GetMax() - Size.GetMin();
	return Size.GetMax() > MaxSpan || Middle > MaxGripWidth;
}
void UJTSCorpseMeshComponent::PlaceAt(const FVector& Origin,bool bOnSurface,float MaximumSpan)
{
	if (!SourceBounds.IsValid) return;
	const float Scale=MaximumSpan>0?FMath::Min(1.f,MaximumSpan/FMath::Max(1.f,SourceBounds.GetSize().GetMax())):1.f;
	SetRelativeScale3D(FVector(Scale)); SetRelativeRotation(FRotator::ZeroRotator);
	const FVector Pivot(SourceBounds.GetCenter().X,SourceBounds.GetCenter().Y,bOnSurface?SourceBounds.Min.Z:SourceBounds.GetCenter().Z);
	SetRelativeLocation(Origin-Pivot*Scale);
}
