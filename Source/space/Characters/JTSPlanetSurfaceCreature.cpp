#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSMirrorDefenseComponent.h"
#include "space/Components/JTSNestBlockadeComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

AJTSPlanetSurfaceCreature::AJTSPlanetSurfaceCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20);
	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->InitSphereRadius(30);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Mesh = CreateDefaultSubobject<UJTSReflectorSkeletalMeshComponent>(TEXT("CreatureMesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->AddTickPrerequisiteActor(this);
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	Steering = CreateDefaultSubobject<UJTSPlanetSurfaceSteeringComponent>(TEXT("SurfaceSteering"));
	MirrorDefense = CreateDefaultSubobject<UJTSMirrorDefenseComponent>(TEXT("MirrorDefense"));
	MirrorDefense->AddTickPrerequisiteActor(this);
}

void AJTSPlanetSurfaceCreature::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(!HasAuthority());
	OnRep_State();
	if (HasAuthority())
	{
		Health->SetMaxHealth(Health->GetMaxHealth(), true);
		Health->OnDamaged.AddDynamic(this, &ThisClass::OnDamaged);
		Health->OnDeath.AddDynamic(this, &ThisClass::OnDied);
	}
}

void AJTSPlanetSurfaceCreature::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
		if (auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get())) Mouth->Blockade->UnregisterResident(this, true);
	GetWorldTimerManager().ClearTimer(BehaviorTimer);
	Super::EndPlay(Reason);
}

bool AJTSPlanetSurfaceCreature::InitializeForSettlement_Implementation(AJTSPlanetAnchor* Planet, FVector HomeLocation, FVector GroundLocation)
{
	if (!HasAuthority() || !IsValid(Planet)) return false;
	FJTSPlanetSurfaceHit SpawnSurface;
	if (!Planet->ProjectPointToSurface(GroundLocation, SpawnSurface) || !Steering->IsWalkable(Planet, SpawnSurface)) return false;
	ActivePlanet = Planet;
	Home = bDefendsNest ? HomeLocation : GroundLocation;
	Refuge = HomeLocation;
	Goal = GroundLocation;
	LastUpdate = GetWorld()->GetTimeSeconds();
	SetState(EJTSForagerState::Emerge, 0);
	GetWorldTimerManager().SetTimer(BehaviorTimer, this, &ThisClass::UpdateBehavior, 0.05f, true, FMath::FRandRange(0.02f, 0.1f));
	return true;
}

void AJTSPlanetSurfaceCreature::SetHabitat(AJTSPlanetEcologyHabitat* InHabitat, const FVector& RefugeLocation)
{
	if (!HasAuthority()) return;
	Habitat = InHabitat;
	RefugeActor = InHabitat ? InHabitat->FindRefugeActor(RefugeLocation) : nullptr;
	if (auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get()); Mouth && !Mouth->Blockade->TryRegisterResident(this))
	{
		Destroy();
		return;
	}
	Refuge = RefugeActor.IsValid() ? AJTSPlanetEcologyHabitat::GetRefugeSurfaceLocation(RefugeActor.Get()) : RefugeLocation;
	if (!bDefendsNest) Home = Refuge;
	if (!bDefendsNest && RefugeActor.IsValid() && State == EJTSForagerState::Emerge && ActivePlanet.IsValid()
		&& (!InHabitat || !InHabitat->bStationaryFeeding))
	{
		FJTSPlanetSurfaceFrame Frame;
		if (ActivePlanet->GetSurfaceFrameAt(Refuge, RefugeActor->GetActorForwardVector(), Frame))
			SetActorLocationAndRotation(Frame.Location, Frame.Transform.GetRotation(), false);
	}
}

void AJTSPlanetSurfaceCreature::SetFeedingTarget(AActor* Target)
{
	if (!HasAuthority() || !IsValid(Target) || !ActivePlanet.IsValid()) return;
	if (FeedingTarget.IsValid()) FeedingTarget->OnDestroyed.RemoveDynamic(this, &ThisClass::OnFeedingTargetDestroyed);
	FeedingTarget = Target;
	FeedingTarget->OnDestroyed.AddDynamic(this, &ThisClass::OnFeedingTargetDestroyed);
	bReturningFromFeeding = false;
	FJTSPlanetSurfaceFrame Frame;
	if (ActivePlanet->GetSurfaceFrameAt(GetActorLocation(), Target->GetActorLocation() - GetActorLocation(), Frame))
		SetActorRotation(Frame.Transform.GetRotation());
	SetState(EJTSForagerState::Feed, FMath::FRandRange(4.0f, 9.0f));
}

