#include "space/World/JTSPlanetEcologyHabitat.h"
#include "Components/StaticMeshComponent.h"
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
	SurfaceScatter = CreateDefaultSubobject<UJTSPlanetSurfaceScatterComponent>(TEXT("SurfaceScatter"));
	RefugeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RefugeMesh"));
	RefugeMesh->SetupAttachment(GetRootComponent());
	RefugeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RefugeMesh->SetCanEverAffectNavigation(false);
	bMaintainPopulation = false;
	bReplicates = true;
	EntranceHealth = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("EntranceHealth"));
	EntranceTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("EntranceTarget"));
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
	RefugeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bDestructibleEntrance && RefugeMesh->GetStaticMesh())
	{
		FBox Bounds = RefugeMesh->CalcBounds(FTransform::Identity).GetBox();
		if (RefugeCutVolume) Bounds += RefugeCutVolume->GetBounds().GetBox();
		EntranceTarget->SetRelativeLocation(Bounds.GetCenter());
		EntranceTarget->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(10)));
		EntranceTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	if (HasAuthority()) EntranceHealth->OnDeath.AddDynamic(this, &ThisClass::OnEntranceDestroyed);
	ApplyCollapsed();
}

void AJTSPlanetEcologyHabitat::ApplyCollapsed()
{
	RefugeMesh->SetVisibility(!bCollapsed && !RefugeCutVolume);
	if (bCollapsed) EntranceTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bCollapsed && HasAuthority() && IsValid(EntranceAttachment)) EntranceAttachment->Destroy();
}

AJTSPlanetSurfaceAttachment* AJTSPlanetEcologyHabitat::CreateEntranceAttachment(AJTSPlanetAnchor* Planet)
{
	if (!HasAuthority() || bCollapsed || !Planet || !RefugeCutVolume || !RefugeMesh->GetStaticMesh() || IsValid(EntranceAttachment)) return nullptr;
	FActorSpawnParameters Params; Params.Owner = Planet; Params.OverrideLevel = GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Pose = RefugeMesh->GetComponentTransform();
	EntranceAttachment = GetWorld()->SpawnActor<AJTSPlanetSurfaceAttachment>(AJTSPlanetSurfaceAttachment::StaticClass(), Pose, Params);
	if (!EntranceAttachment) return nullptr;
	EntranceAttachment->Initialize(RefugeMesh->GetStaticMesh(), Planet, Pose, true, true, 0, false, RefugeCutVolume);
	EntranceAttachment->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	RefugeMesh->SetVisibility(false);
	return EntranceAttachment;
}

void AJTSPlanetEcologyHabitat::OnEntranceDestroyed(AController* InstigatorController, AActor* Causer)
{
	if (!HasAuthority() || !bDestructibleEntrance) return;
	bCollapsed = true;
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
	DOREPLIFETIME(AJTSPlanetEcologyHabitat, bCollapsed);
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
	if (!ContainsPoint(Point, Planet)) return false;
	if (bRequireRefuge && !FindRefugeActor(Point)) return false;
	const float Distance = NormalizedDistance(Point, Planet);
	// The shared template also validates its marker: this exact point is not a fringe sample.
	if (Distance < 1) return true;
	return bFringeOnly ? Distance > CoreRadius : Distance <= CoreRadius;
}

void AJTSPlanetEcologyHabitat::ActivateEcology(AJTSPlanetAnchor* Planet, AActor* Controller)
{
	if (!HasAuthority() || !Planet || bCollapsed || FMath::FRand() >= ExpeditionChance) return;
	EcologyPlanet = Planet;
	Activate(Planet, Controller);
}

FVector AJTSPlanetEcologyHabitat::FindRefuge(FVector Point) const
{
	const AActor* Refuge = FindRefugeActor(Point);
	return Refuge ? Refuge->GetActorLocation() : GetActorLocation();
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
		if (EcologyPlanet.IsValid() && !ContainsPoint(Refuge->GetActorLocation(), EcologyPlanet.Get())) return;
		const double Distance = FVector::DistSquared(Point, Refuge->GetActorLocation());
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
	if (auto* Creature = Cast<AJTSPlanetSurfaceCreature>(Enemy)) Creature->SetHabitat(this, FindRefuge(Creature->GetActorLocation()));
}
