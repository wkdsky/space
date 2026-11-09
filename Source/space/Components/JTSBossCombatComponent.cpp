#include "space/Components/JTSBossCombatComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Math/RotationMatrix.h"
#include "space/Player/JTSCharacter.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetAnchor.h"

UJTSBossCombatComponent::UJTSBossCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
double UJTSBossCombatComponent::Now() const
{
	if (const auto* GS = GetWorld()->GetGameState()) return GS->GetServerWorldTimeSeconds();
	return GetWorld()->GetTimeSeconds();
}
float UJTSBossCombatComponent::GetActionElapsed() const { return FMath::Max(0., Now() - ActionStart); }
float UJTSBossCombatComponent::GetActionDuration() const
{
	if (Action == EJTSBossAction::Emerge) return EmergenceDuration;
	if (Action == EJTSBossAction::Stagger) return .9f;
	if (Action == EJTSBossAction::Death) return 3.2f;
	if (const auto* Attack = Attacks.Find(Action)) return Attack->Duration();
	return 0;
}
bool UJTSBossCombatComponent::IsDamageable() const { return Action != EJTSBossAction::Death && (Action != EJTSBossAction::Emerge || GetActionElapsed() >= EmergenceDuration * .8f); }
bool UJTSBossCombatComponent::IsVulnerable() const
{
	if (Action == EJTSBossAction::Stagger) return true;
	const auto* Attack = Attacks.Find(Action);
	return Attack && GetActionElapsed() >= Attack->Windup + Attack->Active;
}
void UJTSBossCombatComponent::Initialize(AJTSPlanetAnchor* InPlanet)
{
	Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (!Boss.IsValid() || !Boss->HasAuthority() || !InPlanet) return;
	Planet = InPlanet;
	Home = Boss->GetActorLocation();
	LastUpdate = Now();
	BeginAction(EJTSBossAction::Emerge);
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Update, .04f, true);
}
void UJTSBossCombatComponent::BeginAction(EJTSBossAction NewAction)
{
	Action = NewAction;
	ActionStart = Now();
	HitPlayers.Reset();
	bImpactApplied = false;
	if (Boss.IsValid())
	{
		AttackOrigin = Boss->GetActorLocation();
		AttackForward = Boss->GetActorForwardVector();
		AttackAim = Target.IsValid() ? Target->GetActorLocation() : AttackOrigin + AttackForward * 350;
		if (Planet && (NewAction == EJTSBossAction::Leap || NewAction == EJTSBossAction::Charge))
		{
			const FVector Offset = FVector::VectorPlaneProject(AttackAim - AttackOrigin, Planet->GetRadialUpVector(AttackOrigin)).GetClampedToMaxSize(NewAction == EJTSBossAction::Leap ? 1900 : 1500);
			FJTSPlanetSurfaceHit Hit;
			if (Planet->ProjectPointToSurface(AttackOrigin + Offset, Hit)) AttackAim = Hit.ImpactPoint;
		}
		Boss->RefreshAction();
		Boss->ForceNetUpdate();
	}
}
APawn* UJTSBossCombatComponent::FindTarget() const
{
	APawn* Best = nullptr;
	float Distance = FMath::Square(AggroRadius);
	if (!Boss.IsValid()) return nullptr;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = It->Get();
		auto* Pawn = PC ? Cast<AJTSCharacter>(PC->GetPawn()) : nullptr;
		if (!Pawn || !Pawn->GetHealthComponent() || Pawn->GetHealthComponent()->IsDead()) continue;
		if (FVector::DistSquared(Pawn->GetActorLocation(), Home) > FMath::Square(LeashRadius)) continue;
		const float Candidate = FVector::DistSquared(Pawn->GetActorLocation(), Boss->GetActorLocation());
		if (Candidate < Distance) { Best = Pawn; Distance = Candidate; }
	}
	return Best;
}
float UJTSBossCombatComponent::Face(const FVector& Point, float Dt)
{
	if (!Boss.IsValid() || !Planet) return 0;
	FJTSPlanetSurfaceFrame Frame;
	if (!Planet->GetSurfaceFrameAt(Boss->GetActorLocation(), Boss->GetActorForwardVector(), Frame)) return 0;
	const FVector Desired = FVector::VectorPlaneProject(Point - Boss->GetActorLocation(), Frame.Up).GetSafeNormal();
	if (Desired.IsNearlyZero()) return 0;
	// Local yaw on a sphere must never interpolate world Euler pitch or roll.
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Frame.Forward, Desired), Frame.Up), FVector::DotProduct(Frame.Forward, Desired)));
	const float DesiredSpeed = FMath::Sign(Angle) * FMath::Min(TurnSpeedDegrees, FMath::Sqrt(2 * TurnAccelerationDegrees * FMath::Abs(Angle)));
	TurnVelocity = FMath::FInterpConstantTo(TurnVelocity, DesiredSpeed, Dt, TurnAccelerationDegrees);
	const float Step = FMath::Clamp(TurnVelocity * Dt, -FMath::Abs(Angle), FMath::Abs(Angle));
	Boss->SetActorRotation(FRotationMatrix::MakeFromXZ(Frame.Forward.RotateAngleAxis(Step, Frame.Up), Frame.Up).ToQuat());
	return Angle - Step;
}
void UJTSBossCombatComponent::Move(const FVector& Direction, float Speed, float Dt)
{
	if (!Boss.IsValid() || !Planet) return;
	FVector Ground, Heading;
	const bool bCanMove = Action == EJTSBossAction::Charge
		? Boss->Steering->ConstrainDisplacement(Planet, Boss->GetActorLocation(), Direction.GetSafeNormal() * Speed * Dt, Ground, Heading)
		: Boss->Steering->Advance(Planet, Boss->GetActorLocation(), Boss->GetActorForwardVector(), Speed, Dt, Ground, Heading);
	if (bCanMove)
	{
		// The steering component probes the real spherical mesh, including slope and cliff constraints.
		Boss->SetActorLocationAndRotation(Ground, Planet->MakeSurfaceAlignedRotation(Ground, Boss->GetActorForwardVector()));
	}
}
void UJTSBossCombatComponent::ChooseAttack(APawn* Pawn)
{
	const FVector Delta = FVector::VectorPlaneProject(Pawn->GetActorLocation() - Boss->GetActorLocation(), Boss->GetActorUpVector());
	const float Distance = Delta.Size();
	const float Front = FVector::DotProduct(Delta.GetSafeNormal(), Boss->GetActorForwardVector());
	auto Ready = [&](EJTSBossAction A) { return Attacks.Contains(A) && Now() >= ReadyAt.FindRef(A); };
	EJTSBossAction Next = EJTSBossAction::Walk;
	if (Distance > 850 && Front > .85f)
	{
		if (Distance > 1150 && LastAttack == EJTSBossAction::Charge && Ready(EJTSBossAction::Leap)) Next = EJTSBossAction::Leap;
		else if (Ready(EJTSBossAction::Charge)) Next = EJTSBossAction::Charge;
		else if (Ready(EJTSBossAction::Leap)) Next = EJTSBossAction::Leap;
	}
	const auto* Slam = Attacks.Find(EJTSBossAction::Slam);
	const auto* Sweep = Attacks.Find(EJTSBossAction::Sweep);
	const float BodyRadius = Pawn->GetSimpleCollisionRadius();
	const bool bCanSlam = Slam && Ready(EJTSBossAction::Slam) && Front > .45f && Distance <= Slam->Reach * .58f + Slam->Radius + BodyRadius - 5;
	const bool bCanSweep = Sweep && Ready(EJTSBossAction::Sweep) && Front > -.15f && Distance <= Sweep->Reach + BodyRadius - 5;
	if (bCanSlam || bCanSweep)
	{
		if (bCanSweep && (Front < .7f || LastAttack == EJTSBossAction::Slam || !bCanSlam)) Next = EJTSBossAction::Sweep;
		else if (bCanSlam) Next = EJTSBossAction::Slam;
	}
	if (Next == EJTSBossAction::Walk) { if (Action != Next) BeginAction(Next); return; }
	LastAttack = Next;
	++ComboCount;
	ReadyAt.Add(Next, Now() + Attacks[Next].Cooldown);
	BeginAction(Next);
}
void UJTSBossCombatComponent::ApplyAttack(float Phase, const FVector& PreviousLocation)
{
	const auto* Attack = Attacks.Find(Action);
	if (!Attack || !Boss.IsValid() || !Planet) return;
	const FVector Origin = Boss->GetActorLocation();
	const FVector Up = Planet->GetRadialUpVector(Origin);
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = It->Get();
		auto* Pawn = PC ? Cast<AJTSCharacter>(PC->GetPawn()) : nullptr;
		if (!Pawn || HitPlayers.Contains(Pawn) || !Pawn->GetHealthComponent() || Pawn->GetHealthComponent()->IsDead()) continue;
		const FVector Point = Pawn->GetActorLocation();
		const FVector Delta = FVector::VectorPlaneProject(Point - Origin, Up);
		const float Height = FMath::Abs(FVector::DotProduct(Point - Origin, Up));
		const float BodyRadius = Pawn->GetSimpleCollisionRadius();
		bool bHit = false;
		if (Action == EJTSBossAction::Charge)
			bHit = Height < 230 && FMath::PointDistToSegment(Point - Up * FVector::DotProduct(Point - Origin, Up), PreviousLocation + AttackForward * 150, Origin + AttackForward * 150) <= Attack->Radius + BodyRadius;
		else if (Action == EJTSBossAction::Leap)
			bHit = Height < 220 && Delta.Size() <= Attack->Radius + BodyRadius;
		else if (Action == EJTSBossAction::Slam)
			bHit = Height < 240 && (Delta - AttackForward * Attack->Reach * .58f).Size() <= Attack->Radius + BodyRadius;
		else if (Action == EJTSBossAction::Sweep)
		{
			const FVector SweepHeading = AttackForward.RotateAngleAxis(FMath::Lerp(-Attack->ArcDegrees * .5f, Attack->ArcDegrees * .5f, Phase), Up);
			bHit = Height < 240 && Delta.Size() <= Attack->Reach + BodyRadius && FVector::DotProduct(Delta.GetSafeNormal(), SweepHeading) > .80f;
		}
		if (!bHit) continue;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BossAttackVisibility), false, Boss.Get());
		Query.AddIgnoredActor(Pawn);
		FHitResult Blocker;
		if (GetWorld()->LineTraceSingleByChannel(Blocker, Origin + Up * 100, Point, ECC_Visibility, Query)) continue;
		HitPlayers.Add(Pawn);
		UGameplayStatics::ApplyDamage(Pawn, Attack->Damage, nullptr, Boss.Get(), nullptr);
	}
}
void UJTSBossCombatComponent::Update()
{
	if (!Boss.IsValid() || !Planet || Boss->Health->IsDead()) return;
	const double Time = Now();
	const float Dt = FMath::Clamp(float(Time - LastUpdate), .001f, .12f);
	LastUpdate = Time;
	const float Elapsed = GetActionElapsed();
	if (Action == EJTSBossAction::Emerge || Action == EJTSBossAction::Stagger)
	{
		if (Elapsed >= GetActionDuration()) { BeginAction(EJTSBossAction::Idle); NextDecision = Time + AttackPause; }
		return;
	}
	if (const auto* Attack = Attacks.Find(Action))
	{
		if (Elapsed < Attack->Windup)
		{
			if (Elapsed < Attack->Windup * .65f && Target.IsValid()) Face(Target->GetActorLocation(), Dt);
			AttackForward = Boss->GetActorForwardVector();
		}
		else
		{
			const FVector Previous = Boss->GetActorLocation();
			const float Phase = FMath::Clamp((Elapsed - Attack->Windup) / FMath::Max(.05f, Attack->Active), 0.f, 1.f);
			if (Action == EJTSBossAction::Leap && !bImpactApplied)
			{
				FJTSPlanetSurfaceHit Ground;
				if (Planet->ProjectPointToSurface(FMath::Lerp(AttackOrigin, AttackAim, Phase), Ground))
					Boss->SetActorLocationAndRotation(Ground.ImpactPoint + Planet->GetRadialUpVector(Ground.ImpactPoint) * FMath::Sin(PI * Phase) * LeapHeight,
						Planet->MakeSurfaceAlignedRotation(Ground.ImpactPoint, AttackForward));
				if (Phase >= 1) { ApplyAttack(1, Previous); bImpactApplied = true; }
			}
			else if (Elapsed <= Attack->Windup + Attack->Active)
			{
				if (Action == EJTSBossAction::Charge) Move(AttackForward, ChargeSpeed, Dt);
				ApplyAttack(Phase, Previous);
			}
		}
		if (Elapsed >= Attack->Duration())
		{
			Target = FindTarget();
			const bool bCombo = Target.IsValid() && ComboCount < 2 && FVector::DistSquared(Target->GetActorLocation(), Boss->GetActorLocation()) < FMath::Square(650.f);
			BeginAction(EJTSBossAction::Idle);
			NextDecision = Time + (bCombo ? .25 : AttackPause);
			if (!bCombo) ComboCount = 0;
		}
		return;
	}
	Target = FindTarget();
	if (!Target.IsValid())
	{
		ComboCount = 0;
		if (FVector::DistSquared(Home, Boss->GetActorLocation()) > FMath::Square(150.f))
		{
			const float Angle = Face(Home, Dt);
			if (FMath::Abs(Angle) > 20)
			{
				const auto Turn = Angle > 0 ? EJTSBossAction::TurnRight : EJTSBossAction::TurnLeft;
				if (Action != Turn) BeginAction(Turn);
				return;
			}
			if (Action != EJTSBossAction::Walk) BeginAction(EJTSBossAction::Walk);
			Move(Home - Boss->GetActorLocation(), WalkSpeed, Dt);
		}
		else if (Action != EJTSBossAction::Idle) BeginAction(EJTSBossAction::Idle);
		return;
	}
	const float Angle = Face(Target->GetActorLocation(), Dt);
	const bool bTurning = Action == EJTSBossAction::TurnLeft || Action == EJTSBossAction::TurnRight;
	if (FMath::Abs(Angle) > (bTurning ? 7.f : 25.f))
	{
		const auto Turn = Angle > 0 ? EJTSBossAction::TurnRight : EJTSBossAction::TurnLeft;
		if (Action != Turn) BeginAction(Turn);
		return;
	}
	if (bTurning) BeginAction(EJTSBossAction::Idle);
	if (Time < NextDecision) return;
	ChooseAttack(Target.Get());
	if (Action == EJTSBossAction::Walk)
	{
		const float Distance = FVector::Dist(Target->GetActorLocation(), Boss->GetActorLocation());
		if (Distance > 320) Move(Target->GetActorLocation() - Boss->GetActorLocation(), WalkSpeed, Dt);
	}
}
void UJTSBossCombatComponent::ReactToDamage(float Damage)
{
	if (!GetOwner()->HasAuthority()) return;
	StaggerDamage += Damage;
	if (StaggerDamage >= StaggerThreshold && Now() >= NextStagger && (Action == EJTSBossAction::Idle || Action == EJTSBossAction::Walk || Action == EJTSBossAction::TurnLeft || Action == EJTSBossAction::TurnRight || IsVulnerable()))
	{
		StaggerDamage = 0;
		NextStagger = Now() + 7;
		BeginAction(EJTSBossAction::Stagger);
	}
}
void UJTSBossCombatComponent::Die()
{
	if (!GetOwner()->HasAuthority()) return;
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	BeginAction(EJTSBossAction::Death);
}
void UJTSBossCombatComponent::OnRep_Action()
{
	Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (Boss.IsValid()) Boss->RefreshAction();
}
void UJTSBossCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSBossCombatComponent, Action);
	DOREPLIFETIME(UJTSBossCombatComponent, ActionStart);
	DOREPLIFETIME(UJTSBossCombatComponent, AttackAim);
	DOREPLIFETIME(UJTSBossCombatComponent, Planet);
}
void UJTSBossCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Super::EndPlay(Reason);
}
