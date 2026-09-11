// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlanetGravityComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

UJTSPlanetGravityComponent::UJTSPlanetGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UJTSPlanetGravityComponent::SetPlanetAnchor(AJTSPlanetAnchor* InPlanetAnchor)
{
	PlanetAnchor = InPlanetAnchor;
	// A repeated bind is intentional: it must also recover a component that was disabled by
	// an older startup order before the GameMode finished selecting the current planet.
	SetComponentTickEnabled(true);
	UpdatePlanetGravity();
}

AJTSPlanetAnchor* UJTSPlanetGravityComponent::GetPlanetAnchor() const
{
	return PlanetAnchor.Get();
}

bool UJTSPlanetGravityComponent::IsUsingPlanetGravity() const
{
	return bUsingPlanetGravity;
}

void UJTSPlanetGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* const Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* const MovementComponent = Character->GetCharacterMovement())
		{
			DefaultGravityScale = MovementComponent->GravityScale;
			bCapturedDefaultGravityScale = true;
		}
	}

	SetComponentTickEnabled(true);
	UpdatePlanetGravity();
}

void UJTSPlanetGravityComponent::TickComponent(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePlanetGravity();
}

AJTSSpaceWorldManager* UJTSPlanetGravityComponent::ResolveSpaceWorldManager()
{
	if (AJTSSpaceWorldManager* const Manager = CachedSpaceWorldManager.Get(); IsValid(Manager))
	{
		return Manager;
	}

	AJTSSpaceWorldManager* const Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	CachedSpaceWorldManager = Manager;
	return Manager;
}

bool UJTSPlanetGravityComponent::CanUseSurfacePlanetGravity(
	const AJTSSpaceWorldManager* Manager,
	const AJTSPlanetAnchor* Planet) const
{
	// Gravity belongs to the explicit character-to-planet binding. Arrival/streaming state and a
	// successful ground trace must never decide whether a spawned character can fall naturally.
	static_cast<void>(Manager);
	return IsValid(Planet) && Planet->IsGravityEnabled();
}

void UJTSPlanetGravityComponent::UpdatePlanetGravity()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const MovementComponent = Character != nullptr
		? Character->GetCharacterMovement()
		: nullptr;
	if (!IsValid(Character) || !IsValid(MovementComponent))
	{
		return;
	}

	AJTSSpaceWorldManager* const Manager = ResolveSpaceWorldManager();
	AJTSPlanetAnchor* const Planet = PlanetAnchor.Get();
	if (!CanUseSurfacePlanetGravity(Manager, Planet))
	{
		RestoreWorldGravity();
		const TCHAR* const Reason = !IsValid(Planet)
			? TEXT("NoBoundPlanet")
			: TEXT("PlanetGravityDisabled");
		LogGravityDebug(Character, MovementComponent, Manager, Planet, FVector::ZeroVector, false, Reason);
		return;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector GravityDirection = (Planet->GetPlanetCenter() - CharacterLocation).GetSafeNormal();
	if (GravityDirection.IsNearlyZero())
	{
		// At the exact centre no radial direction exists. Keep the most recent movement gravity
		// instead of incorrectly falling back to World-Z while the real surface state is active.
		LogGravityDebug(Character, MovementComponent, Manager, Planet, GravityDirection, true, TEXT("AtPlanetCenter"));
		return;
	}

	if (!bCapturedDefaultGravityScale)
	{
		DefaultGravityScale = MovementComponent->GravityScale;
		bCapturedDefaultGravityScale = true;
	}

	if (const UWorld* const World = GetWorld(); World != nullptr)
	{
		const float WorldGravityMagnitude = FMath::Abs(World->GetGravityZ());
		if (WorldGravityMagnitude > KINDA_SMALL_NUMBER)
		{
			MovementComponent->GravityScale = Planet->GetGravityStrength() / WorldGravityMagnitude;
		}
	}

	MovementComponent->SetGravityDirection(GravityDirection);
	if (!bUsingPlanetGravity)
	{
		UE_LOG(LogTemp, Log, TEXT("Planet gravity enabled for %s on %s."), *GetOwner()->GetName(), *Planet->GetPlanetId().ToString());
	}
	bUsingPlanetGravity = true;

	if (bDebugPlanetGravity)
	{
		DrawDebugLine(GetWorld(), CharacterLocation, Planet->GetPlanetCenter(), FColor::Cyan, false, -1.0f, 0, 1.5f);
		DrawDebugDirectionalArrow(GetWorld(), CharacterLocation, CharacterLocation + GravityDirection * 250.0f, 35.0f, FColor::Magenta, false, -1.0f, 0, 2.0f);
	}

	LogGravityDebug(Character, MovementComponent, Manager, Planet, GravityDirection, true, TEXT("SurfaceRadial"));
}

