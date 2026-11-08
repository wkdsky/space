#include "space/World/JTSPlanetEcologyHabitat.h"
#include "Components/StaticMeshComponent.h"
#include "space/Components/JTSNestBlockadeComponent.h"
#include "space/Components/JTSMiningImpactComponent.h"
#include "Math/RotationMatrix.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlanetSurfaceScatterComponent.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

AJTSPlanetEcologyHabitat::AJTSPlanetEcologyHabitat()
{
	Blockade = CreateDefaultSubobject<UJTSNestBlockadeComponent>(TEXT("Blockade"));
	SurfaceScatter = CreateDefaultSubobject<UJTSPlanetSurfaceScatterComponent>(TEXT("SurfaceScatter"));
	RefugeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RefugeMesh"));
	RefugeMesh->ComponentTags.Add(TEXT("MiningSurface"));
	MiningImpact = CreateDefaultSubobject<UJTSMiningImpactComponent>(TEXT("MiningImpact"));
	RefugeMesh->SetupAttachment(GetRootComponent());
	RefugeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefugeMesh->SetCanEverAffectNavigation(false);
	EntranceBlocker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntranceBlocker"));
	EntranceBlocker->SetupAttachment(RefugeMesh);
	EntranceBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntranceBlocker->SetCanEverAffectNavigation(false);
	EntranceBlocker->SetVisibility(false);
	bMaintainPopulation = false;
	bReplicates = true;
	EntranceHealth = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("EntranceHealth"));
	EntranceTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceTarget"));
	EntranceTarget->ComponentTags.Add(TEXT("InteractionProxy"));
	EntranceTarget->SetupAttachment(RefugeMesh);
	EntranceTarget->SetCollisionObjectType(ECC_WorldDynamic);
	EntranceTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntranceTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	EntranceTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntranceTarget->SetGenerateOverlapEvents(false);
	EntranceTarget->SetCanEverAffectNavigation(false);
}

void AJTSPlanetEcologyHabitat::BeginPlay()
{
	Super::BeginPlay();
	SetCanBeDamaged(bDestructibleEntrance);
	RefugeMesh->SetCastShadow(false);
	RefugeMesh->SetCollisionObjectType(ECC_WorldDynamic);
	RefugeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RefugeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RefugeMesh->SetCollisionResponseToChannel(ECC_Pawn, bCanBlockEntrance ? ECR_Block : ECR_Ignore);
	RefugeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if ((bDestructibleEntrance || bCanBlockEntrance) && RefugeMesh->GetStaticMesh())
	{
		FBox Bounds = RefugeMesh->GetStaticMesh()->GetBounds().GetBox();
		if (RefugeCutVolume) Bounds += RefugeCutVolume->GetBounds().GetBox();
		EntranceTarget->SetRelativeLocation(Bounds.GetCenter());
		EntranceTarget->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(10)));
		EntranceTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	if (HasAuthority()) EntranceHealth->OnDeath.AddDynamic(this, &ThisClass::OnEntranceDestroyed);
	if (HasAuthority()) Blockade->RestorePersistentState();
	ApplyCollapsed();
}

