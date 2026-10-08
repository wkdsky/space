#include "space/Components/JTSStaminaComponent.h"


#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSWallClimbComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerProgressionTypes.h"
#include "space/Player/JTSPlayerState.h"

UJTSStaminaComponent::UJTSStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UJTSStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSStaminaComponent, CurrentStamina);
	DOREPLIFETIME(UJTSStaminaComponent, MaxStamina);
	DOREPLIFETIME(UJTSStaminaComponent, bExhausted);
}

void UJTSStaminaComponent::OnRep_Stamina()
{
	PublishChange();
}

void UJTSStaminaComponent::PublishChange()
{
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

void UJTSStaminaComponent::RefreshCapacity()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority()) return;
	const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	const AJTSPlayerState* State = IsValid(Character) ? Character->GetPlayerState<AJTSPlayerState>() : nullptr;
	const float NewMaximum = FJTSPlayerProgressionRules::GetMaxStamina(
		IsValid(State) ? State->GetAbilityRank(EJTSPlayerAbility::Stamina) : 0);
	if (!FMath::IsNearlyEqual(MaxStamina, NewMaximum))
	{
		const float Difference = NewMaximum - MaxStamina;
		MaxStamina = NewMaximum;
		CurrentStamina = FMath::Clamp(CurrentStamina + FMath::Max(0.0f, Difference), 0.0f, MaxStamina);
		PublishChange();
	}
}

void UJTSStaminaComponent::SetSprintRequested(bool bRequested)
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority()) bSprintRequested = bRequested;
}

bool UJTSStaminaComponent::Spend(float Amount)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || Amount < 0.0f
		|| CurrentStamina + KINDA_SMALL_NUMBER < Amount) return false;
	CurrentStamina = FMath::Max(0.0f, CurrentStamina - Amount);
	RecoveryDelayRemaining = RecoveryDelaySeconds;
	if (CurrentStamina <= KINDA_SMALL_NUMBER) bExhausted = true;
	PublishChange();
	return true;
}

void UJTSStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority()) return;
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const UJTSHealthComponent* Health = Character->GetHealthComponent();
	const UJTSWallClimbComponent* Climb = Character->FindComponentByClass<UJTSWallClimbComponent>();
	const bool bClimbing = IsValid(Climb) && Climb->IsClimbing();
	const bool bAlive = !IsValid(Health) || !Health->IsDead();
	const bool bOnFoot = IsValid(Movement)
		&& (Movement->IsMovingOnGround() || Character->IsSupportedByFloor())
		&& !Character->IsBoarded() && bAlive;
	const bool bMoving = bOnFoot && FVector::VectorPlaneProject(Movement->Velocity,
		-Movement->GetGravityDirection()).SizeSquared() > FMath::Square(30.0f);
	if (bSprintRequested && bMoving && !bExhausted && !bClimbing)
	{
		Spend(FMath::Min(CurrentStamina, SprintDrainPerSecond * DeltaTime));
		return;
	}
	RecoveryDelayRemaining = FMath::Max(0.0f, RecoveryDelayRemaining - DeltaTime);
	if (bOnFoot && !bClimbing && RecoveryDelayRemaining <= 0.0f && CurrentStamina < MaxStamina)
	{
		CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + RecoveryPerSecond * DeltaTime);
		if (bExhausted && CurrentStamina >= MaxStamina * ExhaustionRecoveryFraction) bExhausted = false;
		PublishChange();
	}
}

void UJTSStaminaComponent::Restore(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0) return;
	CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + Amount);
	if (CurrentStamina >= MaxStamina * ExhaustionRecoveryFraction) bExhausted = false;
	PublishChange();
}
