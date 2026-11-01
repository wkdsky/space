#include "space/World/JTSPlanetColonyNest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
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
}

void AJTSPlanetColonyNest::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); ApplyStage(); }
void AJTSPlanetColonyNest::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && HeightRange.X > 0 && HeightRange.Y >= HeightRange.X)
		ChosenHeight = FMath::FRandRange(HeightRange.X, HeightRange.Y);
	ApplyStage();
	if (HasAuthority())
	{
		Health->SetMaxHealth(Health->GetMaxHealth(), true);
		Health->OnDamaged.AddDynamic(this, &ThisClass::OnDamaged);
		Health->OnDeath.AddDynamic(this, &ThisClass::OnDied);
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
void AJTSPlanetColonyNest::ApplyStage()
{
	UStaticMesh* Desired = Stage >= 2 ? BrokenMesh : (Stage == 1 ? DamagedMesh : ClosedMesh);
	if (Desired)
	{
		NestMesh->SetStaticMesh(Desired);
		const float Height = ChosenHeight > 0 ? ChosenHeight : (HeightRange.X + HeightRange.Y) * 0.5f;
		const float Top = Desired->GetBounds().Origin.Z + Desired->GetBounds().BoxExtent.Z;
		if (HeightRange.X > 0 && HeightRange.Y >= HeightRange.X && Top > 0) SetActorScale3D(FVector(Height / Top));
	}
}
void AJTSPlanetColonyNest::OnDamaged(float Current, float Maximum, float Damage, AActor* Causer)
{
	if (!HasAuthority()) return;
	const uint8 Desired = Current <= Maximum * 0.4f ? 2 : 1;
	if (Desired > Stage)
	{
		Stage = Desired;
		ApplyStage();
		if (IsValid(Workers)) Workers->Activate(Planet, SurfaceController);
		if (Stage >= 2 && IsValid(Guards)) Guards->Activate(Planet, SurfaceController);
		ForceNetUpdate();
	}
}
void AJTSPlanetColonyNest::OnDied(AController* InstigatorController, AActor* Causer) { if (HasAuthority()) { Stage = 2; ApplyStage(); ForceNetUpdate(); } }
bool AJTSPlanetColonyNest::CanReceiveMeleeHit_Implementation(APawn* Pawn) const { return !Health->IsDead(); }
void AJTSPlanetColonyNest::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type)
{
	if (HasAuthority() && !Health->IsDead()) Health->ApplyDamage(Type == EJTSMeleeAttackType::Punch ? 8 : 20, Pawn ? Pawn->GetController() : nullptr, Pawn);
}
FText AJTSPlanetColonyNest::GetMeleeTargetDisplayName_Implementation() const { return FText::FromString(TEXT("封闭巢塔")); }
FText AJTSPlanetColonyNest::GetMeleeTargetPrompt_Implementation(APawn* Pawn) const { return FText::FromString(TEXT("敲击巢壳")); }
FVector AJTSPlanetColonyNest::GetMeleeTargetAnchorWorldLocation_Implementation() const { return NestMesh->Bounds.Origin; }
void AJTSPlanetColonyNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSPlanetColonyNest, Stage); DOREPLIFETIME(AJTSPlanetColonyNest, ChosenHeight); }
