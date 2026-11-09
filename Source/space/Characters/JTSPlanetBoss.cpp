#include "space/Characters/JTSPlanetBoss.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSBossEmergenceFeedbackComponent.h"
#include "space/Components/JTSBossDeathPresentationComponent.h"
#include "space/Components/JTSPlanetSurfaceSteeringComponent.h"
#include "space/UI/JTSBossHealthWidget.h"

AJTSPlanetBoss::AJTSPlanetBoss()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30);
	SetNetCullDistanceSquared(FMath::Square(30000.f));
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BossMesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bEnableUpdateRateOptimizations = false;
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	Combat = CreateDefaultSubobject<UJTSBossCombatComponent>(TEXT("Combat"));
	CreateDefaultSubobject<UJTSBossEmergenceFeedbackComponent>(TEXT("EmergenceFeedback"));
	CreateDefaultSubobject<UJTSBossDeathPresentationComponent>(TEXT("DeathPresentation"));
	Steering = CreateDefaultSubobject<UJTSPlanetSurfaceSteeringComponent>(TEXT("SurfaceSteering"));
	Steering->BodyRadius = 115;
	Steering->BodyGroundClearance = 55;
	Steering->ProbeDistance = 100;
	Steering->MaxSlopeDegrees = 48;
}

void AJTSPlanetBoss::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(!HasAuthority());
	if (const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset())
	{
		const auto& Skeleton = Asset->GetRefSkeleton();
		for (int32 Index = 0; Index < HitZones.Num(); ++Index)
		{
			const auto& Zone = HitZones[Index];
			const int32 Bone = Skeleton.FindBoneIndex(Zone.Bone);
			if (Bone == INDEX_NONE) continue;
			FTransform Reference = Skeleton.GetRefBonePose()[Bone];
			for (int32 Parent = Skeleton.GetParentIndex(Bone); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
				Reference *= Skeleton.GetRefBonePose()[Parent];
			auto* Box = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("BossHit_%d"), Index));
			Box->SetupAttachment(Mesh, Zone.Bone);
			Box->SetRelativeTransform(FTransform(Zone.Center).GetRelativeTransform(Reference));
			Box->SetBoxExtent(Zone.HalfExtent);
			Box->SetCollisionObjectType(ECC_WorldDynamic);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->SetCollisionResponseToChannel(ECC_Pawn, Zone.bBlocksMovement ? ECR_Block : ECR_Ignore);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetGenerateOverlapEvents(false);
			Box->SetCanEverAffectNavigation(false);
			Box->ComponentTags.Add(*FString::FromInt(Index));
			Box->RegisterComponent();
			DamageBodies.Add(Box);
		}
	}
	if (HasAuthority())
	{
		Health->OnDeath.AddDynamic(this, &ThisClass::OnDied);
		Health->OnDamaged.AddDynamic(this, &ThisClass::OnDamaged);
	}
	RefreshAction();
	if (GetNetMode() != NM_DedicatedServer && GetGameInstance())
		for (ULocalPlayer* Local : GetGameInstance()->GetLocalPlayers())
			if (APlayerController* PC = Local->GetPlayerController(GetWorld()))
			{
				auto* Hud = CreateWidget<UJTSBossHealthWidget>(PC);
				Hud->SetBoss(this);
				Hud->AddToPlayerScreen(12);
				HudWidgets.Add(Hud);
			}
}

void AJTSPlanetBoss::InitializeOnPlanet(AJTSPlanetAnchor* Planet)
{
	if (!HasAuthority() || !Planet) return;
	int32 Players = 0;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (It->Get() && It->Get()->GetPawn()) ++Players;
	Health->SetMaxHealth(Health->GetMaxHealth() + ExtraPlayerHealth * FMath::Clamp(Players - 1, 0, 3), true);
	Combat->Initialize(Planet);
}

