// Copyright Epic Games, Inc. All Rights Reserved.

#include "JTSPlanetGravityComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Planets/JTSMoonPlanetActor.h"

UJTSPlanetGravityComponent::UJTSPlanetGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UJTSPlanetGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() != nullptr && GetWorld()->GetAuthGameMode<AJTSMoonGameMode>() != nullptr)
	{
		if (ACharacter* const Character = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* const MovementComponent = Character->GetCharacterMovement())
			{
				MovementComponent->SetGravityDirection(FVector::DownVector);
			}
		}
		SetComponentTickEnabled(false);
		UE_LOG(LogTemp, Warning, TEXT("Legacy UJTSPlanetGravityComponent is bypassed in AJTSMoonGameMode."));
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

	if (!MoonPlanet.IsValid())
	{
		FindMoonPlanet();
	}

	ApplyPlanetGravity();
}

void UJTSPlanetGravityComponent::FindMoonPlanet()
{
	MoonPlanet.Reset();

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AJTSMoonPlanetActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			MoonPlanet = *It;
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
