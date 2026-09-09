// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlanetGravityComponent.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Planets/JTSMoonPlanetActor.h"
#include "space/World/JTSMoonSurfaceController.h"

UJTSPlanetGravityComponent::UJTSPlanetGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UJTSPlanetGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController) && SurfaceController->OwnsSurfaceActor(GetOwner()))
	{
		if (ACharacter* const Character = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* const MovementComponent = Character->GetCharacterMovement())
			{
				MovementComponent->SetGravityDirection(FVector::DownVector);
			}
		}
		SetComponentTickEnabled(false);
		UE_LOG(LogTemp, Warning, TEXT("Legacy UJTSPlanetGravityComponent is bypassed on the active Moon surface."));
		return;
	}

	FindMoonPlanet();
	ApplyPlanetGravity();
}

void UJTSPlanetGravityComponent::TickComponent(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (const AJTSMoonSurfaceController* const SurfaceController = AJTSMoonSurfaceController::FindMoonSurfaceController(this);
		IsValid(SurfaceController) && SurfaceController->OwnsSurfaceActor(GetOwner()))
	{
		if (ACharacter* const Character = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* const MovementComponent = Character->GetCharacterMovement())
			{
				MovementComponent->SetGravityDirection(FVector::DownVector);
			}
		}
		SetComponentTickEnabled(false);
		return;
	}

	if (!MoonPlanet.IsValid())
	{
		FindMoonPlanet();
	}

	ApplyPlanetGravity();
}

void UJTSPlanetGravityComponent::FindMoonPlanet()
{
	MoonPlanet.Reset();

	const AActor* const Owner = GetOwner();
	ULevel* const OwnerLevel = IsValid(Owner) ? Owner->GetLevel() : nullptr;
	if (!IsValid(OwnerLevel))
	{
		return;
	}

	for (AActor* const Actor : OwnerLevel->Actors)
	{
		if (AJTSMoonPlanetActor* const Planet = Cast<AJTSMoonPlanetActor>(Actor); IsValid(Planet))
		{
			MoonPlanet = Planet;
			return;
		}
	}
}

void UJTSPlanetGravityComponent::ApplyPlanetGravity()
{
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const MovementComponent = Character != nullptr
		? Character->GetCharacterMovement()
		: nullptr;
	AJTSMoonPlanetActor* const Planet = MoonPlanet.Get();
	if (!IsValid(Character) || !IsValid(MovementComponent) || !IsValid(Planet))
	{
		if (bAppliedCustomGravity && IsValid(MovementComponent))
		{
			MovementComponent->SetGravityDirection(FVector::DownVector);
		}
		bAppliedCustomGravity = false;
		return;
	}

	const FVector GravityDirection = Planet->GetGravityDirection(Character->GetActorLocation());
	if (!GravityDirection.IsNearlyZero())
	{
		MovementComponent->SetGravityDirection(GravityDirection);
		bAppliedCustomGravity = true;
	}
}