void AJTSPlanetEcologyHabitat::ApplyCollapsed()
{
	RefugeMesh->SetVisibility(!bCollapsed && !bEntranceManaged && !bEntrancePlacementRejected);
	EntranceBlocker->SetVisibility(bCollapsed && bCanBlockEntrance && bRubbleVisible);
	// The private presentation mesh also supplies the authored small entrance's pawn collision.
	RefugeMesh->SetCollisionResponseToChannel(ECC_Pawn, bCanBlockEntrance ? ECR_Block : ECR_Ignore);
	RefugeMesh->SetCollisionEnabled(bCanBlockEntrance && !bCollapsed && !bEntrancePlacementRejected
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	EntranceBlocker->SetCollisionObjectType(ECC_WorldDynamic);
	EntranceBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntranceBlocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	EntranceBlocker->SetCollisionEnabled(bCollapsed && bCanBlockEntrance && bRubbleVisible
		? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (bCollapsed || bEntrancePlacementRejected) EntranceTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bCollapsed && HasAuthority() && IsValid(EntranceAttachment)) EntranceAttachment->Destroy();
}

bool AJTSPlanetEcologyHabitat::CanInteract_Implementation(APawn* Pawn) const
{
	const auto* Health = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	return IsValid(Pawn) && Pawn->GetWorld() == GetWorld() && (!Health || !Health->IsDead())
		&& bCanBlockEntrance && !bCollapsed && !bEntrancePlacementRejected && RefugeMesh->GetStaticMesh();
}

FVector AJTSPlanetEcologyHabitat::GetInteractionAnchorWorldLocation() const
{
	return RefugeMesh->GetComponentTransform().TransformPosition(EntranceInteractionOffset);
}

FText AJTSPlanetEcologyHabitat::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
	return !CanInteract_Implementation(Pawn) ? FText::GetEmpty() : FText::FromString(Blockade->HasRequiredTool(Pawn)
		? TEXT("[E] 破坏洞口") : TEXT("[E] 破坏洞口（需要镐子）"));
}

float AJTSPlanetEcologyHabitat::GetInteractionHoldDuration_Implementation(APawn* Pawn) const { return FMath::Max(0.5f, BlockHoldDuration); }

bool AJTSPlanetEcologyHabitat::CanExecuteInteraction_Implementation(APawn* Pawn) const { return Blockade->CanWork(Pawn); }
float AJTSPlanetEcologyHabitat::GetSharedInteractionProgress_Implementation() const { return Blockade->GetProgress(); }
void AJTSPlanetEcologyHabitat::SetInteractionHeld_Implementation(APawn* Pawn, bool bHeld) { Blockade->SetWorking(Pawn, bHeld); }
void AJTSPlanetEcologyHabitat::Interact_Implementation(APawn* Pawn) { /* Completion is committed by the server work component. */ }
AActor* AJTSPlanetEcologyHabitat::GetEntranceAttachment() const { return EntranceAttachment; }
void AJTSPlanetEcologyHabitat::ReleaseEntranceAttachment()
{
	if (IsValid(EntranceAttachment)) EntranceAttachment->Destroy();
	EntranceAttachment = nullptr; bEntranceManaged = false; bEntrancePlacementRejected = false;
	ApplyCollapsed();
}
void AJTSPlanetEcologyHabitat::CommitEntranceBlockade(bool bAnimate)
{
	if (!HasAuthority() || !bCanBlockEntrance || bCollapsed) return;
	bCollapsed = true; bRubbleVisible = !bAnimate;
	SetInitialPopulation(0); ApplyCollapsed(); ForceNetUpdate();
}
void AJTSPlanetEcologyHabitat::ShowBlockedRubble()
{
	if (!HasAuthority()) return;
	bRubbleVisible = true; ApplyCollapsed(); ForceNetUpdate();
}
void AJTSPlanetEcologyHabitat::Deactivate()
{
	Blockade->ResetTransientState();
	Super::Deactivate();
}
AActor* AJTSPlanetEcologyHabitat::SpawnSettlementEnemy(const FTransform& Pose, const FVector& Ground)
{
	if (auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(FindRefugeActor(Ground)); Mouth && !Mouth->Blockade->CanReleaseResident()) return nullptr;
	return Super::SpawnSettlementEnemy(Pose, Ground);
}