void AJTSPlanetSurfaceCreature::OnFeedingTargetDestroyed(AActor* Target)
{
	if (!HasAuthority() || Health->IsDead()) return;
	FeedingTarget.Reset();
	bReturningFromFeeding = true;
	SetState(EJTSForagerState::Flee, 20);
}

void AJTSPlanetSurfaceCreature::SetState(EJTSForagerState NewState, float Duration)
{
	const bool bChanged = State != NewState;
	State = NewState;
	const TObjectPtr<UAnimSequence>* Clip = Animations.Find(State);
	const float* ConfiguredRate = AnimationPlayRates.Find(State);
	const float Rate = ConfiguredRate ? FMath::Max(0.1f, *ConfiguredRate) : 1.0f;
	StateEnd = GetWorld()->GetTimeSeconds() + (Duration > 0 ? Duration : (Clip && *Clip ? (*Clip)->GetPlayLength() / Rate : 0.8f));
	Body->ComponentVelocity = FVector::ZeroVector;
	MovementSpeed = 0;
	if (HasAuthority()) SetActorTickEnabled(false);
	if (bChanged) OnRep_State();
	ForceNetUpdate();
}

void AJTSPlanetSurfaceCreature::OnRep_State()
{
	const bool bUnderground = State == EJTSForagerState::Hidden;
	Mesh->SetVisibility(!bUnderground);
	Body->SetCollisionEnabled(bUnderground || State == EJTSForagerState::Dead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	const EJTSForagerState AnimationState = State == EJTSForagerState::MirrorRetract || State == EJTSForagerState::MirrorPause
		? EJTSForagerState::Reflect : State;
	if (const TObjectPtr<UAnimSequence>* Clip = Animations.Find(AnimationState); Clip && *Clip)
	{
		const bool bLoop = State == EJTSForagerState::Idle || State == EJTSForagerState::Walk
			|| State == EJTSForagerState::Feed || State == EJTSForagerState::Flee || State == EJTSForagerState::Reflect
			|| State == EJTSForagerState::MirrorRetract || State == EJTSForagerState::MirrorPause;
		Mesh->PlayAnimation(*Clip, bLoop);
		const float* Rate = AnimationPlayRates.Find(AnimationState);
		Mesh->SetPlayRate(Rate ? FMath::Max(0.1f, *Rate) : 1.0f);
	}
}

APawn* AJTSPlanetSurfaceCreature::FindThreat() const
{
	APawn* Closest = nullptr;
	double Distance = FMath::Square(bDefendsNest ? LeashRadius : StartleDistance);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerControlled()) continue;
		if (const auto* PlayerHealth = It->FindComponentByClass<UJTSHealthComponent>(); PlayerHealth && PlayerHealth->IsDead()) continue;
		if (bDefendsNest && FVector::DistSquared(It->GetActorLocation(), Home) > FMath::Square(LeashRadius)) continue;
		const double Candidate = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (Candidate < Distance) { Distance = Candidate; Closest = *It; }
	}
	return Closest;
}

bool AJTSPlanetSurfaceCreature::MoveTowards(const FVector& Destination, float Speed, float DeltaSeconds)
{
	const bool bBlockedAtSameGoal = bMovementBlocked && MovementGoal.Equals(Destination, 1.f);
	MovementGoal = Destination;
	MovementSpeed = Speed;
	SetActorTickEnabled(true);
	return !bBlockedAtSameGoal;
}

bool AJTSPlanetSurfaceCreature::AdvanceMovement(const FVector& Destination, float Speed, float DeltaSeconds)
{
	AJTSPlanetAnchor* Planet = ActivePlanet.Get();
	if (!Planet) return false;
	const FVector Previous = GetActorLocation();
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / 0.05f), 1, 16);
	const float StepTime = DeltaSeconds / Steps;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		FVector Ground, Heading;
		const float Remaining = FVector::VectorPlaneProject(Destination - GetActorLocation(), Planet->GetRadialUpVector(GetActorLocation())).Size();
		if (Remaining < 2) break;
		// Slow down while turning into a nearby destination instead of orbiting it at full run speed.
		const float ArrivalSpeed = FMath::Min3(Speed, Remaining * Steering->HeadingResponse * 0.5f, Remaining / StepTime);
		if (!Steering->Advance(Planet, GetActorLocation(), Destination - GetActorLocation(), ArrivalSpeed, StepTime, Ground, Heading)) return false;
		// A species cannot wander out of its authored illumination or activity boundary.
		if (Habitat.IsValid() && !bReturningFromFeeding && !bEntranceDefense && !Habitat->ContainsPoint(Ground, Planet)) return false;
		FJTSPlanetSurfaceFrame Frame;
		if (!Planet->GetSurfaceFrameAt(Ground, Heading, Frame)) return false;
		const FQuat Rotation = FQuat::Slerp(GetActorQuat(), Frame.Transform.GetRotation(), FMath::Clamp(StepTime * 9.0f, 0.0f, 1.0f));
		SetActorLocationAndRotation(Ground, Rotation, false);
	}
	Body->ComponentVelocity = (GetActorLocation() - Previous) / FMath::Max(DeltaSeconds, 0.001f);
	return true;
}

