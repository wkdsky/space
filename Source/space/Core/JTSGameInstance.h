// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "space/Items/JTSResourceTypes.h"

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

	/** Returns the resource amount stored in the cross-level spacecraft snapshot. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetPersistedSpacecraftResourceAmount(EJTSResourceType ResourceType) const;

	/** Returns whether a spacecraft storage snapshot is available for Moon arrival. */
	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	bool HasPersistedSpacecraftStorage() const;

	/** Replaces the cross-level spacecraft storage snapshot after a successful launch. */
	void SetPersistedSpacecraftStorage(const TMap<EJTSResourceType, int32>& NewStorage);

	/** Returns the cross-level spacecraft storage snapshot. */
	const TMap<EJTSResourceType, int32>& GetPersistedSpacecraftStorage() const;

	/** Clears the completed or failed mission's spacecraft storage before a new Earth run. */
	void ClearPersistedSpacecraftStorage();

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
	static bool IsSupportedResourceType(EJTSResourceType ResourceType);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	EJTSAvatarColor SelectedAvatarColor = EJTSAvatarColor::Blue;

	/** Travel-only snapshot. The active spacecraft owns the live Storage in each level. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TMap<EJTSResourceType, int32> PersistedSpacecraftStorage;

	bool bHasPersistedSpacecraftStorage = false;
};
