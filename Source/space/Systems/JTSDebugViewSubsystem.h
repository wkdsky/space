#pragma once

#include "CoreMinimal.h"
#include "ShowFlags.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "JTSDebugViewSubsystem.generated.h"

class UGameViewportClient;

/** Local developer rendering only; never changes world lighting or replicated gameplay. */
UCLASS()
class SPACE_API UJTSDebugViewSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Debug|View")
	bool CanUseDarkVisibility() const;

	UFUNCTION(BlueprintPure, Category = "Debug|View")
	bool IsDarkVisibilityEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Debug|View")
	bool SetDarkVisibilityEnabled(bool bEnabled);

private:
	void RestoreView();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	FDelegateHandle WorldCleanupHandle;
	TWeakObjectPtr<UGameViewportClient> ModifiedViewport;
	TWeakObjectPtr<UWorld> ModifiedWorld;
	FEngineShowFlags SavedShowFlags{ESFIM_Game};
	int32 SavedViewMode = 0;
	bool bHasSavedView = false;
};