void AJTSPlanetSurfaceCreature::ChooseNextActivity()
{
	AJTSPlanetAnchor* Planet = ActivePlanet.Get();
	if (!Planet) return;
	FJTSPlanetSurfaceHit Hit;
	if (Habitat.IsValid() && !Habitat->FoodPoints.IsEmpty() && FMath::FRand() < 0.6f)
	{
		AActor* Food = Habitat->FoodPoints[FMath::RandRange(0, Habitat->FoodPoints.Num() - 1)];
		if (IsValid(Food) && Planet->ApproximateSurfaceArcDistance(Home, Food->GetActorLocation()) <= RoamRadius)
		{
			Goal = Food->GetActorLocation();
			SetState(EJTSForagerState::Walk, 25);
			return;
		}
	}
	if (FMath::FRand() < 0.3f) { SetState(EJTSForagerState::Idle, FMath::FRandRange(2.0f, 6.0f)); return; }
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		if (Planet->RandomPointInSurfaceCap(Planet->GetRadialUpVector(Home), RoamRadius, Hit)
			&& (!Habitat.IsValid() || Habitat->ContainsPoint(Hit.ImpactPoint, Planet)))
		{
			Goal = Hit.ImpactPoint;
			SetState(EJTSForagerState::Walk, FMath::FRandRange(5.0f, 18.0f));
			return;
		}
	}
	SetState(EJTSForagerState::Idle, 3);
}

bool AJTSPlanetSurfaceCreature::BelongsToRefuge(const AJTSPlanetEcologyHabitat* RefugeOwner) const
{
	return RefugeActor.Get() == RefugeOwner || Habitat.Get() == RefugeOwner;
}
bool AJTSPlanetSurfaceCreature::IsAlive() const { return Health && !Health->IsDead(); }
void AJTSPlanetSurfaceCreature::BeginEntranceEmergence(const FTransform& MouthPose, const FVector& ExitPoint)
{
	if (!HasAuthority()) return;
	bTemporaryEntranceDefender = true;
	bExitingEntrance = true;
	EntranceExitPoint = ExitPoint;
	SetActorLocationAndRotation(MouthPose.GetLocation(), MouthPose.GetRotation(), false);
	SetState(EJTSForagerState::Emerge, 0);
}
void AJTSPlanetSurfaceCreature::DefendEntrance(APawn* Target, const FVector& Origin, float Leash, int32 Slot, int32 CohortSize)
{
	if (!HasAuthority() || !IsAlive() || !IsValid(Target)) return;
	if (FVector::DistSquared(Target->GetActorLocation(), Origin) > FMath::Square(Leash)) return;
	if (const auto* TargetHealth = Target->FindComponentByClass<UJTSHealthComponent>(); TargetHealth && TargetHealth->IsDead()) return;
	EntranceDefenseTarget = Target; EntranceDefenseOrigin = Origin; EntranceDefenseLeash = Leash;
	DefenseSlot = Slot;
	DefenseCohortSize = FMath::Max(1, CohortSize);
	if (bReturningFromDefense && GetWorld()->GetTimeSeconds() < DefenseReturnAfter) return;
	bReturningFromDefense = false;
	if (!bEntranceDefense)
	{
		MirrorAggressors.Reset();
		bMirrorRetaliation = false;
		Aggressor.Reset();
		EntranceDefenseForward = GetActorForwardVector();
		MirrorGuardBlockedTime = 0;
		bMirrorGuardFallback = false;
		bEntranceDefense = true;
		SetState(State == EJTSForagerState::Hidden || bTemporaryEntranceDefender ? EJTSForagerState::Emerge : EJTSForagerState::Walk, 0);
	}
	if (bExitingEntrance) MoveTowards(EntranceExitPoint, FleeSpeed, 0);
}
void AJTSPlanetSurfaceCreature::ReturnFromEntranceDefense(bool bEntranceDestroyed)
{
	if (!HasAuthority() || !bEntranceDefense || !IsAlive()) return;
	if (bReturningFromDefense) { bDestroyAfterDefense |= bEntranceDestroyed; return; }
	MirrorDefense->StopReflection();
	EntranceDefenseTarget.Reset(); Aggressor.Reset();
	bReturningFromDefense = true;
	bExitingEntrance = false;
	bDestroyAfterDefense = bEntranceDestroyed || bTemporaryEntranceDefender;
	DefenseReturnAfter = GetWorld()->GetTimeSeconds();
	if (MirrorDefense->bEnabled)
	{
		DefenseReturnAfter += MirrorDefense->RetractSeconds + MirrorDefense->SurfacePauseSeconds;
		SetState(EJTSForagerState::MirrorRetract, MirrorDefense->RetractSeconds);
	}
	else SetState(EJTSForagerState::Walk, 30);
}

