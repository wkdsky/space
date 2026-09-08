#include "space/Components/JTSHealthComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Net/UnrealNetwork.h"

UJTSHealthComponent::UJTSHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

float UJTSHealthComponent::GetHealth() const
{
	return CurrentHealth;
}

float UJTSHealthComponent::GetMaxHealth() const
{
	return MaxHealth;
}

float UJTSHealthComponent::GetHealthNormalized() const
{
	return MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
		: 0.0f;
}

bool UJTSHealthComponent::IsDead() const
{
	return bIsDead || CurrentHealth <= 0.0f;
}

float UJTSHealthComponent::ApplyDamage(float Damage, AController* InstigatorController, AActor* DamageCauser)
{
	if (!CanModifyHealth() || IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	if (AppliedDamage <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const bool bReachedZero = CurrentHealth <= 0.0f;
	if (bReachedZero)
	{
		bIsDead = true;
	}

	BroadcastHealthChanged();
	OnDamaged.Broadcast(CurrentHealth, MaxHealth, AppliedDamage, DamageCauser);

	if (bReachedZero)
	{
		BroadcastDeathOnce(InstigatorController, DamageCauser);
	}

	return AppliedDamage;
}

float UJTSHealthComponent::Heal(float Amount)
{
	if (!CanModifyHealth() || IsDead() || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.0f, MaxHealth);
	const float HealedAmount = CurrentHealth - PreviousHealth;
	if (HealedAmount > KINDA_SMALL_NUMBER)
	{
		BroadcastHealthChanged();
	}

	return HealedAmount;
}

void UJTSHealthComponent::SetMaxHealth(float NewMaxHealth, bool bFillHealth)
{
	if (!CanModifyHealth() || !FMath::IsFinite(NewMaxHealth))
	{
		return;
	}

	const float PreviousMaxHealth = MaxHealth;
	const float PreviousHealth = CurrentHealth;
	MaxHealth = FMath::Max(0.0f, NewMaxHealth);
	if (bFillHealth && !bIsDead)
	{
		CurrentHealth = MaxHealth;
	}
	else
	{
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	}

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
	}

	if (!FMath::IsNearlyEqual(PreviousMaxHealth, MaxHealth) || !FMath::IsNearlyEqual(PreviousHealth, CurrentHealth))
	{
		BroadcastHealthChanged();
	}
}

void UJTSHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* const Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			MaxHealth = FMath::Max(0.0f, MaxHealth);
			CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
			bIsDead = CurrentHealth <= 0.0f;
		}

		Owner->OnTakeAnyDamage.AddDynamic(this, &UJTSHealthComponent::HandleOwnerTakeAnyDamage);
	}
}

void UJTSHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* const Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UJTSHealthComponent::HandleOwnerTakeAnyDamage);
	}

	Super::EndPlay(EndPlayReason);
}

void UJTSHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UJTSHealthComponent, MaxHealth);
	DOREPLIFETIME(UJTSHealthComponent, CurrentHealth);
	DOREPLIFETIME(UJTSHealthComponent, bIsDead);
}

void UJTSHealthComponent::HandleOwnerTakeAnyDamage(
	AActor* DamagedActor,
	float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	(void)DamageType;

	if (DamagedActor == GetOwner())
	{
		ApplyDamage(Damage, InstigatedBy, DamageCauser);
	}
}

void UJTSHealthComponent::OnRep_MaxHealth()
{
	BroadcastHealthChanged();
}

void UJTSHealthComponent::OnRep_CurrentHealth()
{
	BroadcastHealthChanged();
	if (CurrentHealth <= 0.0f)
	{
		BroadcastDeathOnce(nullptr, nullptr);
	}
}

void UJTSHealthComponent::OnRep_IsDead()
{
	BroadcastHealthChanged();
	if (bIsDead)
	{
		BroadcastDeathOnce(nullptr, nullptr);
	}
}

bool UJTSHealthComponent::CanModifyHealth() const
{
	const AActor* const Owner = GetOwner();
	return Owner == nullptr || Owner->HasAuthority();
}

void UJTSHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UJTSHealthComponent::BroadcastDeathOnce(AController* InstigatorController, AActor* DamageCauser)
{
	if (bDeathBroadcasted)
	{
		return;
	}

	bDeathBroadcasted = true;
	OnDeath.Broadcast(InstigatorController, DamageCauser);
}
