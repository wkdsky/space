#include "space/Components/JTSNestBlockadeComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSMeleeComponent.h"
#include "space/Components/JTSMiningStrikeComponent.h"
#include "space/Components/JTSMiningImpactComponent.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

UJTSNestBlockadeComponent::UJTSNestBlockadeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
bool UJTSNestBlockadeComponent::HasRequiredTool(APawn* Pawn) const
{
	const auto* Inventory = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	return Inventory && Inventory->GetActiveItemId() == EJTSItemId::Pickaxe;
}
bool UJTSNestBlockadeComponent::CanWork(APawn* Pawn) const
{
	const auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	if (!Habitat || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld() || !HasRequiredTool(Pawn)
		|| !Habitat->CanInteract_Implementation(Pawn)) return false;
	if (const auto* Character = Cast<AJTSCharacter>(Pawn); Character && Character->IsBoarded()) return false;
	if (const auto* Climb = Pawn->FindComponentByClass<UJTSWallClimbComponent>(); Climb && Climb->IsClimbing()) return false;
	const auto* Strike = Pawn->FindComponentByClass<UJTSMiningStrikeComponent>();
	FHitResult Contact;
	if (!Strike || !Strike->FindWorkContact(const_cast<AJTSPlanetEcologyHabitat*>(Habitat), Contact)) return false;
	const FVector Target = Habitat->GetInteractionAnchorWorldLocation();
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target) > FMath::Square(410.0f)) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NestWork), false, Pawn);
	Params.AddIgnoredActor(Habitat->GetEntranceAttachment());
	for (const auto Defender : Defenders) if (Defender.IsValid()) Params.AddIgnoredActor(Defender.Get());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Target, ECC_Visibility, Params)
		|| Hit.GetActor() == Habitat;
}
void UJTSNestBlockadeComponent::SetWorking(APawn* Pawn, bool bHeld)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Pawn)) return;
	if (!bHeld)
	{
		if (auto* Worker = Workers.Find(Pawn)) Worker->bHeld = false;
		return;
	}
	if (!CanWork(Pawn)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	auto& Worker = Workers.FindOrAdd(Pawn);
	Worker.bHeld = true;
	Worker.PulseUntil = Now + TapWorkDuration;
	if (!GetWorld()->GetTimerManager().IsTimerActive(WorkTimer))
	{
		LastUpdate = Now;
		GetWorld()->GetTimerManager().SetTimer(WorkTimer, this, &ThisClass::ServiceWork, 0.05f, true);
	}
	if (auto* Melee = Pawn->FindComponentByClass<UJTSMeleeComponent>(); Melee && !Melee->IsAttacking() && Now >= Worker.NextSwing)
	{
		Melee->StartWorkAttack(GetOwner()); Worker.NextSwing = Now + Melee->GetHeldWeaponSwingDuration();
	}
	DefenseAggressors.AddUnique(Pawn);
	if (!bAlarmed) StartDefense();
}
void UJTSNestBlockadeComponent::ServiceWork()
{
	if (!GetOwner()->HasAuthority()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Dt = FMath::Max(float(Now - LastUpdate), 0.0f); LastUpdate = Now;
	auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	float Work = 0;
	for (auto It = Workers.CreateIterator(); It; ++It)
	{
		APawn* Pawn = It.Key().Get(); auto& Worker = It.Value();
		if (!CanWork(Pawn)) { It.RemoveCurrent(); continue; }
		const float Contribution = Worker.bHeld ? Dt : FMath::Clamp(float(Worker.PulseUntil - (Now - Dt)), 0.0f, Dt);
		if (Contribution > 0)
		{
			Work += Contribution;
			Worker.UnspentWork += Contribution;
		}
		// A tap can end during windup. Keep its earned work until that strike lands.
		if (Contribution > 0 || Worker.UnspentWork > 0)
		{
			if (auto* Melee = Pawn->FindComponentByClass<UJTSMeleeComponent>(); Melee && !Melee->IsAttacking() && Now >= Worker.NextSwing)
			{
				Melee->StartWorkAttack(GetOwner()); Worker.NextSwing = Now + Melee->GetHeldWeaponSwingDuration();
			}
		}
		else if (!Worker.bHeld) It.RemoveCurrent();
	}
	if (Work > 0)
	{
		LastWork = Now;
	}
	else if (Now - LastWork > DecayDelay) Progress = FMath::Max(0.0f, Progress - Dt * DecayPerSecond);
	if (Progress >= 1 && !Habitat->IsEntranceBlocked())
	{
		// The only commit path is this server clock; direct Interact/RPC calls cannot bypass the work.
		Habitat->CommitEntranceBlockade(true); Workers.Reset();
		if (auto* GameInstance = GetWorld()->GetGameInstance())
			if (auto* Expedition = GameInstance->GetSubsystem<UJTSExpeditionSubsystem>())
				Expedition->MarkEntranceBlocked(PersistentEntranceId);
		MulticastCollapseDust();
		if (auto* Impact = GetOwner()->FindComponentByClass<UJTSMiningImpactComponent>()) Impact->ConfirmBreak();
		GetWorld()->GetTimerManager().SetTimer(RubbleTimer, FTimerDelegate::CreateWeakLambda(Habitat,
			[Habitat] { Habitat->ShowBlockedRubble(); }), FMath::Max(0.01f, RubbleDelay), false);
	}
	UpdateDefense();
	if (Progress <= 0 && Workers.IsEmpty() && !bAlarmed) GetWorld()->GetTimerManager().ClearTimer(WorkTimer);
}
bool UJTSNestBlockadeComponent::ApplyStrikeWork(APawn* Pawn)
{
	if (!GetOwner()->HasAuthority() || !CanWork(Pawn)) return false;
	auto* Worker = Workers.Find(Pawn);
	if (!Worker || Worker->UnspentWork <= 0) return false;
	const auto* Melee = Pawn->FindComponentByClass<UJTSMeleeComponent>();
	const float MaximumWork = Melee ? Melee->GetHeldWeaponSwingDuration()+.06f : .4f;
	const auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	Progress = FMath::Min(1.f,Progress+FMath::Min(Worker->UnspentWork,MaximumWork)/FMath::Max(.5f,Habitat->BlockHoldDuration));
	Worker->UnspentWork=0;
	LastWork=GetWorld()->GetTimeSeconds();
	return true;
}
void UJTSNestBlockadeComponent::StartDefense()
{
	bAlarmed = true;
	auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	for (TActorIterator<AJTSPlanetSurfaceCreature> It(GetWorld()); It; ++It)
		if (It->BelongsToRefuge(Habitat) && It->IsAlive() && Defenders.Num() < DefensePopulation) Defenders.AddUnique(*It);
	PendingDefenders = FMath::Max(0, DefensePopulation - Defenders.Num());
	DefenderSpawnSequence = 0;
	NextDefenderEmergence = GetWorld()->GetTimeSeconds();
	UpdateDefense();
}
void UJTSNestBlockadeComponent::UpdateDefense()
{
	if (!bAlarmed) return;
	auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	ServicePopulation();
	Defenders.RemoveAll([](const auto& Defender) { return !Defender.IsValid() || !Defender->IsAlive(); });
	if (bFinitePopulation)
	{
		for (TActorIterator<AJTSPlanetSurfaceCreature> It(GetWorld()); It && Defenders.Num() < DefensePopulation; ++It)
			if (It->BelongsToRefuge(Habitat) && It->IsAlive()) Defenders.AddUnique(*It);
		PendingDefenders = FMath::Min(GetReservePopulation(), FMath::Max(0, DefensePopulation - Defenders.Num()));
	}
	const FVector Origin = Habitat->GetInteractionAnchorWorldLocation();
	APawn* Target = FindDefenseAggressor();
	const double Now = GetWorld()->GetTimeSeconds();
	// Catch up after a slow frame while retaining distinct mouth lanes and release times at normal frame rates.
	for (int32 Released = 0; Target && !Habitat->IsEntranceBlocked() && PendingDefenders > 0
		&& Now >= NextDefenderEmergence && Released < 3 && (bFinitePopulation || DefenderSpawnSequence < DefensePopulation * 4); ++Released)
	{
		if (auto* Creature = Habitat->SpawnEntranceDefender(DefenderSpawnSequence++))
		{
			Defenders.Add(Creature); --PendingDefenders;
		}
		else NextDefenderEmergence = Now;
		NextDefenderEmergence += FMath::Max(0.08f, DefenderEmergenceInterval);
	}
	for (int32 Slot = 0; Slot < Defenders.Num(); ++Slot) if (auto Defender = Defenders[Slot]; Defender.IsValid())
	{
		if (Target) Defender->DefendEntrance(Target, Origin, DefenseLeashDistance, Slot, FMath::Max(DefensePopulation, Defenders.Num()));
		else Defender->ReturnFromEntranceDefense(Habitat->IsEntranceBlocked());
	}
	if (!Target) { Defenders.Reset(); PendingDefenders = 0; bAlarmed = false; }
}

APawn* UJTSNestBlockadeComponent::FindDefenseAggressor()
{
	if (!bAlarmed || !GetOwner()->HasAuthority()) return nullptr;
	const auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	const FVector Origin = Habitat->GetInteractionAnchorWorldLocation();
	// Only workers that provoked this nest retain threat. Leaving the nest's
	// radius forgets that player, even if they return while another player fights.
	DefenseAggressors.RemoveAll([this, Origin](const auto& Candidate)
	{
		APawn* Pawn = Candidate.Get();
		if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(), Origin) > FMath::Square(DefenseLeashDistance)) return true;
		const auto* Health = Pawn->FindComponentByClass<UJTSHealthComponent>();
		return Health && Health->IsDead();
	});
	APawn* Target = nullptr; double Best = TNumericLimits<double>::Max();
	for (const auto& Candidate : DefenseAggressors)
	{
		const double Distance = FVector::DistSquared(Candidate->GetActorLocation(), Origin);
		if (Distance < Best) { Target = Candidate.Get(); Best = Distance; }
	}
	return Target;
}

