#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSBossDeathPresentationComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Cosmetic corpse hold/fade, timed from the replicated combat action on every peer. */
UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSBossDeathPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSBossDeathPresentationComponent();
	void BeginDeath();
	UPROPERTY(EditDefaultsOnly, Category="Death", meta=(ClampMin="0")) float HoldDuration = 1.4f;
	UPROPERTY(EditDefaultsOnly, Category="Death", meta=(ClampMin="0.1")) float FadeDuration = 4.5f;
	/** Translucent equivalents, in the skeletal mesh's material-slot order. Selected in Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category="Death") TArray<TObjectPtr<UMaterialInterface>> FadeMaterials;
	UPROPERTY(EditDefaultsOnly, Category="Death") FName OpacityParameter = TEXT("CorpseOpacity");
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void UpdatePresentation();
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeInstances;
	FTimerHandle PresentationTimer;
	bool bStarted = false;
};