APawn* AJTSPlanetSurfaceCreature::FindMirrorAggressor()
{
	// Damage creates individual threat. Proximity alone can never recreate a forgotten target.
	MirrorAggressors.RemoveAll([this](const auto& Candidate)
	{
		const APawn* Pawn = Candidate.Get();
		if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(LeashRadius)) return true;
		const auto* TargetHealth = Pawn->FindComponentByClass<UJTSHealthComponent>();
		return TargetHealth && TargetHealth->IsDead();
	});
	APawn* Closest = nullptr;
	double Best = TNumericLimits<double>::Max();
	for (const auto& Candidate : MirrorAggressors)
	{
		const double Distance = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (Distance < Best) { Best = Distance; Closest = Candidate.Get(); }
	}
	return Closest;
}

bool AJTSPlanetSurfaceCreature::UpdateMirrorPosture(double Now, APawn* Target)
{
	if (State == EJTSForagerState::Reflect)
	{
		// Reflection has no duration limit. Only losing the provoked target begins retraction.
		if (IsValid(Target)) { MirrorDefense->AimAt(Target); return true; }
		MirrorDefense->StopReflection();
		SetState(EJTSForagerState::MirrorRetract, MirrorDefense->RetractSeconds);
		return true;
	}
	if (State == EJTSForagerState::MirrorRetract)
	{
		if (Now >= StateEnd) SetState(EJTSForagerState::MirrorPause, MirrorDefense->SurfacePauseSeconds);
		return true;
	}
	if (State == EJTSForagerState::MirrorPause) return Now < StateEnd;
	return false;
}
bool AJTSPlanetSurfaceCreature::UpdateEntranceDefense(double Now, float Dt)
{
	if (!bEntranceDefense) return false;
	auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get());
	const bool bEntranceDestroyed = (Mouth && Mouth->IsEntranceBlocked()) || (Habitat.IsValid() && Habitat->IsEntranceBlocked());
	APawn* Target = EntranceDefenseTarget.Get();
	const auto* TargetHealth = Target ? Target->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	if (!Target || (TargetHealth && TargetHealth->IsDead())
		|| FVector::DistSquared(Target->GetActorLocation(), EntranceDefenseOrigin) > FMath::Square(EntranceDefenseLeash))
	{
		// Another actual nest attacker can retain the alarm when the selected
		// target leaves. Do not insert a false recovery window between players.
		Target = Mouth ? Mouth->Blockade->FindDefenseAggressor() : nullptr;
		if (Target) EntranceDefenseTarget = Target;
		else ReturnFromEntranceDefense(bEntranceDestroyed);
	}
	if (bReturningFromDefense || !EntranceDefenseTarget.IsValid())
	{
		if (MirrorDefense->bEnabled && Now < DefenseReturnAfter)
		{
			UpdateMirrorPosture(Now, nullptr);
			return true;
		}
		if (State != EJTSForagerState::Walk) SetState(EJTSForagerState::Walk, 30);
		if (ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), EntranceDefenseOrigin) < RefugeEntryDistance)
		{
			bEntranceDefense = false;
			if (bDestroyAfterDefense)
			{
				if (Mouth && !Mouth->IsEntranceBlocked())
					Mouth->Blockade->UnregisterResident(this, false);
				Destroy();
			}
			else SetState(EJTSForagerState::Burrow, 0);
		}
		else MoveTowards(EntranceDefenseOrigin, FleeSpeed, Dt);
		return true;
	}
	if (bExitingEntrance)
	{
		// The emergence clip and forward travel happen together, so successive animals clear the lip.
		MoveTowards(EntranceExitPoint, FleeSpeed, Dt);
		if (ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), EntranceExitPoint) > 25) return true;
		bExitingEntrance = false;
		SetState(EJTSForagerState::Flee, 20);
	}
	if ((State == EJTSForagerState::Emerge || State == EJTSForagerState::Hit || State == EJTSForagerState::Attack) && Now < StateEnd) return true;
	Target = EntranceDefenseTarget.Get();
	if (MirrorDefense->bEnabled)
	{
		if (UpdateMirrorPosture(Now, Target)) return true;
		// Fixed, mouth-relative guard lanes. Moving the player never drags the cohort away from its nest.
		const FVector Up = ActivePlanet->GetRadialUpVector(EntranceDefenseOrigin);
		const FVector Forward = FVector::VectorPlaneProject(EntranceDefenseForward, Up).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		const int32 Column = DefenseSlot % 3, Row = DefenseSlot / 3;
		const FVector GuardPoint = EntranceDefenseOrigin + Forward * (MirrorDefense->GuardRadius + Row * 70.f) + Right * ((Column-1)*105.f);
		FJTSPlanetSurfaceHit GuardSurface;
		if (!ActivePlanet->ProjectPointToSurface(GuardPoint, GuardSurface) || !Steering->IsWalkable(ActivePlanet.Get(), GuardSurface))
			bMirrorGuardFallback = true;
		const float Remaining = bMirrorGuardFallback ? 0.f : ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), GuardSurface.ImpactPoint);
		if (Remaining > 30)
		{
			if (State != EJTSForagerState::Flee) SetState(EJTSForagerState::Flee, 20);
			MoveTowards(GuardSurface.ImpactPoint, FleeSpeed, Dt);
			MirrorGuardBlockedTime = bMovementBlocked ? MirrorGuardBlockedTime + Dt : 0.f;
			// A rock beside the mouth must not leave a reflector running against it forever.
			if (MirrorGuardBlockedTime > 1.5f) bMirrorGuardFallback = true;
		}
		else
		{
			Body->ComponentVelocity = FVector::ZeroVector;
			if (State != EJTSForagerState::Reflect) SetState(EJTSForagerState::Reflect, 0);
			MirrorDefense->AimAt(Target);
		}
		return true;
	}
	FJTSPlanetSurfaceHit TargetSurface;
	if (!ActivePlanet->ProjectPointToSurface(Target->GetActorLocation(), TargetSurface)) return true;
	const FVector Up = ActivePlanet->GetRadialUpVector(TargetSurface.ImpactPoint);
	FVector Forward, Right;
	Up.FindBestAxisVectors(Forward, Right);
	const float Angle = DefenseSlot * 2 * PI / DefenseCohortSize;
	FJTSPlanetSurfaceHit ApproachSurface;
	if (!ActivePlanet->ProjectPointToSurface(TargetSurface.ImpactPoint + (Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * AttackReach * 0.80f, ApproachSurface)) return true;
	const FVector Approach = ApproachSurface.ImpactPoint;
	// Reach the individual approach point before attacking. Otherwise every animal stops at the same near edge.
	if (FVector::DistSquared(GetActorLocation(), Approach) > FMath::Square(15.0f))
	{
		if (State != EJTSForagerState::Flee) SetState(EJTSForagerState::Flee, 20);
		MoveTowards(Approach, FleeSpeed, Dt);
		return true;
	}
	// Combat distances use the real ground below the capsule, including inside a deep crater.
	if (FVector::DistSquared(GetActorLocation(), TargetSurface.ImpactPoint) < FMath::Square(AttackReach)
		&& FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) < FMath::Square(AttackReach + Target->GetSimpleCollisionHalfHeight()))
	{
		if (Now >= NextAttack)
		{
			FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(EntranceDefense), true, this); Params.AddIgnoredActor(Target);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + GetActorUpVector() * 35, Target->GetActorLocation(), ECC_Visibility, Params))
			{
				FJTSPlanetSurfaceFrame AttackFrame;
				if (ActivePlanet->GetSurfaceFrameAt(GetActorLocation(), TargetSurface.ImpactPoint - GetActorLocation(), AttackFrame))
					SetActorRotation(AttackFrame.Transform.GetRotation());
				if (!MirrorDefense->bEnabled) UGameplayStatics::ApplyDamage(Target, AttackDamage, nullptr, this, nullptr);
				NextAttack = Now + AttackInterval; SetState(EJTSForagerState::Attack, 0);
				return true;
			}
		}
	}
	return true;
}