float AJTSPlanetBoss::TakeDamage(float Damage, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
	if (!HasAuthority() || Health->IsDead() || !Combat->IsDamageable() || Damage <= 0) return 0;
	float Multiplier = 1;
	const UPrimitiveComponent* HitComponent = nullptr;
	if (Event.IsOfType(FPointDamageEvent::ClassID))
		HitComponent = static_cast<const FPointDamageEvent&>(Event).HitInfo.GetComponent();
	// Melee's authoritative contact already validates reach/occlusion. Resolve the nearest moving part.
	float Best = TNumericLimits<float>::Max();
	for (const UBoxComponent* Box : DamageBodies)
	{
		if (!Box || Box->ComponentTags.IsEmpty()) continue;
		const int32 Index = FCString::Atoi(*Box->ComponentTags[0].ToString());
		if (!HitZones.IsValidIndex(Index)) continue;
		float Distance = 0;
		if (HitComponent) { if (HitComponent != Box) continue; }
		else if (Causer)
		{
			const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Causer->GetActorLocation() + Causer->GetActorUpVector() * 50);
			Distance = (Local.GetAbs() - Box->GetUnscaledBoxExtent()).ComponentMax(FVector::ZeroVector).SizeSquared();
		}
		if (Distance < Best) { Best = Distance; Multiplier = HitZones[Index].DamageMultiplier; }
	}
	if (Combat->IsVulnerable()) Multiplier *= 1.35f;
	return Super::TakeDamage(Damage * Multiplier, Event, EventInstigator, Causer);
}

void AJTSPlanetBoss::RefreshAction()
{
	const auto Action = Combat->GetAction();
	if (const auto* Clip = Animations.Find(Action); Clip && *Clip)
	{
		const bool bLoop = Action == EJTSBossAction::Idle || Action == EJTSBossAction::Walk || Action == EJTSBossAction::TurnLeft || Action == EJTSBossAction::TurnRight;
		Mesh->PlayAnimation(*Clip, bLoop);
		const float Duration = Combat->GetActionDuration();
		const float Rate = Duration > 0 ? (*Clip)->GetPlayLength() / Duration : 1;
		Mesh->SetPlayRate(Rate);
		const float Position = Combat->GetActionElapsed() * Rate;
		Mesh->SetPosition(bLoop ? FMath::Fmod(Position, (*Clip)->GetPlayLength()) : FMath::Min(Position, (*Clip)->GetPlayLength()), false);
	}
	if (Action == EJTSBossAction::Death)
	{
		for (UBoxComponent* Body : DamageBodies) Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FindComponentByClass<UJTSBossDeathPresentationComponent>()->BeginDeath();
	}
	FindComponentByClass<UJTSBossEmergenceFeedbackComponent>()->RefreshAction();
}

void AJTSPlanetBoss::OnDied(AController*, AActor*) { Combat->Die(); }
void AJTSPlanetBoss::OnDamaged(float, float, float Damage, AActor*) { Combat->ReactToDamage(Damage); }
bool AJTSPlanetBoss::CanReceiveMeleeHit_Implementation(APawn*) const { return !Health->IsDead() && Combat->IsDamageable(); }
void AJTSPlanetBoss::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type)
{
	if (HasAuthority()) UGameplayStatics::ApplyDamage(this, Type == EJTSMeleeAttackType::Punch ? 8 : 20, Pawn ? Pawn->GetController() : nullptr, Pawn, nullptr);
}
FText AJTSPlanetBoss::GetMeleeTargetPrompt_Implementation(APawn*) const { return FText::FromString(TEXT("攻击镜虫首领")); }
FVector AJTSPlanetBoss::GetMeleeTargetAnchorWorldLocation_Implementation() const { return Mesh->GetSocketLocation(TEXT("head")); }
void AJTSPlanetBoss::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UJTSBossHealthWidget* Hud : HudWidgets) if (Hud) Hud->RemoveFromParent();
	Super::EndPlay(Reason);
}
void AJTSPlanetBoss::Tick(float Dt)
{
	Super::Tick(Dt);
	FVector Location; FQuat Rotation;
	if (!HasAuthority() && MotionBuffer.Sample(GetWorld()->GetTimeSeconds(), .08f, Location, Rotation))
		SetActorLocationAndRotation(Location, Rotation, false);
}
void AJTSPlanetBoss::OnRep_ReplicatedMovement()
{
	if (HasAuthority() || !GetWorld()) { Super::OnRep_ReplicatedMovement(); return; }
	const FRepMovement& Snapshot = GetReplicatedMovement();
	MotionBuffer.Push(GetWorld()->GetTimeSeconds(), FRepMovement::RebaseOntoLocalOrigin(Snapshot.Location, this), Snapshot.Rotation.Quaternion(), Snapshot.LinearVelocity);
}
