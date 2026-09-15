// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework\Actor.h"

#include "JTSPreLaunchLobbyStage.generated.h"

class UCameraComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Lightweight, map-independent staging presentation for L_PreLaunchLobby.
 * It creates only hangar dressing, a ship silhouette, fixed avatar pads, and a camera; no gameplay actors.
 */
UCLASS()
class SPACE_API AJTSPreLaunchLobbyStage : public AActor
{
	GENERATED_BODY()

public:
	AJTSPreLaunchLobbyStage();

	UFUNCTION(BlueprintPure, Category = "Pre-Launch Lobby")
	FVector GetPresentationPosition(int32 SlotIndex) const;

	UCameraComponent* GetLobbyCamera() const { return LobbyCamera; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UStaticMeshComponent> HangarFloor;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UStaticMeshComponent> BackWall;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UStaticMeshComponent> ShipSilhouette;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TArray<TObjectPtr<UStaticMeshComponent>> PresentationPads;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UCameraComponent> LobbyCamera;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Pre-Launch Lobby")
	TObjectPtr<UPointLightComponent> AccentLight;

	UPROPERTY(EditDefaultsOnly, Category = "Pre-Launch Lobby")
	TArray<FVector> PresentationOffsets;
};