void AJTSPlanetSurfaceCreature::UpdateBehavior()
{
	if (!HasAuthority() || !ActivePlanet.IsValid() || Health->IsDead()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	// Decisions run at a modest cadence; active movement advances separately on every server frame.
	const float Dt = FMath::Max(static_cast<float>(Now - LastUpdate), 0.001f);
	LastUpdate = Now;
	MovementSpeed = 0;
	Body->ComponentVelocity = FVector::ZeroVector;
	SetActorTickEnabled(false);
	if (IsHidden()) return;
	if (UpdateEntranceDefense(Now, Dt)) return;
	if (MirrorDefense->bEnabled && bMirrorRetaliation)
	{
		APawn* Target = FindMirrorAggressor();
		Aggressor = Target;
		if (Target)
		{
			if (State == EJTSForagerState::Hit && Now < StateEnd) return;
			if (State != EJTSForagerState::Reflect) SetState(EJTSForagerState::Reflect, 0);
			MirrorDefense->AimAt(Target);
			return;
		}
		if (UpdateMirrorPosture(Now, nullptr)) return;
		bMirrorRetaliation = false;
		SetState(EJTSForagerState::Flee, 20);
	}
	MirrorDefense->StopReflection();
	if (Habitat.IsValid() && !bDefendsNest)
	{
		const UJTSHealthComponent* RefugeHealth = RefugeActor.IsValid() ? RefugeActor->FindComponentByClass<UJTSHealthComponent>() : nullptr;
		const auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get());
		if (!RefugeActor.IsValid() || (RefugeHealth && RefugeHealth->IsDead()) || (Mouth && !Mouth->IsRefugeAvailable()))
		{
			RefugeActor = Habitat->FindRefugeActor(GetActorLocation());
			if (RefugeActor.IsValid())
			{
				Refuge = AJTSPlanetEcologyHabitat::GetRefugeSurfaceLocation(RefugeActor.Get()); Home = Refuge;
				if (State == EJTSForagerState::Hidden || State == EJTSForagerState::Burrow)
				{
					FJTSPlanetSurfaceFrame Frame;
					if (ActivePlanet->GetSurfaceFrameAt(Refuge, RefugeActor->GetActorForwardVector(), Frame))
						SetActorLocationAndRotation(Frame.Location, Frame.Transform.GetRotation(), false);
					SetState(EJTSForagerState::Emerge, 0);
				}
			}
			else if (State == EJTSForagerState::Hidden || State == EJTSForagerState::Burrow || State == EJTSForagerState::Flee)
				SetState(EJTSForagerState::Idle, 1);
		}
		else
		{
			Refuge = AJTSPlanetEcologyHabitat::GetRefugeSurfaceLocation(RefugeActor.Get()); Home = Refuge;
		}
	}
	const bool bEnded = Now >= StateEnd;
	if (State == EJTSForagerState::Burrow) { if (bEnded) SetState(EJTSForagerState::Hidden, FMath::FRandRange(12.0f, 28.0f)); return; }
	if (State == EJTSForagerState::Hidden) { if (!bReturningFromFeeding && bEnded && !FindThreat()) SetState(EJTSForagerState::Emerge, 0); return; }
	if (State == EJTSForagerState::Startle || State == EJTSForagerState::Hit)
	{
		if (bEnded) SetState((bDefendsNest || (bRetaliates && Aggressor.IsValid())) ? EJTSForagerState::Idle : EJTSForagerState::Flee, 15);
		return;
	}
	if (State == EJTSForagerState::Emerge)
	{
		if (!bEnded) return;
		// Move out of the physical opening after emerging, then roam around that refuge.
		if (ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), Refuge) < RefugeEntryDistance * 1.5f)
		{
			const FVector Up = ActivePlanet->GetRadialUpVector(GetActorLocation());
			FVector Outward = FVector::VectorPlaneProject(GetActorLocation() - Refuge, Up).GetSafeNormal();
			if (Outward.IsNearlyZero()) Outward = GetActorForwardVector();
			const float Angles[] = {0, 45, -45, 90, -90, 135, -135, 180};
			for (const float Distance : {220.0f, 140.0f, 80.0f}) for (const float Angle : Angles)
			{
				const FVector Direction = FQuat(Up, FMath::DegreesToRadians(Angle)).RotateVector(Outward);
				FVector Exit, ExitHeading;
				if (!Steering->ConstrainDisplacement(ActivePlanet.Get(), GetActorLocation(), Direction * Distance, Exit, ExitHeading)
					|| (Habitat.IsValid() && !Habitat->ContainsPoint(Exit, ActivePlanet.Get()))
					|| ActivePlanet->ApproximateSurfaceArcDistance(Exit, Refuge) < RefugeEntryDistance * 1.6f) continue;
				Goal = Exit;
				SetState(EJTSForagerState::Walk, Distance / FMath::Max(WalkSpeed, 1.0f) + 5);
				return;
			}
		}
		SetState(EJTSForagerState::Idle, 0.5f);
		return;
	}
	if (State == EJTSForagerState::Attack)
	{
		if (bEnded) SetState(EJTSForagerState::Idle, 0.5f);
		return;
	}
	if (State == EJTSForagerState::Flee)
	{
		if ((!Habitat.IsValid() || RefugeActor.IsValid()) && ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), Refuge) < RefugeEntryDistance)
		{
			SetState(EJTSForagerState::Burrow, 0);
		}
		else if (!MoveTowards(Refuge, FleeSpeed, Dt) && bEnded) SetState(EJTSForagerState::Idle, 2);
		return;
	}
	APawn* Threat = FindThreat();
	if (bRetaliates && Aggressor.IsValid() && FVector::DistSquared(Aggressor->GetActorLocation(), AggressionOrigin) < FMath::Square(LeashRadius)) Threat = Aggressor.Get();
	if (Threat)
	{
		if (bDefendsNest || (bRetaliates && Threat == Aggressor.Get()))
		{
			const float Distance = ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), Threat->GetActorLocation());
			if (Distance < AttackReach && Now >= NextAttack)
			{
				FHitResult Obstacle;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ColonyAttack), true, this);
				Params.AddIgnoredActor(Threat);
				if (!GetWorld()->LineTraceSingleByChannel(Obstacle, GetActorLocation() + GetActorUpVector() * 35,
					Threat->GetActorLocation(), ECC_Visibility, Params))
				{
					if (!MirrorDefense->bEnabled) UGameplayStatics::ApplyDamage(Threat, AttackDamage, nullptr, this, nullptr);
					NextAttack = Now + AttackInterval;
					SetState(EJTSForagerState::Attack, 0);
				}
			}
			else
			{
				if (State != EJTSForagerState::Walk) SetState(EJTSForagerState::Walk, 20);
				MoveTowards(Threat->GetActorLocation(), FleeSpeed, Dt);
			}
		}
		else SetState(EJTSForagerState::Startle, 0);
		return;
	}
	Aggressor.Reset();
	if (bReturningFromFeeding)
	{
		if (RefugeActor.IsValid()) SetState(EJTSForagerState::Flee, 20);
		return;
	}
	if (FeedingTarget.IsValid())
	{
		// Only a visual activity: no resource, health or inventory mutation.
		if (State != EJTSForagerState::Feed && State != EJTSForagerState::Idle)
			SetFeedingTarget(FeedingTarget.Get());
		else if (bEnded)
			SetState(State == EJTSForagerState::Feed ? EJTSForagerState::Idle : EJTSForagerState::Feed,
				State == EJTSForagerState::Feed ? FMath::FRandRange(1.0f, 3.0f) : FMath::FRandRange(4.0f, 9.0f));
		return;
	}
	if (bDefendsNest && FVector::DistSquared(GetActorLocation(), Home) > FMath::Square(HomeArrivalDistance))
	{
		if (State != EJTSForagerState::Walk) SetState(EJTSForagerState::Walk, 10);
		MoveTowards(Home, WalkSpeed, Dt);
		return;
	}
	if (bDefendsNest)
	{
		if (bRepairsRefuge && Habitat.IsValid() && IsValid(Habitat->MaintenanceTarget))
		{
			if (auto* TargetHealth = Habitat->MaintenanceTarget->FindComponentByClass<UJTSHealthComponent>(); TargetHealth && !TargetHealth->IsDead())
			{
				TargetHealth->Heal(Dt * 1.5f);
				if (State != EJTSForagerState::Feed) SetState(EJTSForagerState::Feed, 4);
			}
		}
		else if (State != EJTSForagerState::Idle) SetState(EJTSForagerState::Idle, 4);
		return;
	}
	if (State == EJTSForagerState::Walk)
	{
		if (ActivePlanet->ApproximateSurfaceArcDistance(GetActorLocation(), Goal) < 45)
		{
			bool bAtFood = false;
			if (Habitat.IsValid()) for (const AActor* Food : Habitat->FoodPoints)
				if (IsValid(Food) && FVector::DistSquared(Goal, Food->GetActorLocation()) < FMath::Square(100.0f)) bAtFood = true;
			SetState(bAtFood ? EJTSForagerState::Feed : EJTSForagerState::Idle, FMath::FRandRange(3.0f, 7.0f));
		}
		else if (bEnded || !MoveTowards(Goal, WalkSpeed, Dt)) SetState(EJTSForagerState::Idle, 1);
	}
	else if (bEnded) ChooseNextActivity();
}

