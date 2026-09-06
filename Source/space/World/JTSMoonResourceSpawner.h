#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSResourceType.h"

#include "JTSMoonResourceSpawner.generated.h"

class AJTSMoonResourceActor;
class AJTSWorldPickupActor;

/** Moon resource distribution values owned by the Moon chapter ruleset. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSMoonResourceSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 TotalResourceCount = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnRadius = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 SmallRockWeight = 55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 MediumRockWeight = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 LargeRockWeight = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0", UIMin = "0"))
	int32 OreWeight = 15;

	/** Extra XY clearance added around the spacecraft's physical mesh bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpacecraftExclusionPadding = 500.0f;
};

UCLASS()
class SPACE_API AJTSMoonResourceSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonResourceSpawner();

	UFUNCTION(BlueprintCallable, Category = "Moon|Resources")
	int32 GenerateResources();

	/** Applies Moon GameMode balance values before this spawner generates its resources. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Resources")
	void ApplyMoonSpawnSettings(const FJTSMoonResourceSpawnSettings& Settings);

protected:
	virtual void BeginPlay() override;

private:
	void ClearGeneratedResources();
	AJTSMoonResourceActor* SpawnMiningNode(
		EJTSResourceType ResourceType,
		int32 TotalYieldUnits,
		const FVector& ResourceScale,
		const FRotator& ResourceRotation,
		const FVector& GroundLocation);
	AJTSWorldPickupActor* SpawnInitialPickup(const FVector& GroundLocation, const FVector& PreferredDirection = FVector::ZeroVector);
	bool ResolveGroundLocation(const FVector& CandidateXY, FVector& OutGroundLocation) const;
	bool IsCandidateExcludedBySpacecraft(const FVector& CandidateXY) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AJTSMoonResourceActor> ResourceActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 ResourceCount = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float Radius = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 SmallRockWeight = 55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 MediumRockWeight = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 LargeRockWeight = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
	int32 OreWeight = 15;

	/** Runtime copy of the Moon GameMode's sole spacecraft exclusion setting. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Moon|Resources", meta = (AllowPrivateAccess = "true"))
	float SpacecraftExclusionPadding = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceStartHeight = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Ground", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float GroundTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Random", meta = (AllowPrivateAccess = "true"))
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Resources|Random", meta = (AllowPrivateAccess = "true", EditCondition = "!bUseRandomSeed"))
	int32 RandomSeed = 1337;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AJTSMoonResourceActor>> GeneratedResources;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AJTSWorldPickupActor>> GeneratedPickups;
};
