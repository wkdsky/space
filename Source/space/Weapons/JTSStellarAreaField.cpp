#include "space/Weapons/JTSStellarAreaField.h"
#include "space/Weapons/JTSStellarEffectActor.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
AJTSStellarAreaField::AJTSStellarAreaField()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true; SetReplicateMovement(true);
	SetActorEnableCollision(false); SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AJTSStellarAreaField::Initialize(const FJTSStellarWeaponDefinition& Def, const FJTSStellarWeaponBinding& Binding, FVector Up, float Radius, float Seconds)
{
	Definition = Def; CoreId = Binding.CoreInstanceId; AttachmentId = Binding.AttachmentInstanceId;
	SurfaceNormal = Up; FieldRadius = Radius; Duration = Seconds;
}
void AJTSStellarAreaField::BeginPlay()
{
	Super::BeginPlay(); OnRep_Presentation();
	if (HasAuthority()) { SetLifeSpan(Duration); LastPulse = GetWorld()->GetTimeSeconds(); GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Pulse, .2f, true); }
}
void AJTSStellarAreaField::Pulse()
{
	auto* Pawn = GetInstigator();
	const auto* Health = IsValid(Pawn) ? Pawn->FindComponentByClass<UJTSHealthComponent>() : nullptr;
	if (!Pawn || (Health && Health->IsDead())) { Destroy(); return; }
	const double Now = GetWorld()->GetTimeSeconds(); const float Delta = FMath::Min(.4, Now - LastPulse); LastPulse = Now;
	TArray<AActor*> Targets; UJTSStellarTargetComponent::QueryTargets(GetWorld(), GetActorLocation(), FieldRadius, Definition.MaximumTargets, Targets);
	for (auto* Target : Targets) if (UJTSStellarTargetComponent::HasLineOfSight(GetWorld(), GetActorLocation() + SurfaceNormal * 20, Target, Pawn))
	{
		UGameplayStatics::ApplyDamage(Target, Definition.StatusDamagePerSecond * Delta, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (IsValid(Target))
		{
			auto* Status = Target->FindComponentByClass<UJTSStellarTargetComponent>();
			Status->SetStatusPresentation(Definition.TargetStatusEffectClass);
			Status->ApplyFire(Pawn, 35 * Delta, Definition.StatusDamagePerSecond, 0);
		}
	}
}
void AJTSStellarAreaField::OnRep_Presentation()
{
	if (GetNetMode() == NM_DedicatedServer || !Definition.EffectClass) return;
	if (!IsValid(LocalEffect)) { FActorSpawnParameters P; P.Owner = this; LocalEffect = GetWorld()->SpawnActor<AJTSStellarEffectActor>(Definition.EffectClass, GetActorTransform(), P); }
	if (LocalEffect) LocalEffect->UpdateArea(Definition.Mode, GetActorLocation(), SurfaceNormal, FieldRadius, Definition.Color, true);
}
void AJTSStellarAreaField::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(Timer); if (IsValid(LocalEffect)) LocalEffect->Destroy(); Super::EndPlay(Reason);
}
void AJTSStellarAreaField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSStellarAreaField, Definition); DOREPLIFETIME(AJTSStellarAreaField, SurfaceNormal); DOREPLIFETIME(AJTSStellarAreaField, FieldRadius);
}
