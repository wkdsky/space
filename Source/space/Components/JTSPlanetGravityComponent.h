// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSPlanetGravityComponent.generated.h"

class AJTSPlanetAnchor;
class AJTSSpaceWorldManager;
class ACharacter;
class UCharacterMovementComponent;

/**
 * Applies UE 5.8 CharacterMovement custom gravity for one real gameplay planet.
 * It deliberately does nothing in Earth and Legacy Fake Moon worlds, where normal World-Z gravity
 * remains authoritative.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSPlanetGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSPlanetGravityComponent();

	/** Binds this character to one explicitly selected real gameplay planet and wakes gravity updates. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Gravity")
	void SetPlanetAnchor(AJTSPlanetAnchor* InPlanetAnchor);

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	AJTSPlanetAnchor* GetPlanetAnchor() const;

	UFUNCTION(BlueprintPure, Category = "Planet|Gravity")
	bool IsUsingPlanetGravity() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	AJTSSpaceWorldManager* ResolveSpaceWorldManager();
	bool CanUseSurfacePlanetGravity(const AJTSSpaceWorldManager* Manager, const AJTSPlanetAnchor* Planet) const;
	void UpdatePlanetGravity();
	void RestoreWorldGravity();
	void LogGravityDebug(
		const ACharacter* Character,
		const UCharacterMovementComponent* MovementComponent,
		const AJTSSpaceWorldManager* Manager,
		const AJTSPlanetAnchor* ComputedPlanet,
		const FVector& ComputedGravityDirection,
		bool bSurfaceGravityActive,
		const TCHAR* Reason);

	/** Set by the owning Character/GameMode; no world-wide planet selection is performed here. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Planet|Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AJTSPlanetAnchor> PlanetAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugPlanetGravity = false;

	TWeakObjectPtr<AJTSSpaceWorldManager> CachedSpaceWorldManager;
	bool bCapturedDefaultGravityScale = false;
	float DefaultGravityScale = 1.0f;
	bool bUsingPlanetGravity = false;
	double LastDebugLogTime = -1.0;
};
