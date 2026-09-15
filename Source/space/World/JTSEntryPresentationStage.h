// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSEntryPresentationStage.generated.h"

class UCameraComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Non-gameplay launch-window tableau for L_Entry.
 *
 * The front end owns no expedition actors, resources, timers, or player pawn. This actor is only
 * a reusable visual composition point: Blueprint may replace meshes/materials without changing
 * menu, save, or session behavior.
 */
UCLASS()
class SPACE_API AJTSEntryPresentationStage : public AActor
{
	GENERATED_BODY()

public:
	AJTSEntryPresentationStage();

	UCameraComponent* GetPresentationCamera() const { return PresentationCamera; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> LaunchDeck;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> Planet;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> OrbitBand;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> SpacecraftHull;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> SpacecraftNose;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UStaticMeshComponent> Beacon;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UPointLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UPointLightComponent> AccentLight;
	UPROPERTY(VisibleAnywhere, Category = "Entry Presentation") TObjectPtr<UCameraComponent> PresentationCamera;
};
