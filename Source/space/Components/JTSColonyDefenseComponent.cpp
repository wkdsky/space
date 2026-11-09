#include "space/Components/JTSColonyDefenseComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEnemySettlement.h"

namespace
{
	int32 ChooseCount(FIntPoint Range)
	{
		return FMath::RandRange(FMath::Max(0, Range.X), FMath::Max3(0, Range.X, Range.Y));
	}
	int32 Living(const TArray<TWeakObjectPtr<AJTSPlanetSurfaceCreature>>& Residents, bool bOutsideOnly)
	{
		return Residents.FilterByPredicate([bOutsideOnly](const auto& Resident)
		{
			return Resident.IsValid() && Resident->IsAlive() && (!bOutsideOnly || !Resident->IsShelteringInColony());
		}).Num();
	}
}

UJTSColonyDefenseComponent::UJTSColonyDefenseComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UJTSColonyDefenseComponent::Initialize(AJTSPlanetAnchor* Planet, AJTSPlanetEnemySettlement* InWorkers, AJTSPlanetEnemySettlement* InSoldiers)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Planet)) return;
	ActivePlanet = Planet; WorkerSettlement = InWorkers; SoldierSettlement = InSoldiers;
	RoamingTarget = ChooseCount(RoamingWorkers);
	GetWorld()->GetTimerManager().SetTimer(ServiceTimer, this, &ThisClass::ServiceColony, 0.2f, true);
}
void UJTSColonyDefenseComponent::RegisterResident(AJTSPlanetSurfaceCreature* Creature, const AJTSPlanetEnemySettlement* Settlement)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Creature)) return;
	if (Settlement == WorkerSettlement.Get()) Workers.AddUnique(Creature);
	else if (Settlement == SoldierSettlement.Get()) Soldiers.AddUnique(Creature);
	else return;
	Creature->ShelterInColony();
}
int32 UJTSColonyDefenseComponent::GetLivingWorkers() const { return Living(Workers, false); }
int32 UJTSColonyDefenseComponent::GetLivingSoldiers() const { return Living(Soldiers, false); }
int32 UJTSColonyDefenseComponent::GetOutsideWorkers() const { return Living(Workers, true); }
int32 UJTSColonyDefenseComponent::GetOutsideSoldiers() const { return Living(Soldiers, true); }

