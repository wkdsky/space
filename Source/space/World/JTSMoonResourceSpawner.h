#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSResourceType.h"

#include "JTSMoonResourceSpawner.generated.h"

class AJTSMoonResourceActor;

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
	AJTSMoonResourceActor* SpawnResource(
		EJTSResourceType ResourceType,
		int32 ResourceAmount,
		bool bCanPickup,
		const FVector& ResourceScale,
		const FRotator& ResourceRotation,
		const FText& ResourcePickupText,
		const FVector& GroundLocation);
	bool ResolveGroundLocation(const FVector& CandidateXY, FVector& OutGroundLocation) const;

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
};
