#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Ships/JTSStellarExchangeTypes.h"
#include "space/Items/JTSShipLockerTypes.h"
#include "JTSStellarExchangeComponent.generated.h"

class AJTSShipDockActor;
class AJTSPlayerState;

USTRUCT()
struct FJTSExchangeRollPose
{
	GENERATED_BODY()
	UPROPERTY() bool bRolling = false;
	UPROPERTY() double StartTime = 0;
	UPROPERTY() int32 Seed = 0;
};

/** One shared machine, economy and result stream for the entire expedition. */
UCLASS(ClassGroup=(Ship), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSStellarExchangeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSStellarExchangeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	EJTSStellarRollResult TryPull(APawn* Pawn);
	bool TryClaim(APawn* Pawn, FGuid Token);
	FJTSStellarExchangeSnapshot CaptureSnapshot() const;
	void RestoreSnapshot(const FJTSStellarExchangeSnapshot& Saved);
	const FJTSStellarExchangeSnapshot& GetState() const { return State; }
	UFUNCTION(BlueprintPure) bool IsRolling() const { return Roll.bRolling; }
	UFUNCTION(BlueprintPure) float GetRollElapsed() const;
	UFUNCTION(BlueprintPure) int32 GetReelIndex(int32 Column, int32 Count) const;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="60")) int32 Capacity = 12; // Serialized compatibility only; there is no public inventory.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) float RollDuration = 3.2f;
private:
	void CompleteRoll();
	double ServerTime() const;
	void Persist();
	UPROPERTY(Replicated) FJTSStellarExchangeSnapshot State;
	UPROPERTY(Replicated) FJTSExchangeRollPose Roll;
	FName PendingResult;
	FGuid PendingToken;
	FString PendingRecipientId;
	void DeliverPendingPrizes();
	FTimerHandle DeliveryTimer;
	double NextPullTime = 0;
	FTimerHandle RevealTimer;
};
