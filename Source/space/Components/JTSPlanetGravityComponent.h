// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSPlanetGravityComponent.generated.h"

class AJTSMoonPlanetActor;

/** Legacy radial-gravity component retained for asset compatibility; Fake Moon gameplay never uses it. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent, DeprecatedNode, DeprecationMessage = "Radial Moon gravity is retired; standard World-Z gravity is used."))
class SPACE_API UJTSPlanetGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSPlanetGravityComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void FindMoonPlanet();
	void ApplyPlanetGravity();

	TWeakObjectPtr<AJTSMoonPlanetActor> MoonPlanet;
	bool bAppliedCustomGravity = false;
};
