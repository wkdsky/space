// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "space/Items/JTSShipLockerTypes.h"
#include "JTSStellarAttachmentDialog.generated.h"

class AJTSSpacecraftActor;
class UJTSStellarLoadoutComponent;
class UJTSStellarLootTable;
class SWidget;

/**
 * Compact permanent core upgrades, or point allocation for a matched attachment. The server owns every
 * mutation; this widget only submits intent and re-reads the replicated item and its current pairing.
 */
UCLASS()
class SPACE_API UJTSStellarAttachmentDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the dialog for a locker slot and returns false when the slot cannot be inspected. */
	UFUNCTION(BlueprintCallable, Category="UI|Stellar")
	bool OpenForSlot(AJTSSpacecraftActor* Spacecraft, int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="UI|Stellar")
	bool OpenForLoadoutSlot(int32 InSlotIndex);
	UFUNCTION(BlueprintCallable, Category="UI|Stellar")
	void CloseDialog();
	UFUNCTION(BlueprintPure, Category="UI|Stellar")
	bool IsDialogOpen() const { return bOpen; }
	void NotifyProgressionResult(bool bUpgrade, bool bSucceeded);

	/** Fired after the dialog removes itself, so the host can take keyboard focus back. */
	FSimpleDelegate OnDialogClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UJTSStellarLoadoutComponent* Loadout() const;
	const UJTSStellarLootTable* LootTable() const;
	bool IsEditable() const;
	bool CanInspect(const FJTSShipLockerSlot& Slot) const;
	FJTSShipLockerSlot ReadSlot() const;
	/** True while the slot still holds the item the dialog was opened for. */
	bool IsSlotStillValid(const FJTSShipLockerSlot& Slot) const;
	int32 GetBudget(const FJTSShipLockerSlot& Slot) const;
	bool IsMatchedWeapon(const FJTSShipLockerSlot& Slot) const;
	bool HasAttachment(const FJTSShipLockerSlot& Slot) const;
	bool HasCore(const FJTSShipLockerSlot& Slot) const;
	TArray<uint8> GetRecordedPoints(const FJTSShipLockerSlot& Slot) const;
	bool IsDraftDirty(const FJTSShipLockerSlot& Slot) const;
	FText BuildTitle(const FJTSShipLockerSlot& Slot) const;
	FText BuildDescription(const FJTSShipLockerSlot& Slot) const;
	FText BuildBudgetText(const FJTSShipLockerSlot& Slot) const;
	FText BuildSkillName(int32 SkillIndex, const FJTSShipLockerSlot& Slot) const;
	FText BuildSkillEffect(int32 SkillIndex, const FJTSShipLockerSlot& Slot) const;
	FText BuildSkillLevels(int32 SkillIndex, const FJTSShipLockerSlot& Slot) const;
	FText BuildCoreText(const FJTSShipLockerSlot& Slot) const;
	int32 GetAvailableFirmwareUnits() const;
	bool CanUpgradeCore(const FJTSShipLockerSlot& Slot) const;
	bool CanChangeSkill(int32 SkillIndex, int32 Delta, const FJTSShipLockerSlot& Slot) const;
	void ChangeSkill(int32 SkillIndex, int32 Delta);
	void ResetDraft();
	void ApplyDraft();
	void UpgradeCore();
	void SetStatus(const FString& Message, bool bError);

	TWeakObjectPtr<AJTSSpacecraftActor> ActiveSpacecraft;
	int32 SlotIndex = INDEX_NONE;
	FGuid OpenedToken;
	TArray<uint8> Draft;
	FString StatusMessage;
	bool bStatusIsError = false;
	bool bOpen = false;
	bool bLoadoutContext = false;
	bool bAwaitingAction = false;
	int32 OpenedRevision = INDEX_NONE;
	int32 ObservedActionResponse = 0;
};
