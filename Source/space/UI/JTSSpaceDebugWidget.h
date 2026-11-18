#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "JTSSpaceDebugWidget.generated.h"
class UTextBlock;
class UVerticalBox;
class UBorder;
class AJTSSpaceDebugTravelManager;

UCLASS()
class UJTSDebugPlanetButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(class AJTSPlayerController* Controller, FName InPlanetId, bool bDefault);
private:
	UFUNCTION() void Activate();
	TWeakObjectPtr<class AJTSPlayerController> Player;
	FName PlanetId;
	bool bSetDefault = false;
};

UCLASS()
class UJTSDebugPermitButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(class AJTSPlayerController* Controller, FName InPermitId);
private:
	UFUNCTION() void Activate();
	TWeakObjectPtr<class AJTSPlayerController> Player;
	FName PermitId;
};

/** Developer panel; destination configuration belongs to the level actor. */
UCLASS()
class SPACE_API UJTSSpaceDebugWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	bool IsPointerOverPanel(const FVector2D& ScreenPosition) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void BuildPanel(const AJTSSpaceDebugTravelManager& Debug);
	void Refresh();
	UFUNCTION() void ToggleDarkVisibility();
	UFUNCTION() void ToggleInvulnerability();
	UFUNCTION() void ToggleOneHitKill();
	UFUNCTION() void ToggleSidebar();
	UPROPERTY(Transient) TObjectPtr<UBorder> Sidebar;
	UPROPERTY(Transient) TObjectPtr<UButton> SidebarTab;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SidebarTabLabel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Content;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton> DarkVisibilityButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DarkVisibilityLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> InvulnerabilityButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> InvulnerabilityLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> OneHitKillButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OneHitKillLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<UJTSDebugPermitButton>> PermitButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PermitLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UJTSDebugPlanetButton>> Buttons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PlanetLabels;
	TWeakObjectPtr<AJTSSpaceDebugTravelManager> Manager;
	float RefreshElapsed = 1.0f;
	float SidebarAlpha = 0;
	bool bSidebarOpen = false;
};
