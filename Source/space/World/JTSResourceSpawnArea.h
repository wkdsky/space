// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Modes/JTSEarthGameMode.h"

#include "JTSResourceSpawnArea.generated.h"

class AJTSResourcePickupActor;
class UBoxComponent;

/**
 * Generates the small, repeatable Earth-stage resource layout inside an editor-visible box.
 */
UCLASS()
class SPACE_API AJTSResourceSpawnArea : public AActor
{
	GENERATED_BODY()

public:
	AJTSResourceSpawnArea();

	/** Generates this round's resource pickups and returns the number actually placed. */
	UFUNCTION(BlueprintCallable, Category = "Resources|Spawning")
	int32 GenerateResources();

	/** Applies Earth chapter balance values before this area generates its pickups. */
	UFUNCTION(BlueprintCallable, Category = "Resources|Spawning")
	void ApplyEarthSpawnSettings(const FJTSEarthResourceSpawnSettings& Settings);

	/** Pickup class used for every generated resource. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning")
	TSubclassOf<AJTSResourcePickupActor> ResourcePickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0", UIMin = "0"))
	int32 FuelPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0", UIMin = "0"))
	int32 WaterPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0", UIMin = "0"))
	int32 FoodPickupCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MinimumPickupSpacing = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PlayerExclusionRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftExclusionRadius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EdgePadding = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (EditCondition = "!bUseRandomSeed"))
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Spawning")
	bool bClearExistingResourcePickups = true;

private:
	void ClearGeneratedPickups();

	/** Editor volume used to define the square XY generation region. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources|Spawning", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> SpawnBox;

	/** References to pickups created by this area, used for safe subsequent cleanup. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AJTSResourcePickupActor>> GeneratedPickups;
};
