#include "space/World/JTSPlanetColonyNest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSColonyDefenseComponent.h"
#include "GameFramework/Controller.h"
#include "space/World/JTSPlanetEnemySettlement.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSPlanetColonyNest::AJTSPlanetColonyNest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	NestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NestMesh"));
	NestMesh->SetMobility(EComponentMobility::Movable);
	SetRootComponent(NestMesh);
	NestMesh->SetCollisionProfileName(TEXT("BlockAll"));
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	ColonyDefense = CreateDefaultSubobject<UJTSColonyDefenseComponent>(TEXT("ColonyDefense"));
}

void AJTSPlanetColonyNest::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); ApplyAppearance(); }
void AJTSPlanetColonyNest::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && HeightRange.X > 0 && HeightRange.Y >= HeightRange.X)
		ChosenHeight = FMath::FRandRange(HeightRange.X, HeightRange.Y);
	ApplyAppearance();
	if (HasAuthority())
	{
		SetCanBeDamaged(false);
		Health->SetInvulnerable(true);
		ColonyDefense->Initialize(Planet, Workers, Guards);
		if (IsValid(Workers)) Workers->Activate(Planet, SurfaceController);
		if (IsValid(Guards)) Guards->Activate(Planet, SurfaceController);
	}
}
void AJTSPlanetColonyNest::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		if (IsValid(Workers)) Workers->Deactivate();
		if (IsValid(Guards)) Guards->Deactivate();
	}
	Super::EndPlay(Reason);
}
void AJTSPlanetColonyNest::ApplyAppearance()
{
	UStaticMesh* Desired = ClosedMesh;
	if (Desired)
	{
		NestMesh->SetStaticMesh(Desired);
		const float Height = ChosenHeight > 0 ? ChosenHeight : (HeightRange.X + HeightRange.Y) * 0.5f;
		const float Top = Desired->GetBounds().Origin.Z + Desired->GetBounds().BoxExtent.Z;
		if (HeightRange.X > 0 && HeightRange.Y >= HeightRange.X && Top > 0) SetActorScale3D(FVector(Height / Top));
	}
}
float AJTSPlanetColonyNest::TakeDamage(float Damage, const FDamageEvent&, AController* InstigatorController, AActor* Causer)
{
	if (HasAuthority() && Damage > 0)
	{
		APawn* Attacker = Cast<APawn>(Causer);
		if (InstigatorController) Attacker = InstigatorController->GetPawn();
		ColonyDefense->RaiseAlarm(Attacker);
	}
	return 0;
}
bool AJTSPlanetColonyNest::CanReceiveMeleeHit_Implementation(APawn*) const { return true; }
void AJTSPlanetColonyNest::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType)
{
	ColonyDefense->RaiseAlarm(Pawn);
}
FText AJTSPlanetColonyNest::GetMeleeTargetDisplayName_Implementation() const { return FText::FromString(TEXT("塔蚁巢穴")); }
FText AJTSPlanetColonyNest::GetMeleeTargetPrompt_Implementation(APawn*) const { return FText::FromString(TEXT("敲击巢穴会惊动守卫")); }
FVector AJTSPlanetColonyNest::GetMeleeTargetAnchorWorldLocation_Implementation() const { return NestMesh->Bounds.Origin; }
void AJTSPlanetColonyNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSPlanetColonyNest, ChosenHeight);
}
