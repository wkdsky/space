#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSCombatFacingComponent.generated.h"

/** One facing policy for free movement, committed melee swings and camera-directed fire. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSCombatFacingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSCombatFacingComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	bool UsesViewFacing() const;
	bool GetDesiredFacing(FVector& Forward) const;
	/** Called before gameplay traces, and before their presentation, on authority and owner. */
	void PrepareAttack(bool bMelee);
	void BeginMeleeSwing(const FVector& Forward);
	FVector GetSwingForward() const { return SwingForward; }
private:
	bool CanFaceForCombat() const;
	bool GetViewForward(FVector& Forward) const;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal SwingForward = FVector::ForwardVector;
	double PredictedSwingUntil = -1.0;
};