void UJTSNestBlockadeComponent::ServicePopulation()
{
	if (!bFinitePopulation || !GetOwner()->HasAuthority()) return;
	if (!bPopulationInitialized)
	{
		RemainingPopulation = FMath::Clamp(PopulationCapacity, 0, 512);
		bPopulationInitialized = true;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const int32 Lost = Residents.RemoveAll([](const auto& Resident) { return !Resident.IsValid() || !Resident->IsAlive(); });
	ResidentPopulation = Residents.Num();
	if (Lost > 0)
	{
		RemainingPopulation = FMath::Max(0, RemainingPopulation - Lost);
		NextPopulationRecovery = Now + PopulationRecoveryDelay;
	}
	const auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	if (Habitat && !Habitat->IsEntranceBlocked() && RemainingPopulation < PopulationCapacity
		&& NextPopulationRecovery > 0 && Now >= NextPopulationRecovery)
	{
		// Recover one resident per interval. An old timer never creates a catch-up wave during a fight.
		++RemainingPopulation;
		NextPopulationRecovery = Now + FMath::Max(1.f, PopulationRecoveryInterval);
	}
}

bool UJTSNestBlockadeComponent::CanReleaseResident()
{
	if (!GetOwner()->HasAuthority()) return false;
	if (!bFinitePopulation) return true;
	ServicePopulation();
	const auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	return Habitat && !Habitat->IsEntranceBlocked() && GetReservePopulation() > 0;
}

bool UJTSNestBlockadeComponent::TryRegisterResident(AJTSPlanetSurfaceCreature* Creature)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Creature)) return false;
	if (!bFinitePopulation) return true;
	ServicePopulation();
	if (Residents.Contains(Creature)) return true;
	const auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(GetOwner());
	if (!Habitat || Habitat->IsEntranceBlocked() || GetReservePopulation() <= 0) return false;
	Residents.Add(Creature);
	ResidentPopulation = Residents.Num();
	return true;
}

