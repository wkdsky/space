#include "space/Systems/JTSPlanetCrowdNavigation.h"

#include "Engine/World.h"
#include "space/World/JTSPlanetAnchor.h"

namespace
{
	constexpr float CellSize = 250.0f;
	constexpr int32 MaxFields = 8;
	constexpr int32 MaxCells = 16384;
	const FIntPoint Neighbors[] = { {1,0}, {0,1}, {-1,0}, {0,-1}, {1,1}, {-1,1}, {-1,-1}, {1,-1} };
	const int32 ReverseEdges[] = { 2,3,0,1,6,7,4,5 };
	struct FNode
	{
		FVector Ground = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
		uint8 KnownEdges = 0;
		uint8 OpenEdges = 0;
		bool bSampled = false;
		bool bWalkable = false;
	};
	struct FFrontier
	{
		FIntPoint Cell;
		float Cost;
		bool operator<(const FFrontier& Other) const { return Cost < Other.Cost; }
	};
	struct FCrowdRouteField
	{
		TWeakObjectPtr<AJTSPlanetAnchor> Planet;
		TWeakObjectPtr<const AActor> GoalActor;
		bool bActorGoal = false;
		FVector Home, Up, X, Y;
		float Radius = 0, Leash = 0, LastUsed = 0, LastBuild = -1000;
		FIntPoint Min, Max, GoalCell;
		TMap<FIntPoint, FNode> Nodes;
		TMap<FIntPoint, float> Costs, ReadyCosts;
		TSet<FIntPoint> Settled;
		TArray<FFrontier> Frontier;
		FFrontier Pending;
		int32 NextEdge = 0;
		bool bPending = false, bBuilding = false;

		FIntPoint ToCell(const FVector& Position) const
		{
			const FVector Radial = Planet->GetRadialUpVector(Position);
			const float Denominator = FMath::Max(0.1f, FVector::DotProduct(Radial, Up));
			return FIntPoint(FMath::RoundToInt(FVector::DotProduct(Radial, X) * Radius / Denominator / CellSize),
				FMath::RoundToInt(FVector::DotProduct(Radial, Y) * Radius / Denominator / CellSize));
		}
		bool Contains(const FIntPoint& Cell) const
		{
			return Cell.X >= Min.X && Cell.Y >= Min.Y && Cell.X <= Max.X && Cell.Y <= Max.Y;
		}
		void Begin(const FIntPoint& NewGoal)
		{
			GoalCell = NewGoal;
			Costs.Reset(); Settled.Reset(); Frontier.Reset();
			Costs.Add(GoalCell, 0.0f);
			Frontier.HeapPush(FFrontier{GoalCell, 0});
			bBuilding = true; bPending = false; NextEdge = 0;
		}
		bool Sample(UWorld* World, const FIntPoint& Cell, int32& Budget)
		{
			FNode& Node = Nodes.FindOrAdd(Cell);
			if (Node.bSampled) return true;
			if (Budget < 2) return false;
			Budget -= 2;
			Node.bSampled = true;
			FJTSPlanetSurfaceHit Hit;
			const FVector Candidate = Home + X * (Cell.X * CellSize) + Y * (Cell.Y * CellSize);
			if (!Planet->ProjectPointToSurface(Candidate, Hit)
				|| Planet->ApproximateSurfaceArcDistance(Home, Hit.ImpactPoint) > Leash) return true;
			Node.Ground = Hit.ImpactPoint; Node.Up = Hit.ImpactNormal.GetSafeNormal();
			if (FVector::DotProduct(Node.Up, Planet->GetRadialUpVector(Node.Ground)) < 0.65f) return true;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PlanetCrowdNode), false);
			Params.AddIgnoredActor(Planet->GetGameplaySurfaceActor());
			Node.bWalkable = !World->OverlapAnyTestByObjectType(Node.Ground + Node.Up * 65,
				FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(48), Params);
			return true;
		}
		bool Edge(UWorld* World, const FIntPoint& Cell, int32 Index, int32& Budget, bool& bOpen)
		{
			const FIntPoint Other = Cell + Neighbors[Index];
			bOpen = false;
			if (!Contains(Other)) return true;
			if (!Sample(World, Cell, Budget) || !Sample(World, Other, Budget)) return false;
			// A diagonal cannot squeeze through the corner of two blocked cardinal cells.
			if (Index >= 4)
			{
				const FIntPoint A = Cell + FIntPoint(Neighbors[Index].X, 0);
				const FIntPoint B = Cell + FIntPoint(0, Neighbors[Index].Y);
				if (!Sample(World, A, Budget) || !Sample(World, B, Budget)) return false;
				if (!Nodes.FindChecked(A).bWalkable || !Nodes.FindChecked(B).bWalkable) return true;
			}
			FNode& Node = Nodes.FindChecked(Cell);
			FNode& Next = Nodes.FindChecked(Other);
			if (!Node.bWalkable || !Next.bWalkable) return true;
			const uint8 Bit = 1 << Index;
			if (!(Node.KnownEdges & Bit))
			{
				if (Budget < 1) return false;
				--Budget;
				Node.KnownEdges |= Bit;
				Next.KnownEdges |= 1 << ReverseEdges[Index];
				FCollisionQueryParams Params(SCENE_QUERY_STAT(PlanetCrowdEdge), false);
				Params.AddIgnoredActor(Planet->GetGameplaySurfaceActor());
				FHitResult Hit;
				const FVector AverageUp = (Node.Up + Next.Up).GetSafeNormal();
				const bool bClear = FMath::Abs(FVector::DotProduct(Next.Ground - Node.Ground, AverageUp)) < 100
					&& !World->SweepSingleByObjectType(Hit, Node.Ground + Node.Up * 65, Next.Ground + Next.Up * 65,
						FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(48), Params);
				if (bClear) { Node.OpenEdges |= Bit; Next.OpenEdges |= 1 << ReverseEdges[Index]; }
			}
			bOpen = (Node.OpenEdges & Bit) != 0;
			return true;
		}
		void Integrate(UWorld* World, int32& Queries, int32& Expansions)
		{
			while (bBuilding && Expansions > 0)
			{
				if (!bPending)
				{
					if (Frontier.IsEmpty()) { ReadyCosts = Costs; bBuilding = false; break; }
					Frontier.HeapPop(Pending, EAllowShrinking::No);
					if (Settled.Contains(Pending.Cell) || Pending.Cost > Costs.FindChecked(Pending.Cell)) continue;
					bPending = true; NextEdge = 0;
				}
				if (!Sample(World, Pending.Cell, Queries)) return;
				if (!Nodes.FindChecked(Pending.Cell).bWalkable) { bPending = false; --Expansions; continue; }
				Settled.Add(Pending.Cell);
				for (; NextEdge < 8; ++NextEdge)
				{
					bool bOpen;
					if (!Edge(World, Pending.Cell, NextEdge, Queries, bOpen)) return;
					if (!bOpen) continue;
					const FIntPoint Other = Pending.Cell + Neighbors[NextEdge];
					if (Settled.Contains(Other)) continue;
					const float Cost = Pending.Cost + FVector::Distance(Nodes.FindChecked(Pending.Cell).Ground, Nodes.FindChecked(Other).Ground);
					float* Old = Costs.Find(Other);
					if (Old == nullptr || Cost < *Old)
					{
						Costs.Add(Other, Cost); Frontier.HeapPush(FFrontier{Other, Cost});
					}
				}
				bPending = false; --Expansions;
			}
		}
	};
}

