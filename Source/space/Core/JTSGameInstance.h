// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "JTSGameInstance.generated.h"

UENUM(BlueprintType)
enum class EJTSAvatarColor : uint8
{
	Blue UMETA(DisplayName = "Blue"),
	Orange UMETA(DisplayName = "Orange"),
	Green UMETA(DisplayName = "Green")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnJTSExpeditionSuppliesChanged, float, Food, float, Water);

/**
 * Native cross-level root for Jump to Space runtime state.
 */
UCLASS()
class SPACE_API UJTSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintPure, Category = "Settings")
	EJTSAvatarColor GetSelectedAvatarColor() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetSelectedAvatarColor(EJTSAvatarColor NewAvatarColor);

	UFUNCTION(BlueprintPure, Category = "Settings")
	FLinearColor GetSelectedAvatarLinearColor() const;

	UFUNCTION(BlueprintPure, Category = "Expedition|Supplies")
	float GetExpeditionFood() const;

	UFUNCTION(BlueprintPure, Category = "Expedition|Supplies")
	float GetExpeditionWater() const;

	UFUNCTION(BlueprintCallable, Category = "Expedition|Supplies")
	void SetExpeditionSupplies(float NewFood, float NewWater);

	UFUNCTION(BlueprintCallable, Category = "Expedition|Supplies")
	void ConsumeExpeditionSupplies(float FoodAmount, float WaterAmount);

	UFUNCTION(BlueprintPure, Category = "Expedition|Supplies")
	bool IsFoodDepleted() const;

	UFUNCTION(BlueprintPure, Category = "Expedition|Supplies")
	bool IsWaterDepleted() const;

	UPROPERTY(BlueprintAssignable, Category = "Expedition|Supplies")
	FOnJTSExpeditionSuppliesChanged OnExpeditionSuppliesChanged;

private:
	static float NormalizeExpeditionResource(float Value);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	EJTSAvatarColor SelectedAvatarColor = EJTSAvatarColor::Blue;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition|Supplies", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ExpeditionFood = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Expedition|Supplies", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ExpeditionWater = 0.0f;
};
