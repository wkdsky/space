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

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	EJTSAvatarColor SelectedAvatarColor = EJTSAvatarColor::Blue;
};