void AJTSPlanetSurfaceCreature::OnDamaged(float Current, float Maximum, float Damage, AActor* Causer)
{
	if (!HasAuthority() || Health->IsDead()) return;
	APawn* Pawn = Cast<APawn>(Causer);
	if (!Pawn && IsValid(Causer)) Pawn = Causer->GetInstigator();
	if (MirrorDefense->bEnabled)
	{
		// An active nest alarm owns the larger, mouth-centred leash. A new hit after
		// that alarm ended starts a fresh, smaller, creature-centred retaliation.
		if (Pawn && (!bEntranceDefense || bReturningFromDefense))
		{
			MirrorAggressors.AddUnique(Pawn);
			bMirrorRetaliation = true;
			bEntranceDefense = false;
			bReturningFromDefense = false;
		}
		if (State == EJTSForagerState::Reflect) return;
	}
	MirrorDefense->StopReflection();
	Aggressor = Pawn;
	AggressionOrigin = GetActorLocation();
	SetState(EJTSForagerState::Hit, 0);
}

void AJTSPlanetSurfaceCreature::OnDied(AController* InstigatorController, AActor* Causer)
{
	if (!HasAuthority()) return;
	MirrorDefense->StopReflection();
	if (auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get())) Mouth->Blockade->UnregisterResident(this, true);
	GetWorldTimerManager().ClearTimer(BehaviorTimer);
	SetState(EJTSForagerState::Dead, 0);
	SetLifeSpan(8);
}

