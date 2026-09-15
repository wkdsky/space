// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "space/Core/JTSExpeditionTypes.h"

#include "JTSGameInstance.generated.h"

/** Local-only preferences. Shared expedition state lives in UJTSExpeditionSubsystem instead. */
UCLASS()
class SPACE_API UJTSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

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