struct FJTSPlanetCrowdNavigationImpl
{
	TArray<TUniquePtr<FCrowdRouteField>> Fields;
	int32 LastQueries = 0, Cursor = 0;
};

FJTSPlanetCrowdNavigation::FJTSPlanetCrowdNavigation() : Impl(MakeUnique<FJTSPlanetCrowdNavigationImpl>()) {}
FJTSPlanetCrowdNavigation::~FJTSPlanetCrowdNavigation() = default;

void FJTSPlanetCrowdNavigation::Tick(UWorld* World, float TimeSeconds, int32 PhysicsQueryBudget, int32 IntegrationBudget)
{
	Impl->Fields.RemoveAll([&](const TUniquePtr<FCrowdRouteField>& F)
		{ return !F->Planet.IsValid() || (F->bActorGoal && !F->GoalActor.IsValid()) || TimeSeconds - F->LastUsed > 8; });
	int32 Queries = FMath::Max(0, PhysicsQueryBudget);
	int32 Expansions = FMath::Max(0, IntegrationBudget);
	// Rotate priority and allocate slices so several players cannot starve each other's routes.
	for (int32 Pass = 0; Pass < Impl->Fields.Num() && Queries >= 2 && Expansions > 0; ++Pass)
	{
		FCrowdRouteField& Field = *Impl->Fields[(Impl->Cursor + Pass) % Impl->Fields.Num()];
		const int32 Slice = FMath::Min(Queries, FMath::Max(8, PhysicsQueryBudget / FMath::Max(1, Impl->Fields.Num())));
		int32 LocalQueries = Slice;
		Field.Integrate(World, LocalQueries, Expansions);
		Queries -= Slice - LocalQueries;
	}
	++Impl->Cursor;
	Impl->LastQueries = PhysicsQueryBudget - Queries;
}

