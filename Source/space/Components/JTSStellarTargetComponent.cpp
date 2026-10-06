#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UJTSStellarTargetComponent::UJTSStellarTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UJTSStellarTargetComponent::IsAliveTarget() const
{
	const auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	const auto* Pawn = Cast<APawn>(GetOwner());
	return Health && !Health->IsDead() && !(Pawn && Pawn->IsPlayerControlled());
}

void UJTSStellarTargetComponent::QueryTargets(UWorld* World, const FVector& Center, float Radius,
	int32 Limit, TArray<AActor*>& Out)
{
	Out.Reset();
	if (!World || Radius <= 0 || Limit <= 0) return;
	TArray<FOverlapResult> Hits;
	World->OverlapMultiByObjectType(Hits, Center, FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects, FCollisionShape::MakeSphere(Radius));
	TSet<AActor*> Seen;
	for (const auto& Hit : Hits)
	{
		AActor* Actor = Hit.GetActor();
		const auto* Target = IsValid(Actor) ? Actor->FindComponentByClass<UJTSStellarTargetComponent>() : nullptr;
		if (Target && Target->IsAliveTarget() && !Seen.Contains(Actor)) { Seen.Add(Actor); Out.Add(Actor); }
	}
	Out.Sort([&](const AActor& A, const AActor& B) {
		return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center); });
	if (Out.Num() > Limit) Out.SetNum(Limit);
}

bool UJTSStellarTargetComponent::HasLineOfSight(UWorld* World, const FVector& From, AActor* Target, AActor* Source)
{
	if (!World || !IsValid(Target)) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarVisibility), false, Source);
	// Enemy bodies must not occlude an area weapon; static geometry still does.
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByObjectType(Hit, From, Target->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	return !bBlocked || Hit.GetActor() == Target;
}

void UJTSStellarTargetComponent::WakeStatusTimer()
{
	if (!GetWorld()->GetTimerManager().IsTimerActive(StatusTimer))
		GetWorld()->GetTimerManager().SetTimer(StatusTimer, this, &ThisClass::StatusPulse, 0.2f, true);
}

void UJTSStellarTargetComponent::ApplyFire(APawn* Source, float Heat, float BurnDPS, int32 SpreadTargets)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Source) || !IsAliveTarget() || !FMath::IsFinite(Heat) || Heat < 0) return;
	const double Now = GetWorld()->GetTimeSeconds();
	FJTSStellarDamageSource* Entry = DamageSources.FindByPredicate([&](const auto& S){ return S.Pawn == Source; });
	if (!Entry) { Entry = &DamageSources.AddDefaulted_GetRef(); Entry->Pawn = Source; }
	Entry->FireDPS = FMath::Max(0.0f, BurnDPS);
	Entry->FireEnd = Now + 2.5;
	HeatAmount += Heat;
	LastHeatTime = Now;
	if (!bIgnited && HeatAmount >= IgnitionThreshold)
	{
		bIgnited = true;
		IgnitionEnd = Now + 2.0;
		Igniter = Source;
		TArray<AActor*> Neighbors;
		QueryTargets(GetWorld(), GetOwner()->GetActorLocation(), 300.0f, FMath::Clamp(SpreadTargets, 0, 5) + 1, Neighbors);
		int32 Remaining = FMath::Clamp(SpreadTargets, 0, 5);
		for (AActor* Actor : Neighbors) if (Actor != GetOwner() && Remaining > 0
			&& HasLineOfSight(GetWorld(), GetOwner()->GetActorLocation(), Actor, Source))
		{
			Actor->FindComponentByClass<UJTSStellarTargetComponent>()->ApplyFire(Source, IgnitionThreshold * 0.35f, BurnDPS * 0.35f, 0);
			--Remaining;
		}
	}
	WakeStatusTimer();
}