AJTSPlanetSurfaceCreature* AJTSPlanetEcologyHabitat::SpawnEntranceDefender(int32 Index)
{
	if (!HasAuthority() || bCollapsed || !EcologyPlanet.IsValid()) return nullptr;
	const FTransform MouthPose = RefugeMesh->GetComponentTransform();
	const FVector Heading = MouthPose.TransformVectorNoScale(EntranceExitOffset - EntranceEmergenceOffset).GetSafeNormal();
	// Small alternate lanes share one opening. The cohort is released over time, not stacked at its centre.
	const FVector Lane(0, ((Index % 3) - 1) * 9.0f, 0);
	FJTSPlanetSurfaceFrame Frame;
	FJTSPlanetSurfaceFrame Exit;
	if (!EcologyPlanet->GetSurfaceFrameAt(MouthPose.TransformPosition(EntranceEmergenceOffset + Lane), Heading, Frame)
		|| !EcologyPlanet->GetSurfaceFrameAt(MouthPose.TransformPosition(EntranceExitOffset + Lane), Heading, Exit)) return nullptr;
	if (auto* Creature = Cast<AJTSPlanetSurfaceCreature>(SpawnSettlementEnemy(Frame.Transform, Frame.Location)))
	{
		// ConfigureSpawnedEnemy assigns the refuge and can move ordinary residents. Set the actual mouth pose afterwards.
		Creature->BeginEntranceEmergence(Frame.Transform, Exit.Location);
		return Creature;
	}
	return nullptr;
}

AJTSPlanetSurfaceAttachment* AJTSPlanetEcologyHabitat::CreateEntranceAttachment(AJTSPlanetAnchor* Planet)
{
	if (!HasAuthority() || bCollapsed || !Planet || !RefugeMesh->GetStaticMesh() || IsValid(EntranceAttachment)) return nullptr;
	bEntrancePlacementRejected = false;
	FJTSPlanetSurfaceHit Hit;
	if (!Planet->ProjectPointToSurface(RefugeMesh->GetComponentLocation(), Hit) || !Planet->IsSurfacePlacementAllowed(Hit))
	{
		bEntrancePlacementRejected = true;
		ApplyCollapsed();
		ForceNetUpdate();
		return nullptr;
	}
	FActorSpawnParameters Params; Params.Owner = Planet; Params.OverrideLevel = GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Pose = RefugeMesh->GetComponentTransform();
	EntranceAttachment = GetWorld()->SpawnActor<AJTSPlanetSurfaceAttachment>(AJTSPlanetSurfaceAttachment::StaticClass(), Pose, Params);
	if (!EntranceAttachment) return nullptr;
	bEntranceManaged = true;
	// A sealed, above-ground tunnel keeps its volume; only cut-backed legacy patches are warped per vertex.
	EntranceAttachment->Initialize(RefugeMesh->GetStaticMesh(), Planet, Pose, RefugeCutVolume != nullptr, true, 0, false, RefugeCutVolume);
	EntranceAttachment->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	RefugeMesh->SetVisibility(false);
	ForceNetUpdate();
	return EntranceAttachment;
}

void AJTSPlanetEcologyHabitat::OnEntranceDestroyed(AController* InstigatorController, AActor* Causer)
{
	if (!HasAuthority() || !bDestructibleEntrance) return;
	bCollapsed = true;
	SetInitialPopulation(0);
	ApplyCollapsed();
	ForceNetUpdate();
}

bool AJTSPlanetEcologyHabitat::CanReceiveMeleeHit_Implementation(APawn* Pawn) const { return bDestructibleEntrance && !bCollapsed; }
void AJTSPlanetEcologyHabitat::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type)
{
	if (HasAuthority() && CanReceiveMeleeHit_Implementation(Pawn))
		EntranceHealth->ApplyDamage(Type == EJTSMeleeAttackType::Punch ? 8 : 20, Pawn ? Pawn->GetController() : nullptr, Pawn);
}
FText AJTSPlanetEcologyHabitat::GetMeleeTargetDisplayName_Implementation() const { return FText::FromString(TEXT("巢穴洞口")); }
FText AJTSPlanetEcologyHabitat::GetMeleeTargetPrompt_Implementation(APawn* Pawn) const { return FText::FromString(TEXT("摧毁洞口")); }
FVector AJTSPlanetEcologyHabitat::GetMeleeTargetAnchorWorldLocation_Implementation() const { return GetActorLocation() + GetActorUpVector() * 25; }
void AJTSPlanetEcologyHabitat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlanetEcologyHabitat, bEntranceManaged);
	DOREPLIFETIME(AJTSPlanetEcologyHabitat, bEntrancePlacementRejected);
	DOREPLIFETIME(AJTSPlanetEcologyHabitat, bCollapsed);
	DOREPLIFETIME(AJTSPlanetEcologyHabitat, bRubbleVisible);
}