FVector FJTSPlanetCrowdNavigation::GetDirection(AJTSPlanetAnchor* Planet, const FVector& Home,
	const FVector& Position, const FVector& Goal, const AActor* GoalActor, float RoamRadius, float LeashRadius, float TimeSeconds)
{
	if (!IsValid(Planet)) return FVector::ZeroVector;
	FCrowdRouteField* Field = nullptr;
	for (const TUniquePtr<FCrowdRouteField>& Candidate : Impl->Fields)
	{
		if (Candidate->Planet == Planet && Candidate->GoalActor == GoalActor && FVector::DistSquared(Candidate->Home, Home) < 100)
		{ Field = Candidate.Get(); break; }
	}
	if (!Field)
	{
		if (Impl->Fields.Num() >= MaxFields)
		{
			int32 Oldest = 0;
			for (int32 I = 1; I < Impl->Fields.Num(); ++I)
				if (Impl->Fields[I]->LastUsed < Impl->Fields[Oldest]->LastUsed) Oldest = I;
			Impl->Fields.RemoveAtSwap(Oldest);
		}
		TUniquePtr<FCrowdRouteField> New = MakeUnique<FCrowdRouteField>();
		New->Planet = Planet; New->GoalActor = GoalActor; New->bActorGoal = GoalActor != nullptr;
		New->Home = Home; New->Up = Planet->GetRadialUpVector(Home);
		New->Up.FindBestAxisVectors(New->X, New->Y);
		New->Radius = FVector::Distance(Home, Planet->GetPlanetCenter()); New->Leash = LeashRadius;
		Field = New.Get(); Impl->Fields.Add(MoveTemp(New));
	}
	Field->LastUsed = TimeSeconds;
	if (!FMath::IsNearlyEqual(Field->Leash, LeashRadius))
	{
		Field->Leash = LeashRadius;
		Field->Nodes.Reset(); Field->ReadyCosts.Reset();
	}
	const FIntPoint GoalCell = Field->ToCell(Goal);
	const FIntPoint PositionCell = Field->ToCell(Position);
	if (Field->Nodes.IsEmpty() || !Field->Contains(GoalCell) || !Field->Contains(PositionCell))
	{
		const int32 Margin = FMath::CeilToInt((FMath::Max(600.0f, RoamRadius) + 500) / CellSize);
		const FIntPoint NewMin(FMath::Min3(0, GoalCell.X, PositionCell.X) - Margin, FMath::Min3(0, GoalCell.Y, PositionCell.Y) - Margin);
		const FIntPoint NewMax(FMath::Max3(0, GoalCell.X, PositionCell.X) + Margin, FMath::Max3(0, GoalCell.Y, PositionCell.Y) + Margin);
		Field->Min = Field->Nodes.IsEmpty() ? NewMin : FIntPoint(FMath::Min(Field->Min.X, NewMin.X), FMath::Min(Field->Min.Y, NewMin.Y));
		Field->Max = Field->Nodes.IsEmpty() ? NewMax : FIntPoint(FMath::Max(Field->Max.X, NewMax.X), FMath::Max(Field->Max.Y, NewMax.Y));
		if ((int64(Field->Max.X) - Field->Min.X + 1) * (int64(Field->Max.Y) - Field->Min.Y + 1) > MaxCells)
			return FVector::ZeroVector;
		Field->Nodes.Reset(); Field->ReadyCosts.Reset(); Field->Begin(GoalCell); Field->LastBuild = TimeSeconds;
	}
	else if (GoalCell != Field->GoalCell && !Field->bBuilding && TimeSeconds - Field->LastBuild > 0.75f)
	{
		// Preserve the last completed field until the moving goal's replacement reaches this cell.
		Field->Begin(GoalCell); Field->LastBuild = TimeSeconds;
	}
	const FIntPoint Cell = Field->ToCell(Position);
	if (Cell == GoalCell) return FVector::VectorPlaneProject(Goal - Position, Planet->GetRadialUpVector(Position)).GetSafeNormal();
	const bool bCurrent = Field->Settled.Contains(Cell);
	const TMap<FIntPoint, float>& Costs = bCurrent ? Field->Costs : Field->ReadyCosts;
	const float* Cost = Costs.Find(Cell);
	const FNode* Node = Field->Nodes.Find(Cell);
	if (!Cost || !Node) return FVector::ZeroVector;
	float Best = *Cost;
	FVector Direction = FVector::ZeroVector;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		if (!(Node->OpenEdges & (1 << Index))) continue;
		const FIntPoint Other = Cell + Neighbors[Index];
		if (bCurrent && !Field->Settled.Contains(Other)) continue;
		if (const float* NextCost = Costs.Find(Other); NextCost && *NextCost < Best)
		{
			Best = *NextCost; Direction = Field->Nodes.FindChecked(Other).Ground - Position;
		}
	}
	// Consumers already own their actual mesh normal. Do not perform another surface trace per consumer.
	return FVector::VectorPlaneProject(Direction, Planet->GetRadialUpVector(Position)).GetSafeNormal();
}

int32 FJTSPlanetCrowdNavigation::GetLastPhysicsQueryCount() const { return Impl->LastQueries; }
int32 FJTSPlanetCrowdNavigation::GetReadyFieldCount() const
{
	int32 Count = 0;
	for (const auto& Field : Impl->Fields) if (!Field->ReadyCosts.IsEmpty()) ++Count;
	return Count;
}
