// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSExperienceRewardComponent.generated.h"

class AController;
class AActor;
class UJTSHealthComponent;

/**
 * Server-only enemy reward policy.  Attach it to any actor with UJTSHealthComponent: ordinary
 * enemies award their killer, while a boss awards its killer twice the configured value and every
 * other active expedition teammate the configured value.
 */
UCLASS(ClassGroup = (Progression), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSExperienceRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSExperienceRewardComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerDeath(AController* InstigatorController, AActor* DamageCauser);

	static bool IsEligibleKiller(const class AJTSPlayerState* PlayerState);
	static bool IsEligibleBossTeammate(const class AJTSPlayerState* PlayerState);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 ExperienceReward = 20;

	/** Bosses use the shared expedition reward rule; future boss Blueprints only need to enable this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience", meta = (AllowPrivateAccess = "true"))
	bool bBossReward = false;

	TWeakObjectPtr<UJTSHealthComponent> BoundHealthComponent;
	bool bRewardGranted = false;
};
