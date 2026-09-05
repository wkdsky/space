#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "JTSMoonWrapSubsystem.generated.h"

/** Centralized 2D toroidal coordinate math for the flat Moon gameplay sheet. */
UCLASS()
class SPACE_API UJTSMoonWrapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D GetMapSize2D() const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D CanonicalizePosition2D(const FVector2D& Position) const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D ShortestWrappedDelta2D(const FVector2D& From, const FVector2D& To) const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	float WrappedDistance2D(const FVector2D& From, const FVector2D& To) const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D GetLogicalPositionFromWorld(const FVector& WorldPosition) const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D GetNearestPhysicalImage(const FVector2D& PlayerPhysicalXY, const FVector2D& ActorLogicalXY) const;

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	bool IsConfiguredForMoon() const;

	void RefreshConfiguration() const;

private:
	const class AJTSMoonWorldActor* FindConfigurationActor() const;
	static float CanonicalizeAxis(float Value, float Size);
	static float ShortestWrappedAxisDelta(float From, float To, float Size);

	mutable TWeakObjectPtr<const class AJTSMoonWorldActor> ConfigurationActor;
	mutable FVector2D CachedMapSize = FVector2D(24000.0f, 24000.0f);
};
