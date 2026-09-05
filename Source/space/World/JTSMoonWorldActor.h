#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "JTSMoonWorldActor.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;

/**
 * Runtime configuration and material-parameter driver for the flat, looping Moon world.
 * The actor is intentionally visual-only: it does not provide gravity or collision.
 */
UCLASS()
class SPACE_API AJTSMoonWorldActor : public AActor
{
	GENERATED_BODY()

public:
	AJTSMoonWorldActor();

	UFUNCTION(BlueprintPure, Category = "Moon|Looping Map")
	float GetMapSizeX() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Looping Map")
	float GetMapSizeY() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Looping Map")
	FVector2D GetMapSize2D() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	bool IsWorldBendEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetFlatRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetBendTransitionWidth() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetVisualCurveRadius() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetBendMaxDistance() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetMaximumVisualDisplacement() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Visual Bend")
	float GetRecommendedBoundsScale(float BaseBoundsRadius) const;

	/** Publishes the local player's physical XY and the configured bend values to the optional MPC. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Visual Bend")
	void UpdateBendMaterialParameters();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool IsMoonWorld() const;
	void PublishScalar(class UMaterialParameterCollectionInstance* Instance, FName ParameterName, float Value, float& CachedValue, bool& bHasCachedValue) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Looping Map", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MapSizeX = 24000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Looping Map", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float MapSizeY = 24000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true"))
	bool bEnableWorldBend = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float VisualCurveRadius = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float FlatRadius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float BendTransitionWidth = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", UIMin = "1.0"))
	float BendMaxDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialParameterCollection> BendParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Visual Bend", meta = (AllowPrivateAccess = "true"))
	bool bDriveMaterialParameterCollection = true;

	mutable float CachedBendOriginX = 0.0f;
	mutable float CachedBendOriginY = 0.0f;
	mutable float CachedWorldBendEnabled = -1.0f;
	mutable float CachedFlatRadius = -1.0f;
	mutable float CachedTransitionWidth = -1.0f;
	mutable float CachedCurveRadius = -1.0f;
	mutable float CachedBendMaxDistance = -1.0f;
	mutable bool bHasCachedBendOriginX = false;
	mutable bool bHasCachedBendOriginY = false;
	mutable bool bHasCachedWorldBendEnabled = false;
	mutable bool bHasCachedFlatRadius = false;
	mutable bool bHasCachedTransitionWidth = false;
	mutable bool bHasCachedCurveRadius = false;
	mutable bool bHasCachedBendMaxDistance = false;
	bool bMissingCollectionLogged = false;
};