void UJTSStellarTargetComponent::ApplyLightBurn(APawn* Source, float DPS, float Duration,
	int32 SpreadTargets, float SpreadScale)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Source) || !IsAliveTarget() || !FMath::IsFinite(DPS) || DPS <= 0) return;
	FJTSStellarDamageSource* Entry = DamageSources.FindByPredicate([&](const auto& S){ return S.Pawn == Source; });
	if (!Entry) { Entry = &DamageSources.AddDefaulted_GetRef(); Entry->Pawn = Source; }
	Entry->LightDPS = DPS;
	Entry->LightEnd = GetWorld()->GetTimeSeconds() + FMath::Clamp(Duration, 0.2f, 5.0f);
	bLightBurning = true;
	if (SpreadTargets > 0 && SpreadScale > 0)
	{
		TArray<AActor*> Neighbors;
		QueryTargets(GetWorld(), GetOwner()->GetActorLocation(), 300.0f, SpreadTargets + 1, Neighbors);
		for (AActor* Actor : Neighbors) if (Actor != GetOwner()
			&& HasLineOfSight(GetWorld(), GetOwner()->GetActorLocation(), Actor, Source))
			Actor->FindComponentByClass<UJTSStellarTargetComponent>()->ApplyLightBurn(Source,
				DPS * FMath::Clamp(SpreadScale, 0.0f, 0.5f), Duration, 0, 0);
	}
	WakeStatusTimer();
}

void UJTSStellarTargetComponent::ExecuteIgnition(APawn* Source)
{
	auto* Health = GetOwner()->FindComponentByClass<UJTSHealthComponent>();
	if (!Health || Health->IsDead()) return;
	const bool bExecute = Tier == EJTSStellarTargetTier::Normal
		|| (Tier == EJTSStellarTargetTier::Elite && Health->GetHealthNormalized() <= 0.15f);
	UGameplayStatics::ApplyDamage(GetOwner(), bExecute ? Health->GetHealth() : ResistantIgnitionDamage,
		Source ? Source->GetController() : nullptr, Source, UDamageType::StaticClass());
	if (Tier == EJTSStellarTargetTier::Boss) PosturePressure = FMath::Min(100.0f, PosturePressure + 20.0f);
}

void UJTSStellarTargetComponent::StatusPulse()
{
	if (!IsAliveTarget())
	{
		DamageSources.Reset(); Forces.Reset(); bIgnited = false; bLightBurning = false;
		GetWorld()->GetTimerManager().ClearTimer(StatusTimer); return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (bIgnited && Now >= IgnitionEnd)
	{
		bIgnited = false; HeatAmount = 0;
		ExecuteIgnition(Igniter.Get());
		if (!IsAliveTarget()) { StatusPulse(); return; }
	}
	if (Now > LastHeatTime + 1.0) HeatAmount = FMath::Max(0.0f, HeatAmount - 4.0f);
	DamageSources.RemoveAll([&](const auto& S){ return !S.Pawn.IsValid() || (S.FireEnd <= Now && S.LightEnd <= Now); });
	// Each element has its own three-source cap. Refreshing does not add another source from that player.
	for (bool bFire : {true, false})
	{
		TArray<FJTSStellarDamageSource> Active;
		for (const auto& S : DamageSources) if ((bFire ? S.FireEnd : S.LightEnd) > Now) Active.Add(S);
		Active.Sort([&](const auto& A, const auto& B){ return (bFire ? A.FireDPS : A.LightDPS) > (bFire ? B.FireDPS : B.LightDPS); });
		for (int32 Index = 0; Index < FMath::Min(3, Active.Num()) && IsAliveTarget(); ++Index)
		{
			const auto& S = Active[Index];
			APawn* Source = S.Pawn.Get();
			UGameplayStatics::ApplyDamage(GetOwner(), (bFire ? S.FireDPS : S.LightDPS) * 0.2f,
				Source->GetController(), Source, UDamageType::StaticClass());
		}
	}
	bLightBurning = DamageSources.ContainsByPredicate([&](const auto& S){ return S.LightEnd > Now; });
	Forces.RemoveAll([&](const auto& S){ return !S.Pawn.IsValid() || !S.Field.IsValid() || S.Field->IsActorBeingDestroyed() || S.End <= Now; });
	if (!bIgnited && HeatAmount <= 0 && DamageSources.IsEmpty() && Forces.IsEmpty())
		GetWorld()->GetTimerManager().ClearTimer(StatusTimer);
}

void UJTSStellarTargetComponent::ApplyAttraction(APawn* Source, AActor* Field, const FVector& Center,
	float Acceleration, float Radius, float SofteningRadius, float Duration)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Source) || !IsValid(Field) || !IsAliveTarget()
		|| Center.ContainsNaN() || !FMath::IsFinite(Acceleration) || !FMath::IsFinite(Radius) || Radius <= 0) return;
	if (Tier == EJTSStellarTargetTier::Boss)
	{
		PosturePressure = FMath::Min(100.0f, PosturePressure + 0.5f);
		return;
	}
	FJTSStellarForceSource* Entry = Forces.FindByPredicate([&](const auto& S){ return S.Field == Field; });
	if (!Entry) { Entry = &Forces.AddDefaulted_GetRef(); Entry->Pawn = Source; Entry->Field = Field; }
	Entry->Center = Center;
	Entry->Strength = FMath::Clamp(Acceleration, 0.0f, 24000.0f) * (Tier == EJTSStellarTargetTier::Elite ? 0.4f : 1.0f);
	Entry->Radius = Radius;
	Entry->SofteningRadius = FMath::Clamp(SofteningRadius, 0.0f, Radius);
	Entry->bRepulsion = false;
	Entry->End = GetWorld()->GetTimeSeconds() + FMath::Clamp(Duration, 0.05f, 0.5f);
	WakeStatusTimer();
}

