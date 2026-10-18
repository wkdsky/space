#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSSpaceDebugTravelManager.generated.h"

class AJTSPlayerController;

USTRUCT(BlueprintType)
struct FJTSDebugTravelDestination
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlanetId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
};

/** Optional level configuration for developer travel. All mutations go through the owning controller. */
UCLASS()
class SPACE_API AJTSSpaceDebugTravelManager : public AActor
{
	GENERATED_BODY()
public:
	AJTSSpaceDebugTravelManager();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AJTSSpaceDebugTravelManager* Find(const UObject* Context);
	bool IsAvailable() const;
	bool ContainsDestination(FName PlanetId) const;
	bool CanUse(const AJTSPlayerController* Controller) const;
	FName GetPreferredPlanetId() const;
	void Request(AJTSPlayerController* Controller, FName PlanetId, bool bSetDefault);
	const TArray<FJTSDebugTravelDestination>& GetDestinations() const { return Destinations; }
	FName GetDefaultPlanetId() const { return DefaultPlanetId; }
	const FText& GetStatus() const { return Status; }
private:
	UPROPERTY(EditAnywhere, Replicated, Category = "Debug Travel") bool bEnabled = true;
	UPROPERTY(EditAnywhere, Replicated, Category = "Debug Travel") TArray<FJTSDebugTravelDestination> Destinations;
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Debug Travel") FName DefaultPlanetId;
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Debug Travel") FText Status;
	double LastTravelTime = -10.0;
};
