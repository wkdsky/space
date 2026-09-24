// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSRangedWeaponComponent.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Weapons/JTSProjectileActor.h"
#include "space/Weapons/JTSProjectileImpactActor.h"
#include "space/World/JTSMoonResourceActor.h"

UJTSRangedWeaponComponent::UJTSRangedWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSRangedWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFireTimer();
	bFireHeld = false;
	bIsAiming = false;
	Super::EndPlay(EndPlayReason);
}

const UJTSItemDefinition* UJTSRangedWeaponComponent::GetActiveRangedDefinition() const
{
	const UJTSInventoryComponent* Inventory = GetOwner() != nullptr
		? GetOwner()->FindComponentByClass<UJTSInventoryComponent>() : nullptr;
	if (!IsValid(Inventory)) return nullptr;
	const UJTSItemDefinition* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId());
	return IsValid(Definition) && Definition->IsRangedWeapon() ? Definition : nullptr;
}

bool UJTSRangedWeaponComponent::CanUseWeapon() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return false;
	if (const AJTSCharacter* Character = Cast<AJTSCharacter>(Pawn))
	{
		if (Character->IsBoarded()) return false;
	}
	const UJTSHealthComponent* Health = Pawn->FindComponentByClass<UJTSHealthComponent>();
	return !IsValid(Health) || !Health->IsDead();
}

bool UJTSRangedWeaponComponent::HasActiveRangedWeapon() const
{
	return GetActiveRangedDefinition() != nullptr;
}

float UJTSRangedWeaponComponent::GetActiveAimFOV() const
{
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	return IsValid(Definition) ? FMath::Clamp(Definition->RangedAimFOV, 30.0f, 120.0f) : 60.0f;
}

void UJTSRangedWeaponComponent::StartAim()
{
	if (!CanUseWeapon()) return;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		if (HasActiveRangedWeapon())
		{
			bIsAiming = true;
			ServerStartAim();
		}
		return;
	}
	bIsAiming = HasActiveRangedWeapon();
}

void UJTSRangedWeaponComponent::StopAim()
{
	bIsAiming = false;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority()) ServerStopAim();
}

void UJTSRangedWeaponComponent::StartFire()
{
	if (bFireHeld) return;
	if (!CanUseWeapon()) return;
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	if (!IsValid(Definition)) return;
	bFireHeld = true;
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority())
	{
		PlayLocalShotFeedback(Definition);
		ServerStartFire();
	}
	else
	{
		FireOnce();
	}
	// Holding attack repeats every ranged weapon at its own fire interval; a tap still fires once.
	ScheduleHeldFire(Definition);
}