void UJTSStellarTargetComponent::ApplyRepulsion(APawn* Source, float Stiffness, float Radius, float Duration,
	float EntryDepth, float ExitOffset)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Source) || Stiffness <= 0) return;
	if (auto* Previous = Forces.FindByPredicate([&](const auto& S){ return S.Field == Source; }))
		if (!IsActiveForce(*Previous)) Previous->bRepulsionEngaged = false;
	ApplyAttraction(Source, Source, Source ? Source->GetActorLocation() : FVector::ZeroVector,
		Stiffness, Radius, 0, Duration);
	if (auto* Entry = Forces.FindByPredicate([&](const auto& S){ return S.Field == Source; }))
	{
		Entry->bRepulsion = true;
		Entry->EntryDepth = FMath::Clamp(EntryDepth, 0.0f, Radius * 0.2f);
		Entry->ExitOffset = FMath::Clamp(ExitOffset, 0.0f, 50.0f);
	}
}

void UJTSStellarTargetComponent::RemoveForce(APawn* Source)
{
	RemoveFieldForce(Source);
}

void UJTSStellarTargetComponent::RemoveFieldForce(AActor* Field)
{
	Forces.RemoveAll([&](const auto& Entry){ return Entry.Field == Field; });
}

bool UJTSStellarTargetComponent::IsActiveForce(const FJTSStellarForceSource& S) const
{
	return S.Pawn.IsValid() && S.Field.IsValid() && !S.Field->IsActorBeingDestroyed()
		&& S.End > GetWorld()->GetTimeSeconds();
}

bool UJTSStellarTargetComponent::HasActiveFieldForces() const
{
	return Forces.ContainsByPredicate([&](const auto& S){ return IsActiveForce(S); });
}

void UJTSStellarTargetComponent::GetRepulsionCasters(TArray<AActor*, TInlineAllocator<4>>& Out) const
{
	Out.Reset();
	for (const auto& S : Forces) if (S.bRepulsion && IsActiveForce(S)) Out.AddUnique(S.Pawn.Get());
}

