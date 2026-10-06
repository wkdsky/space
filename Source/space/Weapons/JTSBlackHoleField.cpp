#include "space/Weapons/JTSBlackHoleField.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AJTSBlackHoleField::AJTSBlackHoleField()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(2.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AJTSBlackHoleField::Initialize(const FJTSStellarWeaponDefinition& Definition,
	const FJTSStellarWeaponBinding& Binding, FVector SurfaceUp, FVector SurfacePoint)
{
	if (!HasAuthority() || Binding.EffectiveLevels.Num() != 6) return;
	const auto& X = Binding.EffectiveLevels;
	CoreInstanceId = Binding.CoreInstanceId;
	AttachmentInstanceId = Binding.AttachmentInstanceId;
	Visual.EffectClass = Definition.EffectClass;
	Visual.Color = Definition.Color;
	Visual.SurfaceUp = SurfaceUp.GetSafeNormal();
	Visual.Radius = FMath::Max(1.0f, static_cast<float>(Definition.AreaRadiusCentimeters + Definition.RadiusPerLevel * X[1]));
	Visual.CoreRadius = FMath::Max(10.0f, Definition.BlackHoleCoreRadiusCentimeters);
	AttractionCenter = SurfacePoint;
	DamagePerSecond = Definition.BaseDamage * (1.0 + 0.05 * X[0]);
	PullSpeed = Definition.PullSpeed * (1.0 + 0.1 * X[2]);
	Duration = FMath::Clamp(Definition.BlackHoleLifetimeSeconds, 0.2f, 60.0f);
	MaximumTargets = FMath::Clamp(Definition.MaximumTargets, 1, 256);
}

bool AJTSBlackHoleField::BelongsTo(const FJTSStellarWeaponBinding& Binding) const
{
	return CoreInstanceId == Binding.CoreInstanceId && AttachmentInstanceId == Binding.AttachmentInstanceId;
}

void AJTSBlackHoleField::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Visual();
	if (HasAuthority())
	{
		LastPulseTime = GetWorld()->GetTimeSeconds();
		SetLifeSpan(Duration);
		GetWorld()->GetTimerManager().SetTimer(PulseTimer, this, &ThisClass::Pulse, 0.2f, true);
	}
}

void AJTSBlackHoleField::Pulse()
{
	APawn* Pawn = GetInstigator();
	const auto* Health = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	if (!IsValid(Pawn) || (Health && Health->IsDead())) { Destroy(); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const float Delta = FMath::Clamp(static_cast<float>(Now - LastPulseTime), 0.0f, 0.4f);
	LastPulseTime = Now;
	ControlledTargets.RemoveAll([](const auto& Target) { return !Target.IsValid(); });
	TArray<AActor*> Targets;
	UJTSStellarTargetComponent::QueryTargets(GetWorld(), AttractionCenter, Visual.Radius, MaximumTargets, Targets);
	for (AActor* Target : Targets)
	{
		if (!UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), GetActorLocation(), Target, Pawn)) continue;
		UGameplayStatics::ApplyDamage(Target, DamagePerSecond * Delta, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (!IsValid(Target)) continue;
		if (auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>())
		{
			Status->ApplyAttraction(Pawn, this, AttractionCenter, PullSpeed * 3.0f, Visual.Radius, Visual.CoreRadius, 0.3f);
			ControlledTargets.AddUnique(Target);
		}
	}
}

void AJTSBlackHoleField::OnRep_Visual()
{
	if (GetNetMode() == NM_DedicatedServer || !Visual.EffectClass || Visual.Radius <= 0) return;
	if (!IsValid(LocalEffect))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalEffect = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Visual.EffectClass, GetActorLocation(), FRotator::ZeroRotator, Params);
	}
	if (IsValid(LocalEffect)) LocalEffect->UpdatePersistentField(GetActorLocation(), Visual.SurfaceUp, Visual.Radius, Visual.CoreRadius, Visual.Color);
}

void AJTSBlackHoleField::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(PulseTimer);
	for (const auto& Target : ControlledTargets) if (Target.IsValid())
		if (auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>()) Status->RemoveFieldForce(this);
	if (IsValid(LocalEffect)) LocalEffect->Destroy();
	Super::EndPlay(Reason);
}

void AJTSBlackHoleField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSBlackHoleField, Visual);
}
