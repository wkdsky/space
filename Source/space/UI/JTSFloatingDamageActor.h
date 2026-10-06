#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSFloatingDamageActor.generated.h"

class UWidgetComponent;

/** Local-only short-lived world anchor for floating damage; never replicated. */
UCLASS(NotBlueprintable)
class SPACE_API AJTSFloatingDamageActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSFloatingDamageActor();
	void Initialize(float Damage, bool bCritical, const FVector& SurfaceUp);
	/** Accumulate rapid damage without extending the popup's fixed lifetime. */
	void AddDamage(float Damage, bool bCritical);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent> WidgetComponent;
	FVector StartLocation = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	FVector Side = FVector::RightVector;
	float Age = 0.0f;
	float TotalDamage = 0.0f;
	bool bIsCritical = false;
};