void UJTSRangedWeaponComponent::StopFire()
{
	bFireHeld = false;
	ClearFireTimer();
	if (GetOwner() != nullptr && !GetOwner()->HasAuthority()) ServerStopFire();
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

void UJTSRangedWeaponComponent::ServerStartAim_Implementation()
{
	StartAim();
}

void UJTSRangedWeaponComponent::ServerStopAim_Implementation()
{
	bIsAiming = false;
}

bool UJTSRangedWeaponComponent::GetAim(FVector& OutOrigin, FVector& OutDirection) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return false;
	FRotator ViewRotation;
	if (const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
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
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetWorld() == nullptr || !CanUseWeapon()) return false;
	const UJTSInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const UJTSItemDefinition* Definition = GetActiveRangedDefinition();
	APawn* Pawn = Cast<APawn>(GetOwner());
	FVector CameraStart = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	if (!IsValid(Inventory) || !IsValid(Definition) || !IsValid(Pawn)
		|| !GetAim(CameraStart, Direction)) return false;

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < NextFireTimeSeconds) return false;
	NextFireTimeSeconds = Now + FMath::Max(0.05f, Definition->RangedFireInterval);

	const float SpreadDegrees = bIsAiming ? Definition->RangedAimSpreadDegrees : Definition->RangedHipSpreadDegrees;
	Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(FMath::Clamp(SpreadDegrees, 0.0f, 12.0f)));
	const FVector CameraEnd = CameraStart + Direction * FMath::Max(100.0f, Definition->RangedRange);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JTSRangedShot), false, Pawn);
	QueryParams.AddIgnoredActor(Pawn);
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors) QueryParams.AddIgnoredActor(AttachedActor);

	FHitResult CameraHit;
	GetWorld()->LineTraceSingleByChannel(CameraHit, CameraStart, CameraEnd, ECC_Visibility, QueryParams);
	const FVector AimPoint = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraEnd;
	FVector MuzzleStart = CameraStart + Direction * 70.0f;
	if (const UJTSWeaponVisualComponent* Visual = Pawn->FindComponentByClass<UJTSWeaponVisualComponent>())
	{
		FVector Candidate;
		if (Visual->GetMuzzleWorldLocation(Candidate)
			&& FVector::DistSquared(Candidate, Pawn->GetActorLocation()) < FMath::Square(350.0f))
		{
			MuzzleStart = Candidate;
		}
	}

	// A short muzzle check prevents a third-person camera from shooting through cover beside the gun.
	FHitResult MuzzleHit;
	GetWorld()->LineTraceSingleByChannel(MuzzleHit, MuzzleStart, AimPoint, ECC_Visibility, QueryParams);
	FHitResult Hit = CameraHit;
	if (MuzzleHit.bBlockingHit && (!CameraHit.bBlockingHit
		|| FVector::DistSquared(MuzzleStart, MuzzleHit.ImpactPoint)
			< FVector::DistSquared(MuzzleStart, CameraHit.ImpactPoint) - FMath::Square(2.0f)))
	{
		Hit = MuzzleHit;
	}
	const bool bHitSomething = Hit.bBlockingHit;
	const FVector PresentationEnd = bHitSomething ? Hit.ImpactPoint : AimPoint;
	const FVector ImpactNormal = bHitSomething && !Hit.ImpactNormal.IsNearlyZero()
		? Hit.ImpactNormal.GetSafeNormal() : -Direction;
	AActor* HitActor = bHitSomething ? Hit.GetActor() : nullptr;
	bool bDamageableHit = false;
	if (AJTSMoonResourceActor* Resource = Cast<AJTSMoonResourceActor>(HitActor))
	{
		Resource->ApplyMiningWork(Pawn, Inventory->GetActiveItemId(), Definition->MiningWork);
	}
	else if (IsValid(HitActor) && HitActor->FindComponentByClass<UJTSHealthComponent>() != nullptr)
	{
		bDamageableHit = Definition->RangedDamage > 0.0f;
		UGameplayStatics::ApplyDamage(HitActor, FMath::Max(0.0f, Definition->RangedDamage),
			Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (bDamageableHit) ClientConfirmRangedHit();
	}
	MulticastShotTrace(MuzzleStart, PresentationEnd, ImpactNormal, Definition->ItemId,
		bHitSomething, bDamageableHit);
	return true;
}

void UJTSRangedWeaponComponent::ScheduleHeldFire(const UJTSItemDefinition* Definition)
{
	if (!IsValid(Definition) || GetWorld() == nullptr) return;
	const EJTSItemId StartedItem = Definition->ItemId;
	GetWorld()->GetTimerManager().SetTimer(AutomaticFireTimerHandle, [this, StartedItem]()
	{
		const UJTSItemDefinition* Current = GetActiveRangedDefinition();
		if (!bFireHeld || !CanUseWeapon() || !IsValid(Current) || Current->ItemId != StartedItem)
		{
			bFireHeld = false;
			ClearFireTimer();
			return;
		}
		if (GetOwner() != nullptr && GetOwner()->HasAuthority()) FireOnce();
		else PlayLocalShotFeedback(Current);
	}, FMath::Max(0.05f, Definition->RangedFireInterval), true);
}

void UJTSRangedWeaponComponent::ClearFireTimer()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
}

