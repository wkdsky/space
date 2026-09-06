#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSResourceType.h"

#include "JTSMoonResourceSpawner.generated.h"

class AJTSMoonResourceActor;

UCLASS()
class SPACE_API AJTSMoonResourceSpawner : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonResourceSpawner();

	UFUNCTION(BlueprintCallable, Category = "Moon|Resources")
	int32 GenerateResources();

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