void UJTSPlanetGravityComponent::RestoreWorldGravity()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const MovementComponent = Character != nullptr
		? Character->GetCharacterMovement()
		: nullptr;
	if (!IsValid(MovementComponent))
	{
		return;
	}

	if (!bUsingPlanetGravity)
	{
		return;
	}

	MovementComponent->SetGravityDirection(FVector::DownVector);
	if (bCapturedDefaultGravityScale)
	{
		MovementComponent->GravityScale = DefaultGravityScale;
	}

	if (bUsingPlanetGravity)
	{
		UE_LOG(LogTemp, Log, TEXT("Planet gravity disabled for %s; restored World-Z gravity."), *GetOwner()->GetName());
	}
	bUsingPlanetGravity = false;
}

void UJTSPlanetGravityComponent::LogGravityDebug(
	const ACharacter* Character,
	const UCharacterMovementComponent* MovementComponent,
	const AJTSSpaceWorldManager* Manager,
	const AJTSPlanetAnchor* ComputedPlanet,
	const FVector& ComputedGravityDirection,
	bool bSurfaceGravityActive,
	const TCHAR* Reason)
{
	if (!bDebugPlanetGravity || !IsValid(Character) || !IsValid(MovementComponent))
	{
		return;
	}

	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (LastDebugLogTime >= 0.0 && CurrentTime - LastDebugLogTime < 1.0)
	{
		return;
	}
	LastDebugLogTime = CurrentTime;

	const AJTSPlanetAnchor* const CurrentPlanet = IsValid(Manager) ? Manager->GetCurrentPlanet() : nullptr;
	const AJTSPlanetAnchor* const DebugPlanet = IsValid(ComputedPlanet) ? ComputedPlanet : CurrentPlanet;
	const FVector PlanetCenter = IsValid(DebugPlanet) ? DebugPlanet->GetPlanetCenter() : FVector::ZeroVector;
	const FVector CharacterLocation = Character->GetActorLocation();
	const float DistanceToCenter = IsValid(DebugPlanet)
		? FVector::Distance(CharacterLocation, PlanetCenter)
		: 0.0f;
	const float ApproximateRadius = IsValid(DebugPlanet) ? DebugPlanet->GetApproximateRadius() : 0.0f;
	const float GravityInfluenceRange = IsValid(DebugPlanet) ? DebugPlanet->GetGravityInfluenceRange() : 0.0f;
	const float SpaceExitRange = IsValid(DebugPlanet) ? DebugPlanet->GetSpaceExitRange() : 0.0f;
	const FVector MovementGravityDirection = MovementComponent->GetGravityDirection();
	const bool bDirectionsMatch = bSurfaceGravityActive
		&& !ComputedGravityDirection.IsNearlyZero()
		&& ComputedGravityDirection.Equals(MovementGravityDirection.GetSafeNormal(), KINDA_SMALL_NUMBER);
	const UEnum* const TravelStateEnum = StaticEnum<EJTSSpaceTravelState>();
	const FString TravelStateName = IsValid(Manager) && TravelStateEnum != nullptr
		? TravelStateEnum->GetNameStringByValue(static_cast<int64>(Manager->GetCurrentTravelState()))
		: TEXT("None");
	const FString CurrentPlanetName = IsValid(CurrentPlanet) ? CurrentPlanet->GetName() : TEXT("None");
	const FString PlanetAnchorName = IsValid(PlanetAnchor) ? PlanetAnchor->GetName() : TEXT("None");

	if (bSurfaceGravityActive && !bDirectionsMatch)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlanetGravity: Character=%s State=%s CurrentPlanet=%s PlanetAnchor=%s Center=%s CharacterLocation=%s DistanceToCenter=%.1f ApproximateRadius=%.1f ComputedGravityDirection=%s MovementGravityDirection=%s Match=0 Tick=%d GravityInfluenceRange=%.1f SpaceExitRange=%.1f Reason=%s"),
			*Character->GetName(), *TravelStateName, *CurrentPlanetName, *PlanetAnchorName,
			*PlanetCenter.ToString(), *CharacterLocation.ToString(), DistanceToCenter, ApproximateRadius,
			*ComputedGravityDirection.ToString(), *MovementGravityDirection.ToString(),
			IsComponentTickEnabled() ? 1 : 0, GravityInfluenceRange, SpaceExitRange, Reason);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PlanetGravity: Character=%s State=%s CurrentPlanet=%s PlanetAnchor=%s Center=%s CharacterLocation=%s DistanceToCenter=%.1f ApproximateRadius=%.1f ComputedGravityDirection=%s MovementGravityDirection=%s Match=%d Tick=%d GravityInfluenceRange=%.1f SpaceExitRange=%.1f Reason=%s"),
		*Character->GetName(), *TravelStateName, *CurrentPlanetName, *PlanetAnchorName,
		*PlanetCenter.ToString(), *CharacterLocation.ToString(), DistanceToCenter, ApproximateRadius,
		*ComputedGravityDirection.ToString(), *MovementGravityDirection.ToString(),
		bDirectionsMatch ? 1 : 0, IsComponentTickEnabled() ? 1 : 0,
		GravityInfluenceRange, SpaceExitRange, Reason);
}
