#include "space/Components/JTSMaterialProcessorComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr int64 MaxStoredHalfUnits = int64(MAX_int32) * 2 + 1;
	bool ValidQuantities(const TArray<FJTSMaterialQuantity>& Quantities)
	{
		if (Quantities.IsEmpty() || Quantities.Num() > 16) return false;
		TSet<EJTSResourceType> Seen;
		for (const auto& Q : Quantities)
		{
			if (uint8(Q.ResourceType) > uint8(EJTSResourceType::InorganicCarbon)
				|| Q.HalfUnits <= 0 || Q.HalfUnits > 1000000 || Seen.Contains(Q.ResourceType)) return false;
			Seen.Add(Q.ResourceType);
		}
		return true;
	}
}

UJTSMaterialProcessorComponent::UJTSMaterialProcessorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSMaterialProcessorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSMaterialProcessorComponent, SavedGraph);
}

bool UJTSMaterialProcessorComponent::ValidateLayout(const FJTSMaterialGraph& Graph, FString& Error) const
{
	Error.Reset();
	if (!RecipeBook || Graph.Nodes.Num() < 3 || Graph.Nodes.Num() > 32 || Graph.Links.Num() > 31)
	{ Error = TEXT("流程需要仓库输入、处理程序和仓库输出，最多 32 个节点。"); return false; }
	TMap<int32, EJTSMaterialNodeKind> Kinds;
	int32 Inputs = 0, Outputs = 0;
	for (const FJTSMaterialNode& Node : Graph.Nodes)
	{
		if (Node.Id < 0 || Node.Id > 1000000 || Kinds.Contains(Node.Id) || Node.Position.ContainsNaN()
			|| FMath::Abs(Node.Position.X) > 32768 || FMath::Abs(Node.Position.Y) > 32768
			|| Node.Batches < 1 || Node.Batches > 1000)
		{ Error = TEXT("节点数据无效；每个程序的批数为 1–1000。"); return false; }
		Kinds.Add(Node.Id, Node.Kind);
		if (Node.Kind == EJTSMaterialNodeKind::Input) ++Inputs;
		else if (Node.Kind == EJTSMaterialNodeKind::Output) ++Outputs;
		else if (Node.Kind == EJTSMaterialNodeKind::Program)
		{
			const FJTSMaterialRecipe* Recipe = RecipeBook->FindRecipe(Node.RecipeId);
			if (!Recipe || !ValidQuantities(Recipe->Inputs) || !ValidQuantities(Recipe->Outputs))
			{ Error = TEXT("处理程序未配置或配方无效。"); return false; }
		}
		else { Error = TEXT("未知节点类型。"); return false; }
	}
	if (Inputs != 1 || Outputs != 1)
	{ Error = TEXT("每条流程需要且只能有一个仓库输入和一个仓库输出。"); return false; }
	TSet<int32> LinkedFrom, LinkedTo;
	for (const auto& Link : Graph.Links)
	{
		if (Link.From == Link.To || !Kinds.Contains(Link.From) || !Kinds.Contains(Link.To)
			|| Kinds[Link.From] == EJTSMaterialNodeKind::Output || Kinds[Link.To] == EJTSMaterialNodeKind::Input
			|| LinkedFrom.Contains(Link.From) || LinkedTo.Contains(Link.To))
		{ Error = TEXT("每个端口只能连接一次；请按输入 → 程序 → 输出连接。"); return false; }
		LinkedFrom.Add(Link.From); LinkedTo.Add(Link.To);
	}
	return true;
}

