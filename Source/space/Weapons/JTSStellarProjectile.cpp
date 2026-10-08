#include "space/Weapons/JTSStellarProjectile.h"
#include "space/Weapons/JTSStellarCombat.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Weapons/JTSStellarAreaField.h"
#include "space/Components/JTSStellarAbilityComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
AJTSStellarProjectile::AJTSStellarProjectile()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true; SetReplicateMovement(true); SetNetUpdateFrequency(30);
	SetActorEnableCollision(false); SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AJTSStellarProjectile::Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector End, bool Charged)
{
	Definition = Def; CastBinding = Binding; Destination = End; bCharged = Charged;
	Velocity = (End - GetActorLocation()).GetSafeNormal() * Def.ProjectileSpeed;
}
void AJTSStellarProjectile::BeginPlay()
{
	Super::BeginPlay(); OnRep_Visual();
	if (HasAuthority()) { LastStep = GetWorld()->GetTimeSeconds(); SetLifeSpan(5); GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Advance, .025f, true); }
}
void AJTSStellarProjectile::Advance()
{
	if (bDetonated) return;
	if (!IsValid(GetInstigator())) { Destroy(); return; }
	const double Now = GetWorld()->GetTimeSeconds(); const float Delta = FMath::Min(.2, Now - LastStep); LastStep = Now;
	const FVector Start = GetActorLocation(); const FVector End = Start + Velocity * Delta;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarOrb), false, GetInstigator()); Params.AddIgnoredActor(this);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) if (It->Get()) Params.AddIgnoredActor(It->Get()->GetPawn());
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12), Params))
	{
		Detonate(Hit.ImpactPoint + Hit.ImpactNormal * 16, Hit.ImpactNormal); return;
	}
	if (FVector::DotProduct(Destination - End, Velocity) <= 0)
	{
		Detonate(Destination, JTSStellarCombat::SurfaceUp(GetInstigator(), Destination)); return;
	}
	SetActorLocation(End);
}
void AJTSStellarProjectile::LimitRemainingLife(float Seconds)
{
	if (HasAuthority() && GetLifeSpan() > Seconds) SetLifeSpan(Seconds);
}
void AJTSStellarProjectile::Detonate(FVector Point, FVector Normal)
{
	if (!HasAuthority() || bDetonated) return;
	bDetonated = true; GetWorld()->GetTimerManager().ClearTimer(Timer);
	auto* Pawn = GetInstigator(); const auto& X = CastBinding.EffectiveLevels;
	const float Radius = Definition.AreaRadiusCentimeters + 15 * X[1];
	const float Damage = Definition.BaseDamage * (1 + .05 * X[0]) * (bCharged ? 2.5f : 1.f);
	JTSStellarCombat::DamageArea(Pawn, Point, Radius, Damage, Definition.MaximumTargets);
	const int32 Splits = FMath::FloorToInt(X[2] / 3);
	TMap<AActor*, int32> ExtraHits;
	const FVector Up = JTSStellarCombat::SurfaceUp(Pawn, Point);
	const FVector Side = FVector::CrossProduct(Up, Velocity).GetSafeNormal();
	for (int32 I = 0; I < Splits; ++I)
	{
		const FVector Offset = FQuat(Up, I * 2 * PI / FMath::Max(1, Splits)).RotateVector(Side) * Radius * .65f;
		TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Point + Offset, Radius * .65f, Definition.MaximumTargets, Targets);
		for (auto* Target : Targets) if (ExtraHits.FindRef(Target) < 2 && UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Point, Target, Pawn))
		{
			++ExtraHits.FindOrAdd(Target); UGameplayStatics::ApplyDamage(Target, Damage * .25f, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		}
	}
	if (bCharged)
	{
		TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), Point, Radius, Definition.MaximumTargets, Targets);
		for (auto* Target : Targets) if (UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), Point, Target, Pawn))
		{
			auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
			Status->SetStatusPresentation(Definition.TargetStatusEffectClass);
			Status->ApplyCorrosion(Pawn, 0, 3, .1f + .02f * X[4]);
		}
		const FTransform Transform(FQuat::Identity, Point);
		auto* Area = GetWorld()->SpawnActorDeferred<AJTSStellarAreaField>(AJTSStellarAreaField::StaticClass(), Transform, Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Area)
		{
			Area->Initialize(Definition, CastBinding, Up, Radius, Definition.ResidualDuration + .25f * X[3]); Area->FinishSpawning(Transform);
			if (auto* Abilities = Pawn->FindComponentByClass<UJTSStellarAbilityComponent>()) Abilities->TrackArea(Area);
		}
	}
	MulticastDetonate(Point, Normal, Radius); SetLifeSpan(.2f);
}
void AJTSStellarProjectile::OnRep_Visual()
{
	if (GetNetMode() == NM_DedicatedServer || !Definition.EffectClass) return;
	if (!IsValid(LocalEffect)) { FActorSpawnParameters P; P.Owner = this; LocalEffect = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Definition.EffectClass, GetActorTransform(), P); }
	if (LocalEffect) LocalEffect->UpdateProjectile(Definition.Mode, GetActorLocation(), Definition.Color);
}
void AJTSStellarProjectile::MulticastDetonate_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, float Radius)
{
	if (IsValid(LocalEffect)) { LocalEffect->Destroy(); LocalEffect = nullptr; }
	if (GetNetMode() == NM_DedicatedServer || !Definition.EffectClass) return;
	FActorSpawnParameters P; P.Owner = GetInstigator();
	if (auto* Burst = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Definition.EffectClass, FVector(Point), FRotator::ZeroRotator, P))
		Burst->UpdateArea(Definition.Mode, Point, Normal, Radius, Definition.Color, false);
}
void AJTSStellarProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(Timer); if (IsValid(LocalEffect)) LocalEffect->Destroy(); Super::EndPlay(Reason);
}
void AJTSStellarProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSStellarProjectile, Definition);
}