float AJTSPlanetEcologyHabitat::NormalizedDistance(FVector Point, const AJTSPlanetAnchor* Planet) const
{
	const FVector Up = Planet->GetRadialUpVector(GetActorLocation());
	const FVector Radial = Planet->GetRadialUpVector(Point);
	FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = FVector::VectorPlaneProject(GetActorRightVector(), Up).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
	const FVector Tangent = FVector::VectorPlaneProject(Radial, Up).GetSafeNormal();
	const double Angle = FMath::Atan2(FVector::DotProduct(Tangent, Right), FVector::DotProduct(Tangent, Forward));
	const double Warp = 1 + 0.11 * FMath::Sin(3 * Angle + BoundaryPhase) + 0.065 * FMath::Sin(5 * Angle - BoundaryPhase);
	const double Ellipse = FMath::Sqrt(FMath::Square(FMath::Cos(Angle)) + FMath::Square(FMath::Sin(Angle) / FMath::Clamp(MinorAxisRatio, 0.3f, 1.0f)));
	return Planet->ApproximateSurfaceArcDistance(GetActorLocation(), Point) * Ellipse / Warp;
}

bool AJTSPlanetEcologyHabitat::ContainsPoint(FVector Point, const AJTSPlanetAnchor* Planet, bool bCoreOnly) const
{
	if (!IsValid(Planet)) return false;
	if (Planet->ApproximateSurfaceArcDistance(GetActorLocation(), Point) > SpawnRadius) return false;
	if (LightPreference != EJTSHabitatLight::Any && IsValid(SunLight))
	{
		const double SunDot = FVector::DotProduct(Planet->GetRadialUpVector(Point), -SunLight->GetActorForwardVector());
		if (LightPreference == EJTSHabitatLight::Sunlit && SunDot < TerminatorMargin) return false;
		if (LightPreference == EJTSHabitatLight::Shadow && SunDot > -TerminatorMargin) return false;
	}
	return NormalizedDistance(Point, Planet) <= (bCoreOnly ? CoreRadius : SpawnRadius);
}

bool AJTSPlanetEcologyHabitat::AcceptSpawnPoint_Implementation(FVector Point, AJTSPlanetAnchor* Planet) const
{
	if (bCollapsed || (bStationaryFeeding && !FoodPoints.ContainsByPredicate(
		[](const auto& Food) { return IsValid(Food) && !Food->IsActorBeingDestroyed(); }))) return false;
	if (!ContainsPoint(Point, Planet)) return false;
	FJTSPlanetSurfaceHit Hit;
	if (!Planet->ProjectPointToSurface(Point, Hit) || !Planet->IsSurfacePlacementAllowed(Hit)) return false;
	if (bRequireRefuge && !FindRefugeActor(Point)) return false;
	const float Distance = NormalizedDistance(Point, Planet);
	// The shared template also validates its marker: this exact point is not a fringe sample.
	if (Distance < 1) return true;
	return bFringeOnly ? Distance > CoreRadius : Distance <= CoreRadius;
}

void AJTSPlanetEcologyHabitat::ActivateEcology(AJTSPlanetAnchor* Planet, AActor* Controller)
{
	if (!HasAuthority() || !Planet) return;
	Blockade->RestorePersistentState();
	if (bCollapsed || FMath::FRand() >= ExpeditionChance) return;
	EcologyPlanet = Planet;
	Activate(Planet, Controller);
}

