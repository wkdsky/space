// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSRangedWeaponComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/World/JTSMoonResourceActor.h"
#include "TimerManager.h"

UJTSRangedWeaponComponent::UJTSRangedWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSRangedWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFireTimer();
	bFireHeld = false;
	Super::EndPlay(EndPlayReason);
}

const UJTSItemDefinition* UJTSRangedWeaponComponent::GetActiveRangedDefinition() const
{
	const UJTSInventoryComponent* const Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>()
		: nullptr;
	if (!IsValid(Inventory))
	{
		return nullptr;
	}
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId());
	return IsValid(Definition) && Definition->IsRangedWeapon() ? Definition : nullptr;
}

bool UJTSRangedWeaponComponent::HasActiveRangedWeapon() const
{
	return GetActiveRangedDefinition() != nullptr;
}

void UJTSRangedWeaponComponent::StartFire()
{
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		bFireHeld = true;
		ServerStartFire();
		return;
	}

	bFireHeld = true;
	if (const UJTSItemDefinition* const Definition = GetActiveRangedDefinition())
	{
		FireOnce();
		if (Definition->bAutomaticFire)
		{
			ScheduleAutomaticFire(Definition);
		}
	}
}

void UJTSRangedWeaponComponent::StopFire()
{
	bFireHeld = false;
	ClearFireTimer();
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		ServerStopFire();
	}
}

void UJTSRangedWeaponComponent::ServerStartFire_Implementation()
{
	StartFire();
}

void UJTSRangedWeaponComponent::ServerStopFire_Implementation()
{
	bFireHeld = false;
	ClearFireTimer();
}

bool UJTSRangedWeaponComponent::GetAim(FVector& OutOrigin, FVector& OutDirection) const
{
	APawn* const Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn))
	{
		return false;
	}
	FRotator ViewRotation;
	if (const APlayerController* const Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		Controller->GetPlayerViewPoint(OutOrigin, ViewRotation);
	}
	else
	{
		OutOrigin = Pawn->GetPawnViewLocation();
		ViewRotation = Pawn->GetViewRotation();
	}
	OutDirection = ViewRotation.Vector().GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}

bool UJTSRangedWeaponComponent::FireOnce()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const UJTSInventoryComponent* const Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const UJTSItemDefinition* const Definition = GetActiveRangedDefinition();
	APawn* const Pawn = Cast<APawn>(GetOwner());
	FVector TraceStart = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	if (!IsValid(Inventory) || !IsValid(Definition) || !IsValid(Pawn) || !GetAim(TraceStart, Direction))
	{
		return false;
	}

	const FVector TraceEnd = TraceStart + Direction * FMath::Max(100.0f, Definition->RangedRange);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSRangedShot), false, Pawn);
	QueryParams.AddIgnoredActor(Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	for (AActor* const AttachedActor : AttachedActors)
	{
		QueryParams.AddIgnoredActor(AttachedActor);
	}

	FHitResult Hit;
	FVector PresentationEnd = TraceEnd;
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		PresentationEnd = Hit.ImpactPoint;
		AActor* const HitActor = Hit.GetActor();
		if (AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(HitActor))
		{
			Resource->ApplyMiningWork(Pawn, Inventory->GetActiveItemId(), Definition->MiningWork);
		}
		else if (IsValid(HitActor) && HitActor->FindComponentByClass<UJTSHealthComponent>() != nullptr)
		{
			UGameplayStatics::ApplyDamage(
				HitActor,
				FMath::Max(0.0f, Definition->RangedDamage),
				Pawn->GetController(),
				Pawn,
				UDamageType::StaticClass());
		}
	}

	MulticastShotTrace(TraceStart, PresentationEnd);
	return true;
}

void UJTSRangedWeaponComponent::ScheduleAutomaticFire(const UJTSItemDefinition* Definition)
{
	if (!IsValid(Definition) || GetWorld() == nullptr)
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(
		AutomaticFireTimerHandle,
		[this]()
		{
			const UJTSItemDefinition* const CurrentDefinition = GetActiveRangedDefinition();
			if (!bFireHeld || !IsValid(CurrentDefinition) || !CurrentDefinition->bAutomaticFire)
			{
				ClearFireTimer();
				return;
			}
			FireOnce();
		},
		FMath::Max(0.05f, Definition->RangedFireInterval),
		true);
}

void UJTSRangedWeaponComponent::ClearFireTimer()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
	}
}

void UJTSRangedWeaponComponent::MulticastShotTrace_Implementation(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd)
{
	if (bDebugShotTraces)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor(80, 210, 255), false, 0.18f, 0, 1.2f);
	}
}