FVector UJTSStellarTargetComponent::GetFieldAcceleration(const FVector& Position, const FVector& SurfaceUp,
	const FVector& Velocity)
{
	FVector Result = FVector::ZeroVector;
	for (auto& S : Forces) if (IsActiveForce(S))
	{
		const FVector Center = S.bRepulsion ? S.Pawn->GetActorLocation() : S.Center;
		const FVector Offset = FVector::VectorPlaneProject(Position - Center, SurfaceUp);
		const float Distance = Offset.Size();
		if (S.bRepulsion)
		{
			// Allow a shallow entry, then complete the push even as the target crosses back out.
			// Targets born deep inside take the same path immediately; no entry-crossing event is required.
			if (Distance <= S.Radius - S.EntryDepth) S.bRepulsionEngaged = true;
			if (Distance >= S.Radius + S.ExitOffset + 10.0f) S.bRepulsionEngaged = false;
			if (!S.bRepulsionEngaged) continue;
		}
		else if (Distance >= S.Radius) continue;
		if (Distance < KINDA_SMALL_NUMBER && !S.bRepulsion) continue;
		const FVector Outward = Distance > KINDA_SMALL_NUMBER ? Offset / Distance
			: FVector::CrossProduct(SurfaceUp, FVector(1, 0.37f, 0.19f)).GetSafeNormal();
		if (S.bRepulsion)
		{
			// Slightly overdamped contact absorbs outward momentum before the enemy overshoots the dome.
			// The damping term can brake motion, while the spring's resting point is just outside the range.
			const float Damping = 2.2f * FMath::Sqrt(S.Strength);
			const float Force = S.Strength * (S.Radius + S.ExitOffset - Distance)
				- Damping * FVector::DotProduct(Velocity, Outward);
			Result += Outward * Force;
		}
		else
		{
			// A non-solid attraction point: softened inverse-square force remains finite at the centre.
			const float Softening = FMath::Max(75.0f, S.SofteningRadius * 2.0f);
			const float Reference = FMath::Min(300.0f, S.Radius * 0.5f);
			const float EdgeFade = FMath::Clamp((S.Radius - Distance) / (S.Radius * 0.15f), 0.0f, 1.0f);
			Result -= Outward * (S.Strength * FMath::Square(Reference)
				/ (FMath::Square(Distance) + FMath::Square(Softening)) * EdgeFade);
		}
	}
	return Result.GetClampedToMaxSize(24000.0f);
}

FVector UJTSStellarTargetComponent::IntegrateFieldMotion(const FVector& Position, const FVector& SurfaceUp,
	const FVector& DesiredVelocity, float SteeringAcceleration, const FVector& ContactAcceleration,
	float DeltaSeconds, FVector& Velocity, float& ImpactSpeed)
{
	const FVector Up = SurfaceUp.GetSafeNormal();
	const FVector InitialVelocity = Velocity;
	FVector Current = Position;
	ImpactSpeed = 0;
	const float Duration = FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Duration * 120.0f));
	const float Step = Duration / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const FVector Steering = ((DesiredVelocity - Velocity) * 6.0f).GetClampedToMaxSize(FMath::Max(0.0f, SteeringAcceleration));
		Velocity += (GetFieldAcceleration(Current, Up, Velocity) + Steering + ContactAcceleration - Velocity * FieldDrag) * Step;
		Velocity = FVector::VectorPlaneProject(Velocity, Up).GetClampedToMaxSize(MaximumFieldSpeed);
		Current += Velocity * Step;
	}
	ImpactSpeed = FMath::Max(ImpactSpeed, static_cast<float>((Velocity - InitialVelocity).Size()));
	return FVector::VectorPlaneProject(Current - Position, Up);
}

void UJTSStellarTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(StatusTimer);
	Super::EndPlay(Reason);
}

void UJTSStellarTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSStellarTargetComponent, bIgnited);
	DOREPLIFETIME(UJTSStellarTargetComponent, bLightBurning);
	DOREPLIFETIME(UJTSStellarTargetComponent, PosturePressure);
}