bool AJTSPlanetSurfaceCreature::CanReceiveMeleeHit_Implementation(APawn* AttackingPawn) const { return !Health->IsDead() && State != EJTSForagerState::Hidden; }
void AJTSPlanetSurfaceCreature::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type)
{
	if (HasAuthority() && CanReceiveMeleeHit_Implementation(Pawn)) Health->ApplyDamage(Type == EJTSMeleeAttackType::Punch ? 10 : 24, Pawn ? Pawn->GetController() : nullptr, Pawn);
}
FText AJTSPlanetSurfaceCreature::GetMeleeTargetDisplayName_Implementation() const { return DisplayName; }
FText AJTSPlanetSurfaceCreature::GetMeleeTargetPrompt_Implementation(APawn* Pawn) const { return FText::FromString(TEXT("攻击")); }
FVector AJTSPlanetSurfaceCreature::GetMeleeTargetAnchorWorldLocation_Implementation() const { return GetActorLocation() + GetActorUpVector() * 20; }
void AJTSPlanetSurfaceCreature::OnSettlementAttackStarted_Implementation() { if (HasAuthority() && !MirrorDefense->bEnabled) SetState(EJTSForagerState::Attack, 0); }
void AJTSPlanetSurfaceCreature::OnSettlementForceImpact_Implementation(FVector Direction, float Speed) { if (HasAuthority() && !Health->IsDead()) SetState(EJTSForagerState::Hit, 0); }

void AJTSPlanetSurfaceCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && MovementSpeed > 0 && IsAlive())
		bMovementBlocked = !AdvanceMovement(MovementGoal, MovementSpeed, DeltaSeconds);
	FVector Location; FQuat Rotation;
	if (!HasAuthority() && MotionBuffer.Sample(GetWorld()->GetTimeSeconds(), 0.1f, Location, Rotation)) SetActorLocationAndRotation(Location, Rotation, false);
}

void AJTSPlanetSurfaceCreature::OnRep_ReplicatedMovement()
{
	if (HasAuthority() || !GetWorld()) { Super::OnRep_ReplicatedMovement(); return; }
	const FRepMovement& Snapshot = GetReplicatedMovement();
	MotionBuffer.Push(GetWorld()->GetTimeSeconds(), FRepMovement::RebaseOntoLocalOrigin(Snapshot.Location, this), Snapshot.Rotation.Quaternion(), Snapshot.LinearVelocity);
}

void AJTSPlanetSurfaceCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlanetSurfaceCreature, State);
}
