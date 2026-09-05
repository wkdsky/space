// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSMoonPlanetActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Legacy spherical Moon actor retained only so existing Blueprint references remain loadable. */
UCLASS(meta = (DeprecatedNode, DeprecationMessage = "Legacy spherical Moon gameplay is retired; use AJTSMoonWorldActor."))
class SPACE_API AJTSMoonPlanetActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonPlanetActor();

	UFUNCTION(BlueprintPure, Category = "Moon Planet")
	float GetPlanetRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon Planet")
	FVector GetPlanetCenter() const;

	UFUNCTION(BlueprintPure, Category = "Moon Planet")
	FVector GetGravityDirection(const FVector& WorldLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void UpdateVisualScale();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlanetMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Moon Planet", meta = (AllowPrivateAccess = "true", DeprecatedProperty, DeprecationMessage = "PlanetRadius is retained for legacy assets and is not a Fake Moon gameplay parameter.", ClampMin = "1.0", UIMin = "1.0"))
	float PlanetRadius = 3800.0f;
};
