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
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"

AJTSPlanetSurfaceCreature::AJTSPlanetSurfaceCreature()
{
	PrimaryActorTick.bCanEverTick = true;
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
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CreatureMesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	Steering = CreateDefaultSubobject<UJTSPlanetSurfaceSteeringComponent>(TEXT("SurfaceSteering"));
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
	Refuge = RefugeActor.IsValid() ? RefugeActor->GetActorLocation() : RefugeLocation;
	if (!bDefendsNest) Home = Refuge;
	if (!bDefendsNest && RefugeActor.IsValid() && State == EJTSForagerState::Emerge && ActivePlanet.IsValid())
	{
		FJTSPlanetSurfaceFrame Frame;
		if (ActivePlanet->GetSurfaceFrameAt(Refuge, RefugeActor->GetActorForwardVector(), Frame))
			SetActorLocationAndRotation(Frame.Location, Frame.Transform.GetRotation(), false);
	}
}

void AJTSPlanetSurfaceCreature::SetState(EJTSForagerState NewState, float Duration)
{
	State = NewState;
	const TObjectPtr<UAnimSequence>* Clip = Animations.Find(State);
	StateEnd = GetWorld()->GetTimeSeconds() + (Duration > 0 ? Duration : (Clip && *Clip ? (*Clip)->GetPlayLength() : 0.8f));
	Body->ComponentVelocity = FVector::ZeroVector;
	OnRep_State();
	ForceNetUpdate();
}

void AJTSPlanetSurfaceCreature::OnRep_State()
{
	const bool bUnderground = State == EJTSForagerState::Hidden;
	Mesh->SetVisibility(!bUnderground);
	Body->SetCollisionEnabled(bUnderground || State == EJTSForagerState::Dead ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	if (const TObjectPtr<UAnimSequence>* Clip = Animations.Find(State); Clip && *Clip)
	{
		const bool bLoop = State == EJTSForagerState::Idle || State == EJTSForagerState::Walk
			|| State == EJTSForagerState::Feed || State == EJTSForagerState::Flee;
		Mesh->PlayAnimation(*Clip, bLoop);
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
	AJTSPlanetAnchor* Planet = ActivePlanet.Get();
	if (!Planet) return false;
	FVector Ground, Heading;
	if (!Steering->Advance(Planet, GetActorLocation(), Destination - GetActorLocation(), Speed, DeltaSeconds, Ground, Heading)) return false;
	// A species cannot wander out of its authored illumination or activity boundary.
	if (Habitat.IsValid() && !Habitat->ContainsPoint(Ground, Planet)) return false;
	FJTSPlanetSurfaceFrame Frame;
	if (!Planet->GetSurfaceFrameAt(Ground, Heading, Frame)) return false;
	const FVector Previous = GetActorLocation();
	const FQuat Rotation = FQuat::Slerp(GetActorQuat(), Frame.Transform.GetRotation(), FMath::Clamp(DeltaSeconds * 9.0f, 0.0f, 1.0f));
	SetActorLocationAndRotation(Ground, Rotation, false);
	Body->ComponentVelocity = (Ground - Previous) / FMath::Max(DeltaSeconds, 0.001f);
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

void AJTSPlanetSurfaceCreature::UpdateBehavior()
{
	if (!HasAuthority() || !ActivePlanet.IsValid() || Health->IsDead()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Dt = FMath::Clamp(static_cast<float>(Now - LastUpdate), 0.001f, 0.15f);
	LastUpdate = Now;
	if (IsHidden()) return;
	if (Habitat.IsValid() && !bDefendsNest)
	{
		const UJTSHealthComponent* RefugeHealth = RefugeActor.IsValid() ? RefugeActor->FindComponentByClass<UJTSHealthComponent>() : nullptr;
		const auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(RefugeActor.Get());
		if (!RefugeActor.IsValid() || (RefugeHealth && RefugeHealth->IsDead()) || (Mouth && !Mouth->IsRefugeAvailable()))
		{
			RefugeActor = Habitat->FindRefugeActor(GetActorLocation());
			if (RefugeActor.IsValid())
			{
				Refuge = RefugeActor->GetActorLocation(); Home = Refuge;
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
			Refuge = RefugeActor->GetActorLocation(); Home = Refuge;
		}
	}
	const bool bEnded = Now >= StateEnd;
	if (State == EJTSForagerState::Burrow) { if (bEnded) SetState(EJTSForagerState::Hidden, FMath::FRandRange(12.0f, 28.0f)); return; }
	if (State == EJTSForagerState::Hidden) { if (bEnded && !FindThreat()) SetState(EJTSForagerState::Emerge, 0); return; }
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
					UGameplayStatics::ApplyDamage(Threat, AttackDamage, nullptr, this, nullptr);
					NextAttack = Now + 1.8;
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
	Aggressor = Pawn;
	AggressionOrigin = GetActorLocation();
	SetState(EJTSForagerState::Hit, 0);
}

void AJTSPlanetSurfaceCreature::OnDied(AController* InstigatorController, AActor* Causer)
{
	if (!HasAuthority()) return;
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
void AJTSPlanetSurfaceCreature::OnSettlementAttackStarted_Implementation() { if (HasAuthority()) SetState(EJTSForagerState::Attack, 0); }
void AJTSPlanetSurfaceCreature::OnSettlementForceImpact_Implementation(FVector Direction, float Speed) { if (HasAuthority() && !Health->IsDead()) SetState(EJTSForagerState::Hit, 0); }

void AJTSPlanetSurfaceCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
