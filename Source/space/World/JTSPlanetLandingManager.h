// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/World/JTSPlanetLandingTypes.h"

#include "JTSPlanetLandingManager.generated.h"

class AJTSCharacter;
class AJTSPlanetAnchor;
class AJTSPlanetLandingSite;
class AJTSSpacecraftActor;
class APlayerController;
class UObject;

/**
 * Runtime coordinator for first arrival, legal landing-site queries, and landed-spacecraft respawn
 * resolution. It stores a dynamic site registry rather than putting level configuration on a planet.
 */
UCLASS(BlueprintType)
class SPACE_API AJTSPlanetLandingManager : public AActor
{
	GENERATED_BODY()

public:
	AJTSPlanetLandingManager();

	static AJTSPlanetLandingManager* FindPlanetLandingManager(const UObject* WorldContextObject);

	/** Dynamic sites call this on BeginPlay; existing persistent sites are discovered on manager startup. */
	bool RegisterLandingSite(AJTSPlanetLandingSite* LandingSite);
	void UnregisterLandingSite(AJTSPlanetLandingSite* LandingSite);

	/** Returns every currently enabled or disabled registered site belonging to Planet. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing")
	TArray<AJTSPlanetLandingSite*> GetLandingSitesForPlanet(AJTSPlanetAnchor* Planet) const;

	/** The union-area query: true when Location belongs to any enabled site for Planet. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	bool IsLocationInsideLandingArea(AJTSPlanetAnchor* Planet, const FVector& Location) const;

	/** Returns the enabled site with the closest legal-area point, or nullptr when this planet has none. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	AJTSPlanetLandingSite* GetNearestLandingSite(AJTSPlanetAnchor* Planet, const FVector& Location) const;

	/** Distance from Location to the closest enabled legal landing area. Returns -1 when no area exists. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	float GetLandingDistance(AJTSPlanetAnchor* Planet, const FVector& Location) const;

	/** Non-mutating landing-rule query intended for flight HUDs and future Blueprint UI. */
	UFUNCTION(BlueprintPure, Category = "Planet|Landing")
	bool IsLandingAvailable(AJTSSpacecraftActor* Spacecraft) const;

	/** Draws every enabled landing-area volume belonging to Planet through its owning LandingSite. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Landing|Debug")
	void DrawDebugLandingAreas(AJTSPlanetAnchor* Planet, float Duration = 5.0f) const;

	/** Starts a site-independent first arrival. Spawn transforms come from an ArrivalAnchor or safe generic fallback. */
	UFUNCTION(BlueprintCallable, Category = "Planet|Arrival")
	bool StartLandingSequence(APlayerController* PlayerController, AJTSPlanetAnchor* Planet);

	/** Entry point used by AJTSSpacecraftActor::RequestLanding. */
	bool RequestLanding(AJTSSpacecraftActor* Spacecraft, FJTSPlanetLandingValidationResult& OutResult);

	/** Resolves a respawn transform using the union of nearby legal site areas and documented fallbacks. */
	bool FindPlayerRespawnTransform(
		const AJTSSpacecraftActor* Spacecraft,
		FJTSPlayerRespawnTransformResult& OutResult) const;

	/** Respawns a controller only when its associated spacecraft is legally Landed. */
	bool RespawnPlayerAtLandedSpacecraft(APlayerController* PlayerController);

	void RegisterPlayerSpacecraft(APlayerController* PlayerController, AJTSSpacecraftActor* Spacecraft);
	AJTSSpacecraftActor* GetPlayerSpacecraft(const APlayerController* PlayerController) const;

	/** Allows the current SpaceWorld GameMode Blueprint spacecraft class to migrate without hard-coded assets. */
	void SetDefaultSpacecraftClass(TSubclassOf<AJTSSpacecraftActor> InSpacecraftClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void DiscoverPersistentLandingSites();
	AJTSPlanetAnchor* ResolvePlanetForSpacecraft(AJTSSpacecraftActor* Spacecraft) const;
	bool ValidateLandingSite(
		AJTSSpacecraftActor* Spacecraft,
		AJTSPlanetAnchor* Planet,
		AJTSPlanetLandingSite* LandingSite,
		const struct FJTSSpacecraftGroundInfo& GroundInfo,
		FJTSPlanetLandingValidationResult& OutResult) const;
	bool QueryLandingAvailability(
		AJTSSpacecraftActor* Spacecraft,
		FJTSPlanetLandingValidationResult& OutResult,
		AJTSPlanetAnchor*& OutPlanet) const;
	bool BuildRespawnTransformAtAreaPoint(
		const AJTSSpacecraftActor* Spacecraft,
		AJTSPlanetAnchor* Planet,
		const FVector& AreaPoint,
		FTransform& OutTransform,
		bool bRequireClearance) const;
	bool IsRespawnTransformClear(const AJTSSpacecraftActor* Spacecraft, const FTransform& Transform) const;
	bool SpawnAndConfigureCharacter(APlayerController* PlayerController, AJTSPlanetAnchor* Planet, const FTransform& SpawnTransform, AJTSCharacter*& OutCharacter) const;
	AJTSSpacecraftActor* FindOrSpawnArrivalSpacecraft(
		AJTSPlanetAnchor* Planet,
		const FTransform& SpawnTransform,
		const FVector& PreferredSurfaceLocation);
	bool ParkArrivalSpacecraftOnSurface(
		AJTSSpacecraftActor* Spacecraft,
		AJTSPlanetAnchor* Planet,
		const FVector& PreferredSurfaceLocation) const;
	/**
	 * First-arrival safety net for an authored map whose PlanetAnchor surface reference is stale.
	 * It follows the selected planet's radial gravity direction and uses only the physical ground
	 * directly below the arrival location; it never selects a level actor by name or asset.
	 */
	bool ParkArrivalSpacecraftOnPhysicalGround(
		AJTSSpacecraftActor* Spacecraft,
		AJTSPlanetAnchor* Planet,
		const FVector& QueryLocation) const;
	bool ResolveArrivalTransforms(
		APlayerController* PlayerController,
		AJTSPlanetAnchor* Planet,
		FTransform& OutPlayerTransform,
		FTransform& OutSpacecraftTransform) const;
	void LogGroundDetectionResult(AJTSPlanetAnchor* Planet, const FVector& Location, const TCHAR* Context) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSSpacecraftActor> DefaultSpacecraftClass;

	/** Used only when no ArrivalAnchor is authored: local X/Y follow the player's surface frame and Z is radial up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true"))
	FVector GenericArrivalSpacecraftOffset = FVector(900.0f, 0.0f, 450.0f);

	/** Desired descent time used by the spacecraft to derive a capped automatic landing speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Landing", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float LandingAssistDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 RespawnRandomAttempts = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Respawn", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float RespawnSurfaceProbeDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float InitialGroundProbeDistance = 8000.0f;

	/** Generic air-space search used only when an independent arrival craft initially overlaps collision. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float ArrivalCollisionSearchStep = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Arrival", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 ArrivalCollisionSearchAttempts = 8;

	TSet<TWeakObjectPtr<AJTSPlanetLandingSite>> RegisteredLandingSites;
	TMap<TWeakObjectPtr<AJTSPlanetAnchor>, TWeakObjectPtr<AJTSSpacecraftActor>> ArrivalSpacecraftByPlanet;
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AJTSSpacecraftActor>> PlayerSpacecraft;
};
