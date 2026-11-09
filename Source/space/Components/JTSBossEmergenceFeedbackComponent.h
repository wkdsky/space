#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "Components/ActorComponent.h"
#include "JTSBossEmergenceFeedbackComponent.generated.h"
class APlayerCameraManager;

/** Small, rapid camera-local tremor. Uses Engine's shake blending and cancellation. */
UCLASS()
class SPACE_API UJTSGroundTremorPattern : public UCameraShakePattern
{
	GENERATED_BODY()
private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& Info) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result) override;
	virtual bool IsFinishedImpl() const override { return !State.IsPlaying(); }
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override { State.Stop(Params.bImmediately); }
	FCameraShakeState State;
};
UCLASS()
class SPACE_API UJTSGroundTremorShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UJTSGroundTremorShake(const FObjectInitializer& Initializer);
};

/** Local feedback only; the replicated combat action supplies the authoritative emergence clock. */
UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSBossEmergenceFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSBossEmergenceFeedbackComponent();
	/** Called again after initialization/replication; BeginPlay may precede the emergence clock. */
	void RefreshAction();
	UPROPERTY(EditDefaultsOnly, Category="Emergence") float Radius = 5000;
	UPROPERTY(EditDefaultsOnly, Category="Emergence") float MaximumHeight = 500;
	UPROPERTY(EditDefaultsOnly, Category="Emergence") TSubclassOf<UCameraShakeBase> ShakeClass;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void UpdateFeedback();
	void StopFeedback();
	TMap<TWeakObjectPtr<APlayerCameraManager>, TWeakObjectPtr<UCameraShakeBase>> Shakes;
	FTimerHandle FeedbackTimer;
};
