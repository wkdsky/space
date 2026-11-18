#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSItemTypes.h"
#include "JTSSpaceDebugTravelManager.generated.h"

class AJTSPlayerController;

USTRUCT(BlueprintType)
struct FJTSDebugTravelDestination
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlanetId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
};

USTRUCT(BlueprintType)
struct FJTSDebugAccessPermit
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EJTSItemId DeviceItemId=EJTSItemId::None;
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
	const TArray<FJTSDebugAccessPermit>& GetAccessPermits() const { return AccessPermits; }
	const FJTSDebugAccessPermit* FindAccessPermit(FName ItemId) const;
	void GrantAccessPermit(AJTSPlayerController* Controller, FName ItemId);
	const FText& GetStatus() const { return Status; }
private:
	UPROPERTY(EditAnywhere, Replicated, Category = "Debug Travel") bool bEnabled = true;
	UPROPERTY(EditAnywhere, Replicated, Category = "Debug Travel") TArray<FJTSDebugTravelDestination> Destinations;
	/** Important item granted by the debug panel; selected by the level, never by client input. */
	UPROPERTY(EditAnywhere, Replicated, Category = "Debug Access") TArray<FJTSDebugAccessPermit> AccessPermits;
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Debug Travel") FName DefaultPlanetId;
	UPROPERTY(VisibleInstanceOnly, Replicated, Category = "Debug Travel") FText Status;
	double LastTravelTime = -10.0;
};