void UJTSRangedWeaponComponent::PlayLocalShotFeedback(const UJTSItemDefinition* Definition)
{
	if (!IsValid(Definition) || GetWorld() == nullptr) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now + KINDA_SMALL_NUMBER < NextLocalFeedbackTimeSeconds) return;
	NextLocalFeedbackTimeSeconds = Now + FMath::Max(0.05f, Definition->RangedFireInterval);
	LastLocalShotSeconds = Now;
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return;
	const FLinearColor ShotColor = Definition->AccentColor;
	UMaterialInterface* Glow = Definition->RangedGlowMaterial.LoadSynchronous();
	FVector SoundOrigin = Pawn->GetActorLocation();
	if (UJTSWeaponVisualComponent* Visual = Pawn->FindComponentByClass<UJTSWeaponVisualComponent>())
	{
		FVector MuzzleLocation;
		if (Visual->GetMuzzleWorldLocation(MuzzleLocation)) SoundOrigin = MuzzleLocation;
		Visual->PlayShotPresentation(Glow, ShotColor);
	}
	if (Pawn->IsLocallyControlled())
	{
		if (AJTSCharacter* Character = Cast<AJTSCharacter>(Pawn))
		{
			Character->ApplyWeaponViewKick(FMath::Clamp(Definition->RangedViewKickDegrees, 0.0f, 8.0f));
		}
	}
	if (USoundBase* FireSound = Definition->RangedFireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, SoundOrigin, 0.9f,
			FMath::FRandRange(0.97f, 1.03f));
	}
}

void UJTSRangedWeaponComponent::MulticastShotTrace_Implementation(FVector_NetQuantize MuzzleStart,
	FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal ImpactNormal, EJTSItemId ShotItem,
	bool bHitSomething, bool bDamageableHit)
{
	if (GetWorld() == nullptr) return;
	APawn* Pawn = Cast<APawn>(GetOwner());
	const UJTSItemDefinition* Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ShotItem);
	const FLinearColor ShotColor = IsValid(Definition) ? Definition->AccentColor : FLinearColor(1.0f, 0.7f, 0.3f);
	UMaterialInterface* Glow = IsValid(Definition) ? Definition->RangedGlowMaterial.LoadSynchronous() : nullptr;
	// The owning client already played the trigger, kick, flash, and sound when input was pressed.
	if (IsValid(Pawn) && (!Pawn->IsLocallyControlled() || Pawn->HasAuthority()))
	{
		PlayLocalShotFeedback(Definition);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Pawn;
	SpawnParameters.Instigator = Pawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJTSProjectileActor* Tracer = GetWorld()->SpawnActor<AJTSProjectileActor>(
		AJTSProjectileActor::StaticClass(), FTransform::Identity, SpawnParameters))
	{
		Tracer->InitializeTracer(MuzzleStart, TraceEnd, Glow, ShotColor);
	}
	if (bHitSomething)
	{
		if (AJTSProjectileImpactActor* Impact = GetWorld()->SpawnActor<AJTSProjectileImpactActor>(
			AJTSProjectileImpactActor::StaticClass(), FTransform(FRotator::ZeroRotator, TraceEnd + FVector(ImpactNormal) * 0.5f), SpawnParameters))
		{
			Impact->InitializeImpact(ImpactNormal, Glow, ShotColor, !bDamageableHit);
		}
		if (IsValid(Definition))
		{
			if (USoundBase* ImpactSound = Definition->RangedImpactSound.LoadSynchronous())
			{
				UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, TraceEnd, 0.6f,
					FMath::FRandRange(0.94f, 1.06f));
			}
		}
	}
	if (bDebugShotTraces)
	{
		DrawDebugLine(GetWorld(), MuzzleStart, TraceEnd, FColor(80, 210, 255), false, 0.18f, 0, 1.2f);
	}
}

void UJTSRangedWeaponComponent::ClientConfirmRangedHit_Implementation()
{
	if (GetWorld() != nullptr) LastConfirmedHitSeconds = GetWorld()->GetTimeSeconds();
}

float UJTSRangedWeaponComponent::GetReticleKickAlpha() const
{
	return GetWorld() != nullptr
		? FMath::Clamp(1.0f - static_cast<float>((GetWorld()->GetTimeSeconds() - LastLocalShotSeconds) / 0.16), 0.0f, 1.0f)
		: 0.0f;
}

bool UJTSRangedWeaponComponent::HasRecentConfirmedHit() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() - LastConfirmedHitSeconds < 0.16;
}

void UJTSRangedWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSRangedWeaponComponent, bIsAiming);
}