void UJTSNestBlockadeComponent::UnregisterResident(AJTSPlanetSurfaceCreature* Creature, bool bKilled)
{
	if (!GetOwner()->HasAuthority() || !bFinitePopulation || Residents.Remove(Creature) <= 0) return;
	ResidentPopulation = Residents.Num();
	if (bKilled)
	{
		RemainingPopulation = FMath::Max(0, RemainingPopulation - 1);
		NextPopulationRecovery = GetWorld()->GetTimeSeconds() + FMath::Max(1.f, PopulationRecoveryDelay);
	}
}
void UJTSNestBlockadeComponent::RestorePersistentState()
{
	if (!GetOwner()->HasAuthority() || PersistentEntranceId.IsNone()) return;
	if (const auto* GameInstance = GetWorld()->GetGameInstance())
		if (const auto* Expedition = GameInstance->GetSubsystem<UJTSExpeditionSubsystem>(); Expedition && Expedition->IsEntranceBlocked(PersistentEntranceId))
			CastChecked<AJTSPlanetEcologyHabitat>(GetOwner())->CommitEntranceBlockade();
}
void UJTSNestBlockadeComponent::MulticastCollapseDust_Implementation()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer || !CollapseDust) return;
	const auto* Habitat = CastChecked<AJTSPlanetEcologyHabitat>(GetOwner());
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CollapseDust, Habitat->GetInteractionAnchorWorldLocation(),
		Habitat->GetActorRotation(), DustScale);
}
void UJTSNestBlockadeComponent::ResetTransientState()
{
	if (!GetOwner()->HasAuthority()) return;
	for (auto Defender : Defenders) if (Defender.IsValid()) Defender->ReturnFromEntranceDefense(false);
	Defenders.Reset(); DefenseAggressors.Reset(); Workers.Reset(); Progress = 0; PendingDefenders = 0; bAlarmed = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(WorkTimer);
}
void UJTSNestBlockadeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorkTimer);
		World->GetTimerManager().ClearTimer(RubbleTimer);
	}
	Super::EndPlay(Reason);
}
void UJTSNestBlockadeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UJTSNestBlockadeComponent, Progress);
	DOREPLIFETIME(UJTSNestBlockadeComponent, RemainingPopulation);
	DOREPLIFETIME(UJTSNestBlockadeComponent, ResidentPopulation);
	DOREPLIFETIME(UJTSNestBlockadeComponent, bPopulationInitialized);
}