FVector UJTSColonyDefenseComponent::EntranceFor(const AJTSPlanetSurfaceCreature* Creature) const
{
	FVector Result = GetOwner()->GetActorLocation();
	double Best = TNumericLimits<double>::Max();
	for (const FVector& Offset : EntranceOffsets)
	{
		FJTSPlanetSurfaceHit Hit;
		if (!ActivePlanet->ProjectPointToSurface(GetOwner()->GetActorTransform().TransformPosition(Offset), Hit)) continue;
		const double Distance = Creature ? FVector::DistSquared(Creature->GetActorLocation(), Hit.ImpactPoint) : 0;
		if (Distance < Best) { Result = Hit.ImpactPoint; Best = Distance; }
	}
	return Result;
}
bool UJTSColonyDefenseComponent::Release(AJTSPlanetSurfaceCreature* Creature)
{
	if (!Creature || !Creature->IsInsideColony() || EntranceOffsets.IsEmpty()) return false;
	const FVector Offset = EntranceOffsets[EntranceSequence % EntranceOffsets.Num()];
	FJTSPlanetSurfaceHit Mouth, Exit;
	if (!ActivePlanet->ProjectPointToSurface(GetOwner()->GetActorTransform().TransformPosition(Offset), Mouth)) return false;
	const FVector Forward = ActivePlanet->ProjectDirectionToSurfaceTangent(
		GetOwner()->GetActorTransform().TransformVectorNoScale(Offset), Mouth.ImpactPoint).GetSafeNormal();
	if (!ActivePlanet->ProjectPointToSurface(Mouth.ImpactPoint + Forward * ExitDistance, Exit)) return false;
	FJTSPlanetSurfaceFrame Frame;
	if (!ActivePlanet->GetSurfaceFrameAt(Mouth.ImpactPoint, Forward, Frame)) return false;
	Creature->LeaveColony(Frame.Transform, Exit.ImpactPoint);
	++EntranceSequence;
	return true;
}
void UJTSColonyDefenseComponent::RaiseAlarm(APawn* Attacker)
{
	if (!GetOwner()->HasAuthority() || !ActivePlanet.IsValid() || !IsValid(Attacker) || Attacker->GetWorld() != GetWorld()
		|| FVector::DistSquared(Attacker->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(DefenseLeash)) return;
	if (const auto* Health = Attacker->FindComponentByClass<UJTSHealthComponent>(); Health && Health->IsDead()) return;
	Aggressors.AddUnique(Attacker);
	if (!bAlarmed)
	{
		bAlarmed = true;
		SoldierTarget = ChooseCount(FightingSoldiers);
		WorkerDefenders.Reset();
		const int32 Count = ChooseCount(FightingWorkers);
		for (const auto& Worker : Workers)
			if (Worker.IsValid() && Worker->IsAlive() && !Worker->IsShelteringInColony() && WorkerDefenders.Num() < Count)
				WorkerDefenders.Add(Worker);
	}
	ServiceColony();
}
void UJTSColonyDefenseComponent::ServiceColony()
{
	if (!GetOwner()->HasAuthority() || !ActivePlanet.IsValid()) return;
	auto Prune = [](auto& Residents) { Residents.RemoveAll([](const auto& C) { return !C.IsValid() || !C->IsAlive(); }); };
	Prune(Workers); Prune(Soldiers); Prune(WorkerDefenders);
	const FVector Origin = GetOwner()->GetActorLocation();
	Aggressors.RemoveAll([this, Origin](const auto& Candidate)
	{
		const auto* Pawn = Candidate.Get();
		const auto* Health = Pawn ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
		return !Pawn || (Health && Health->IsDead()) || FVector::DistSquared(Pawn->GetActorLocation(), Origin) > FMath::Square(DefenseLeash);
	});
	APawn* Target = Aggressors.IsEmpty() ? nullptr : Aggressors[0].Get();
	if (!Target && bAlarmed)
	{
		bAlarmed = false;
		for (const auto& Worker : WorkerDefenders) Worker->RecallToColony(EntranceFor(Worker.Get()));
		WorkerDefenders.Reset();
		for (const auto& Soldier : Soldiers) Soldier->RecallToColony(EntranceFor(Soldier.Get()));
	}
	if (Target)
	{
		for (const auto& Worker : Workers)
		{
			if (WorkerDefenders.Contains(Worker)) Worker->DefendEntrance(Target, Origin, DefenseLeash, WorkerDefenders.IndexOfByKey(Worker), SoldierTarget + WorkerDefenders.Num());
			else Worker->RecallToColony(EntranceFor(Worker.Get()));
		}
		for (const auto& Soldier : Soldiers)
			if (!Soldier->IsShelteringInColony()) Soldier->DefendEntrance(Target, Origin, DefenseLeash, WorkerDefenders.Num() + Soldiers.IndexOfByKey(Soldier), SoldierTarget + WorkerDefenders.Num());
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextEmergence) return;
	auto& Roster = Target ? Soldiers : Workers;
	int32 Outside = Target ? GetOutsideSoldiers() : GetOutsideWorkers();
	const int32 Desired = Target ? SoldierTarget : RoamingTarget;
	if (!Target && Outside > Desired)
	{
		for (const auto& Worker : Workers)
			if (Outside > Desired && !Worker->IsShelteringInColony()) { Worker->RecallToColony(EntranceFor(Worker.Get())); --Outside; }
	}
	if (Outside >= Desired) return;
	for (const auto& Resident : Roster)
	{
		if (!Release(Resident.Get())) continue;
		if (Target) Resident->DefendEntrance(Target, Origin, DefenseLeash, WorkerDefenders.Num() + Outside, SoldierTarget + WorkerDefenders.Num());
		NextEmergence = Now + FMath::Max(0.1f, EmergenceInterval);
		break;
	}
}
void UJTSColonyDefenseComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ServiceTimer);
	Super::EndPlay(Reason);
}
