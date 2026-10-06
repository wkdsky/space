// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Items/JTSShipLockerTypes.h"
#include "JTSWeaponUpgradePanel.generated.h"

class AJTSSpacecraftActor;
class SWidget;

/**
 * Left half of the shop's weapon-upgrade page. It shows a prompt until a weapon in the ship locker (right half) is
 * clicked, then lists the six skills of that weapon. Every point change is sent to the server immediately and the
 * locker slot is the source of truth: the panel only keeps an optimistic draft while requests are in flight.
 */
UCLASS()
class SPACE_API UJTSWeaponUpgradePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Selects a locker slot for upgrading and returns false when it holds no upgradeable weapon. */
	bool SelectSlot(AJTSSpacecraftActor* Spacecraft, int32 InSlotIndex);
	void ClearSelection();
	int32 GetSelectedSlot() const { return SlotIndex; }
	void NotifyUpgradeResult(bool bUpgrade, bool bSucceeded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FJTSShipLockerSlot ReadSlot() const;
	bool HasValidSelection() const;
	bool IsSlotStillValid(const FJTSShipLockerSlot& LockerSlot) const;
	TArray<uint8> GetRecordedPoints(const FJTSShipLockerSlot& LockerSlot) const;
	FText BuildTitle(const FJTSShipLockerSlot& LockerSlot) const;
	FText BuildBudgetText(const FJTSShipLockerSlot& LockerSlot) const;
	FText BuildCostText(const FJTSShipLockerSlot& LockerSlot) const;
	FText BuildSkillName(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const;
	FText BuildSkillEffect(int32 SkillIndex, const FJTSShipLockerSlot& LockerSlot) const;
	bool CanChangeSkill(int32 SkillIndex, int32 Delta, const FJTSShipLockerSlot& LockerSlot) const;
	bool CanAffordUpgrade(const FJTSShipLockerSlot& LockerSlot) const;
	void ChangeSkill(int32 SkillIndex, int32 Delta);
	void ResetPoints();
	void SubmitDraft();
	void UpgradeBody();
	void SetStatus(const FString& Message, bool bError);

	TWeakObjectPtr<AJTSSpacecraftActor> ActiveSpacecraft;
	int32 SlotIndex = INDEX_NONE;
	FGuid SelectedToken;
	TArray<uint8> Draft;
	/** Point submissions the server has not answered yet; the draft is only re-synced when this is zero. */
	int32 PendingPointRequests = 0;
	FString StatusMessage;
	bool bStatusIsError = false;
};