bool UJTSMaterialProcessorComponent::Preview(const FJTSMaterialGraph& Graph,
	TMap<EJTSResourceType, int64>& OutDeltas, FString& Error) const
{
	OutDeltas.Reset();
	if (!ValidateLayout(Graph, Error)) return false;
	const AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship) { Error = TEXT("飞船不可用。"); return false; }
	TMap<int32, const FJTSMaterialNode*> Nodes;
	TMap<int32, int32> Next;
	int32 Current = INDEX_NONE;
	for (const auto& Node : Graph.Nodes)
	{
		Nodes.Add(Node.Id, &Node);
		if (Node.Kind == EJTSMaterialNodeKind::Input) Current = Node.Id;
	}
	for (const auto& Link : Graph.Links) Next.Add(Link.From, Link.To);
	TSet<int32> Visited;
	TArray<const FJTSMaterialNode*> Programs;
	while (Current != INDEX_NONE && !Visited.Contains(Current))
	{
		Visited.Add(Current);
		const auto* Node = Nodes[Current];
		if (Node->Kind == EJTSMaterialNodeKind::Program) Programs.Add(Node);
		if (Node->Kind == EJTSMaterialNodeKind::Output) break;
		const int32* To = Next.Find(Current);
		Current = To ? *To : INDEX_NONE;
	}
	if (Visited.Num() != Nodes.Num() || Current == INDEX_NONE
		|| Nodes[Current]->Kind != EJTSMaterialNodeKind::Output || Graph.Links.Num() != Nodes.Num() - 1)
	{ Error = TEXT("流程尚未连通：连接全部节点，且不要形成回路。"); return false; }

	TMap<EJTSResourceType, int64> Working;
	for (uint8 I = 0; I <= uint8(EJTSResourceType::InorganicCarbon); ++I)
		Working.Add(EJTSResourceType(I), Ship->GetResourceHalfUnits(EJTSResourceType(I)));
	for (const auto* Node : Programs)
	{
		const FJTSMaterialRecipe* Recipe = RecipeBook->FindRecipe(Node->RecipeId);
		for (const auto& Q : Recipe->Inputs)
		{
			const int64 Required = int64(Q.HalfUnits) * Node->Batches;
			if (Working[Q.ResourceType] < Required)
			{
				Error = FString::Printf(TEXT("%s：还缺 %s %s。"), *Recipe->DisplayName.ToString(),
					*JTSMaterialProcessing::FormatHalfUnits(Required - Working[Q.ResourceType]),
					*JTSMaterialProcessing::ResourceLabel(Q.ResourceType));
				return false;
			}
		}
		for (const auto& Q : Recipe->Inputs) Working[Q.ResourceType] -= int64(Q.HalfUnits) * Node->Batches;
		for (const auto& Q : Recipe->Outputs)
		{
			Working[Q.ResourceType] += int64(Q.HalfUnits) * Node->Batches;
			if (Working[Q.ResourceType] > MaxStoredHalfUnits)
			{ Error = TEXT("产出超过仓库容量上限。"); return false; }
		}
	}
	for (const auto& Entry : Working)
	{
		const int64 Delta = Entry.Value - Ship->GetResourceHalfUnits(Entry.Key);
		if (Delta) OutDeltas.Add(Entry.Key, Delta);
	}
	return true;
}

bool UJTSMaterialProcessorComponent::Submit(APawn* Player, const FJTSMaterialGraph& Graph, bool bExecute, FString& Message)
{
	AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !Ship->HasAuthority() || !IsValid(Player) || !Ship->CanUseShipTerminal(Player))
	{ Message = TEXT("请在飞船终端的交互范围内操作。"); return false; }
	if (!ValidateLayout(Graph, Message)) return false;
	TMap<EJTSResourceType, int64> Deltas;
	if (bExecute && !Preview(Graph, Deltas, Message)) return false;
	const FJTSMaterialGraph Previous = SavedGraph;
	SavedGraph = Graph;
	if (bExecute && !Ship->ApplyMaterialDeltas(Deltas))
	{ SavedGraph = Previous; Message = TEXT("仓库材料已变化，请重新检查流程。"); return false; }
	if (!bExecute) Ship->PersistMaterialConfiguration();
	Ship->ForceNetUpdate();
	Message = bExecute ? TEXT("处理完成，产物已进入飞船仓库。") : TEXT("流程已保存到本次远征。");
	return true;
}

void UJTSMaterialProcessorComponent::RestoreGraph(const FJTSMaterialGraph& Graph)
{
	FString Error;
	if (GetOwner() && GetOwner()->HasAuthority() && ValidateLayout(Graph, Error)) SavedGraph = Graph;
}