FVector AJTSPlanetEcologyHabitat::FindRefuge(FVector Point) const
{
	const AActor* Refuge = FindRefugeActor(Point);
	return Refuge ? GetRefugeSurfaceLocation(Refuge) : GetSettlementLocation();
}

FVector AJTSPlanetEcologyHabitat::GetSettlementLocation() const { return RefugeMesh->GetStaticMesh() ? RefugeMesh->GetComponentLocation() : GetActorLocation(); }

FVector AJTSPlanetEcologyHabitat::GetRefugeSurfaceLocation(const AActor* Refuge)
{
	if (const auto* Habitat = Cast<AJTSPlanetEcologyHabitat>(Refuge)) return Habitat->GetSettlementLocation();
	return IsValid(Refuge) ? Refuge->GetActorLocation() : FVector::ZeroVector;
}

AActor* AJTSPlanetEcologyHabitat::FindRefugeActor(FVector Point) const
{
	AActor* Result = nullptr;
	double Best = TNumericLimits<double>::Max();
	auto Consider = [&](AActor* Refuge)
	{
		if (!IsValid(Refuge)) return;
		if (const auto* Mouth = Cast<AJTSPlanetEcologyHabitat>(Refuge))
			if (Mouth->bCollapsed || !Mouth->RefugeMesh->GetStaticMesh() || Mouth->EnemyClass != EnemyClass
				|| (Mouth->RefugeCutVolume && (!IsValid(Mouth->EntranceAttachment) || !Mouth->EntranceAttachment->IsRefugeUsable()))) return;
		if (const auto* Attachment = Cast<AJTSPlanetSurfaceAttachment>(Refuge); Attachment && !Attachment->IsRefugeUsable()) return;
		if (const auto* Health = Refuge->FindComponentByClass<UJTSHealthComponent>(); Health && Health->IsDead()) return;
		if (EcologyPlanet.IsValid())
		{
			FJTSPlanetSurfaceHit Hit;
			if (!EcologyPlanet->ProjectPointToSurface(GetRefugeSurfaceLocation(Refuge), Hit) || !EcologyPlanet->IsSurfacePlacementAllowed(Hit)) return;
		}
		if (!bStationaryFeeding && EcologyPlanet.IsValid() && !ContainsPoint(GetRefugeSurfaceLocation(Refuge), EcologyPlanet.Get())) return;
		const double Distance = FVector::DistSquared(Point, GetRefugeSurfaceLocation(Refuge));
		if (Distance < Best) { Best = Distance; Result = Refuge; }
	};
	TSet<const AJTSPlanetEcologyHabitat*> Visited;
	TFunction<void(const AJTSPlanetEcologyHabitat*)> Visit = [&](const AJTSPlanetEcologyHabitat* Provider)
	{
		if (!IsValid(Provider) || Provider->bCollapsed || Visited.Contains(Provider)) return;
		Visited.Add(Provider);
		// Providers may contain residents of several species. Only their own species' refuges are eligible.
		if (Provider->EnemyClass == EnemyClass)
			for (AActor* Refuge : Provider->Refuges) Consider(Refuge);
		for (const AJTSPlanetEcologyHabitat* Child : Provider->RefugeProviders) Visit(Child);
	};
	Visit(this);
	return Result;
}

void AJTSPlanetEcologyHabitat::ConfigureSpawnedEnemy_Implementation(AActor* Enemy)
{
	if (auto* Creature = Cast<AJTSPlanetSurfaceCreature>(Enemy))
	{
		Creature->SetHabitat(this, FindRefuge(Creature->GetActorLocation()));
		if (Creature->IsActorBeingDestroyed()) return;
		if (bStationaryFeeding && !FoodPoints.IsEmpty()) Creature->SetFeedingTarget(FoodPoints[0]);
	}
}
