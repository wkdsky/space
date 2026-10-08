#include "space/Weapons/JTSStellarDrone.h"
#include "space/Weapons/JTSStellarCombat.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
AJTSStellarDrone::AJTSStellarDrone()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true; SetReplicateMovement(true); SetNetUpdateFrequency(15);
	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body")); SetRootComponent(Body); Body->SetSphereRadius(24);
	Body->SetCollisionObjectType(ECC_WorldDynamic); Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCanEverAffectNavigation(false);
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
}
void AJTSStellarDrone::Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding)
{
	Definition = Def; CoreId = Binding.CoreInstanceId; AttachmentId = Binding.AttachmentInstanceId;
	const auto& X = Binding.EffectiveLevels;
	Damage = Def.BaseDamage * (1 + .04 * X[1]); Health->SetMaxHealth(Def.RobotHealth * (1 + .08 * X[2]));
	BlastDamage = Def.RobotExplosionDamage * (1 + .05 * X[4]); BlastRadius = Def.AreaRadiusCentimeters + 20 * X[4];
	Duration = 20 + 2 * X[3];
}
void AJTSStellarDrone::BeginPlay()
{
	Super::BeginPlay(); OnRep_Visual();
	Health->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
	if (HasAuthority()) { SetLifeSpan(Duration); GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Think, .1f, true); }
}
void AJTSStellarDrone::LimitRemainingLife(float Seconds)
{
	if (HasAuthority() && GetLifeSpan() > Seconds) SetLifeSpan(Seconds);
}
void AJTSStellarDrone::Think()
{
	auto* Pawn = GetInstigator();
	const auto* OwnerHealth = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	if (!Pawn || (OwnerHealth && OwnerHealth->IsDead())) { Destroy(); return; }
	const FVector Up = JTSStellarCombat::SurfaceUp(Pawn, GetActorLocation());
	TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), GetActorLocation(), Definition.RangeCentimeters, 24, Targets);
	AActor* Enemy = nullptr;
	for (auto* Target : Targets) if (UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), GetActorLocation(), Target, this)) { Enemy = Target; break; }
	FVector Goal = Pawn->GetActorLocation() + Up * 180;
	if (Enemy) Goal = Enemy->GetActorLocation() + Up * 180 + (GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal() * 220;
	// Keep a stable formation away from the player's capsule, even when all drones share a target.
	const FVector Side = FVector::CrossProduct(Up, Pawn->GetActorForwardVector()).GetSafeNormal();
	Goal += FQuat(Up, GetUniqueID() * 2.39996f).RotateVector(Side) * 110;
	const FVector Next = FMath::VInterpConstantTo(GetActorLocation(), Goal, .1f, 650);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarDroneMove), false, this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, GetActorLocation(), Next, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(24), Params))
		SetActorLocation(Hit.Location + Hit.ImpactNormal * 4);
	else SetActorLocation(Next);
	if (Enemy && GetWorld()->GetTimeSeconds() >= NextShot)
	{
		NextShot = GetWorld()->GetTimeSeconds() + Definition.PrimaryInterval;
		UGameplayStatics::ApplyDamage(Enemy, Damage, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		MulticastShot(Enemy->GetActorLocation());
	}
}
void AJTSStellarDrone::Detonate()
{
	if (!HasAuthority() || IsActorBeingDestroyed() || Health->IsDead()) return;
	JTSStellarCombat::DamageArea(GetInstigator(), GetActorLocation(), BlastRadius, BlastDamage, Definition.MaximumTargets);
	MulticastBurst(); Destroy();
}
void AJTSStellarDrone::HandleDeath(AController*, AActor*) { if (HasAuthority()) Destroy(); }
void AJTSStellarDrone::OnRep_Visual()
{
	if (GetNetMode() == NM_DedicatedServer || !Definition.EffectClass) return;
	if (!IsValid(LocalEffect)) { FActorSpawnParameters P; P.Owner = this; LocalEffect = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Definition.EffectClass, GetActorTransform(), P); }
	if (LocalEffect) LocalEffect->UpdateDrone(GetActorLocation(), Definition.Color);
}
void AJTSStellarDrone::MulticastShot_Implementation(FVector_NetQuantize End)
{
	if (LocalEffect) LocalEffect->UpdateEffect(EJTSStellarWeaponMode::Instance, GetActorLocation(), End, 4, Definition.Color, true, false, 0, false, true);
}
void AJTSStellarDrone::MulticastBurst_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer || !Definition.EffectClass) return;
	FActorSpawnParameters P; P.Owner = GetInstigator();
	if (auto* FX = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Definition.EffectClass, GetActorTransform(), P))
		FX->UpdateArea(EJTSStellarWeaponMode::Instance, GetActorLocation(), JTSStellarCombat::SurfaceUp(GetInstigator(), GetActorLocation()), BlastRadius, Definition.Color, false);
}
void AJTSStellarDrone::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(Timer); if (IsValid(LocalEffect)) LocalEffect->Destroy(); Super::EndPlay(Reason);
}
void AJTSStellarDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AJTSStellarDrone, Definition);
}
