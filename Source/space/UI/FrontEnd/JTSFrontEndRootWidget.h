// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "JTSFrontEndRootWidget.generated.h"

class UJTSExpeditionSelectWidget;
class UJTSNewExpeditionWidget;
class UJTSJoinExpeditionWidget;
class UJTSMainMenuWidget;
class UJTSSettingsWidget;
class UJTSCreditsWidget;
class UCanvasPanel;
class UWidgetSwitcher;

/** Owns front-end page navigation; child widgets remain independently reusable. */
UCLASS(Config = Game)
class SPACE_API UJTSFrontEndRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Return reusable child pages to the landing page without hard-coding page ownership into them. */
	void ShowMainMenu();
	void ShowExpeditionSelect();
	void ShowNewExpedition(int32 SaveSlot);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildWidgetTree();
	void ShowPage(FName PageName);
	TSubclassOf<UJTSMainMenuWidget> ResolveMainMenuClass() const;
	TSubclassOf<UJTSExpeditionSelectWidget> ResolveExpeditionSelectClass() const;
	TSubclassOf<UJTSNewExpeditionWidget> ResolveNewExpeditionClass() const;
	TSubclassOf<UJTSJoinExpeditionWidget> ResolveJoinClass() const;
	TSubclassOf<UJTSSettingsWidget> ResolveSettingsClass() const;
	TSubclassOf<UJTSCreditsWidget> ResolveCreditsClass() const;

	/** Editor-owned composition overrides. Empty values retain the native safe fallback. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSMainMenuWidget> MainMenuWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSExpeditionSelectWidget> ExpeditionSelectWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSNewExpeditionWidget> NewExpeditionWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSJoinExpeditionWidget> JoinExpeditionWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSSettingsWidget> SettingsWidgetClass;
	UPROPERTY(EditDefaultsOnly, Config, Category = "UI") TSoftClassPtr<UJTSCreditsWidget> CreditsWidgetClass;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> PageSwitcher;
	UPROPERTY(Transient) TObjectPtr<UJTSMainMenuWidget> MainMenu;
	UPROPERTY(Transient) TObjectPtr<UJTSExpeditionSelectWidget> ExpeditionSelectPage;
	UPROPERTY(Transient) TObjectPtr<UJTSNewExpeditionWidget> NewExpeditionPage;
	UPROPERTY(Transient) TObjectPtr<UJTSJoinExpeditionWidget> JoinPage;
	UPROPERTY(Transient) TObjectPtr<UJTSSettingsWidget> SettingsPage;
	UPROPERTY(Transient) TObjectPtr<UJTSCreditsWidget> CreditsPage;
};
